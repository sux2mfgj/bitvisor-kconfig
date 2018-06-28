/*
 * Copyright (c) 2017 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file	drivers/nvme/nvme_subm_handler.c
 * @brief	NVMe submission queue handling
 * @author	Ake Koomsin
 */

#include <core.h>

#include "nvme.h"
#include "nvme_io.h"
#include "nvme_subm_handler.h"

static void
handle_delete_queue (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_cmd *cmd = &req->cmd;

	u16 queue_id = QUEUE_GET_QID (cmd);

	u8 type = cmd->opcode;

	ASSERT (type == NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE ||
		type == NVME_ADMIN_OPCODE_DELETE_COMP_QUEUE);

	host->io_ready = 0;

	if (type == NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE)
		nvme_free_subm_queue_info (host, queue_id);
	else
		nvme_free_comp_queue_info (host, queue_id);
}

static void
create_queue (struct nvme_queue_info *h_queue_info,
	      struct nvme_queue_info *g_queue_info,
	      uint page_nbytes,
	      u16 h_queue_n_entries,
	      u16 g_queue_n_entries,
	      uint entry_nbytes,
	      phys_t g_queue_phys,
	      uint map_flag)
{
	h_queue_info->n_entries	   = h_queue_n_entries;
	h_queue_info->entry_nbytes = entry_nbytes;

	g_queue_info->n_entries	   = g_queue_n_entries;
	g_queue_info->entry_nbytes = entry_nbytes;

	uint h_nbytes = h_queue_n_entries  * entry_nbytes;
	h_nbytes = (h_nbytes < page_nbytes) ? page_nbytes : h_nbytes;

	uint g_nbytes = g_queue_n_entries * entry_nbytes;
	g_nbytes = (g_nbytes < page_nbytes) ? page_nbytes : g_nbytes;

	phys_t h_queue_phys;
	h_queue_info->queue.ptr = zalloc2 (h_nbytes, &h_queue_phys);
	h_queue_info->queue_phys = h_queue_phys;

	g_queue_info->queue_phys = g_queue_phys;
	g_queue_info->queue.ptr = mapmem_gphys (g_queue_phys,
						g_nbytes,
						map_flag);

	h_queue_info->phase = NVME_INITIAL_PHASE;
	g_queue_info->phase = NVME_INITIAL_PHASE;

	spinlock_init (&h_queue_info->lock);

	dprintf (NVME_SUBM_DEBUG, "Guest addr: 0x%016llX\n", g_queue_phys);
	dprintf (NVME_SUBM_DEBUG, "Host  addr: 0x%016llX\n", h_queue_phys);
}

static void
handle_create_queue (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_cmd *cmd = &req->cmd;

	if (host->h_queue.max_n_subm_queues == 0) {
		dprintf (1, "Warning: SET_FEATURE for n_queues not found, ");
		dprintf (1, "Assume it is 1\n");
		nvme_set_max_n_queues (host, 1, 1);
	}

	/* Currently support only physical continuous */
	ASSERT (QUEUE_GET_PC (cmd));

	ASSERT (host->h_queue.comp_queue_info);
	ASSERT (host->g_queue.comp_queue_info);

	u8 type = cmd->opcode;

	ASSERT (type == NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE ||
		type == NVME_ADMIN_OPCODE_CREATE_COMP_QUEUE);

	u16 queue_id = QUEUE_GET_QID (cmd);
	u16 g_queue_n_entries = QUEUE_GET_QSIZE (cmd) + 1; /* 0 based */
	u16 h_queue_n_entries = g_queue_n_entries;

	if (host->io_interceptor &&
	    host->io_interceptor->get_io_entries) {
		void *interceptor = host->io_interceptor->interceptor;

		u16 (*get_io_entries) (void *interceptor,
				       u16 g_n_entries,
				       u16 max_n_entries);
		get_io_entries = host->io_interceptor->get_io_entries;

		h_queue_n_entries = get_io_entries (interceptor,
						    g_queue_n_entries,
				    		    host->max_n_entries);

		cmd->cmd_flags[0] = (h_queue_n_entries - 1) << 16 | queue_id;
	}


	dprintf (NVME_SUBM_DEBUG, "Queue ID: %u\n", queue_id);
	dprintf (NVME_SUBM_DEBUG, "Host Queue #entries: %u\n",
		 h_queue_n_entries);
	dprintf (NVME_SUBM_DEBUG, "Guest Queue #entries: %u\n",
		 g_queue_n_entries);

	struct nvme_queue *h_queue = &host->h_queue;
	struct nvme_queue *g_queue = &host->g_queue;

	if (type == NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE) {
		ASSERT (!h_queue->subm_queue_info[queue_id]);
		ASSERT (!g_queue->subm_queue_info[queue_id]);

		struct nvme_queue_info *h_subm_queue_info, *g_subm_queue_info;
		h_subm_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);
		spinlock_init (&h_subm_queue_info->lock);
		g_subm_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);

		u16 comp_queue_id = QUEUE_GET_COMP_QID (cmd);

		struct nvme_queue_info *g_comp_queue_info;
		g_comp_queue_info = g_queue->comp_queue_info[comp_queue_id];

		create_queue (h_subm_queue_info,
			      g_subm_queue_info,
			      host->page_nbytes,
			      h_queue_n_entries,
			      g_queue_n_entries,
			      host->io_subm_entry_nbytes,
			      NVME_CMD_PRP_PTR1 (cmd),
			      0);

		NVME_CMD_PRP_PTR1 (cmd) = h_subm_queue_info->queue_phys;

		struct nvme_request_hub *req_hub;
		req_hub = zalloc (NVME_REQUEST_HUB_NBYTES);
		spinlock_init (&req_hub->lock);

		uint nbytes;
		nbytes = h_queue_n_entries * sizeof (struct nvme_request *);
		req_hub->req_slot = zalloc (nbytes);
		req_hub->h_subm_queue_info = h_subm_queue_info;
		req_hub->g_comp_queue_info = g_comp_queue_info;

		h_queue->request_hub[queue_id] = req_hub;
		h_queue->subm_queue_info[queue_id] = h_subm_queue_info;
		g_queue->subm_queue_info[queue_id] = g_subm_queue_info;
	} else {
		ASSERT (!h_queue->comp_queue_info[queue_id]);
		ASSERT (!g_queue->comp_queue_info[queue_id]);

		struct nvme_queue_info *h_comp_queue_info, *g_comp_queue_info;
		h_comp_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);
		spinlock_init (&h_comp_queue_info->lock);
		g_comp_queue_info = zalloc (NVME_QUEUE_INFO_NBYTES);

		create_queue (h_comp_queue_info,
			      g_comp_queue_info,
			      host->page_nbytes,
			      h_queue_n_entries,
			      g_queue_n_entries,
			      host->io_comp_entry_nbytes,
			      NVME_CMD_PRP_PTR1 (cmd),
			      MAPMEM_WRITE);

		NVME_CMD_PRP_PTR1 (cmd) = h_comp_queue_info->queue_phys;

		h_queue->comp_queue_info[queue_id] = h_comp_queue_info;
		g_queue->comp_queue_info[queue_id] = g_comp_queue_info;
	}
}

static void
handle_identify (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_cmd *cmd = &req->cmd;

	u8 tx_type = NVME_CMD_GET_TX_TYPE (cmd);

	if (tx_type != NVME_CMD_TX_TYPE_PRP) {
		dprintf (NVME_SUBM_DEBUG,
			 "Transfer type: %u\n", tx_type);
		dprintf (NVME_SUBM_DEBUG,
			 "Ignore intercepting identify command");
		return;
	}

	if (!NVME_CMD_PRP_PTR1 (cmd)) {
		dprintf (NVME_SUBM_DEBUG,
			 "No guest buffer for identify command!\n");
		dprintf (NVME_SUBM_DEBUG,
			 "This should not be possible, ignore intercepting\n");
		return;
	}

	req->h_buf = alloc2_align (host->page_nbytes,
				   &req->buf_phys,
				   host->page_nbytes);
	NVME_CMD_PRP_PTR1 (cmd) = req->buf_phys;
}

static void
admin_cmd_handler (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_queue_info *admin_subm_queue_info;
	admin_subm_queue_info = host->h_queue.subm_queue_info[0];

	switch (req->cmd.opcode) {
	case NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_DELETE_SUBM_QUEUE));
		handle_delete_queue (host, req);
		break;
	case NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_CREATE_SUBM_QUEUE));
		handle_create_queue (host, req);
		break;
	case NVME_ADMIN_OPCODE_GET_LOG_PAGE:
		dprintf (NVME_SUBM_DEBUG_OPT, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_GET_LOG_PAGE));
		break;
	case NVME_ADMIN_OPCODE_DELETE_COMP_QUEUE:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_DELETE_COMP_QUEUE));
		handle_delete_queue (host, req);
		break;
	case NVME_ADMIN_OPCODE_CREATE_COMP_QUEUE:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_CREATE_COMP_QUEUE));
		handle_create_queue (host, req);
		break;
	case NVME_ADMIN_OPCODE_IDENTIFY:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_IDENTIFY));
		handle_identify (host, req);
		break;
	case NVME_ADMIN_OPCODE_ABORT:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_ABORT));
		/*
		 * Patch Abort command to avoid canceling requests from
		 * the interceptor.
		 *
		 * XXX: Is there a better way?
		 */
		if (host->io_interceptor) {
			uint n_entries;
			n_entries = admin_subm_queue_info->n_entries;
			u16 *cmd_flag0_word = (u16 *)&req->cmd.cmd_flags[0];
			cmd_flag0_word[1] = n_entries + 1;
			dprintf (NVME_SUBM_DEBUG, "Patch Abort command\n");
		}
		break;
	case NVME_ADMIN_OPCODE_SET_FEATURE:
		dprintf (NVME_SUBM_DEBUG_OPT, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_SET_FEATURE));
		break;
	case NVME_ADMIN_OPCODE_GET_FEATURE:
		dprintf (NVME_SUBM_DEBUG_OPT, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_GET_FEATURE));
		break;
	case NVME_ADMIN_OPCODE_ASYNC_EV_REQ:
		host->h_queue.request_hub[0]->n_async_g_reqs++;
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_ASYNC_EV_REQ));
		break;
	case NVME_ADMIN_OPCODE_NS_MANAGEMENT:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_NS_MANAGEMENT));
		break;
	case NVME_ADMIN_OPCODE_FW_COMMIT:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_FW_COMMIT));
		break;
	case NVME_ADMIN_OPCODE_FW_IMG_DL:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_FW_IMG_DL));
		break;
	case NVME_ADMIN_OPCODE_NS_ATTACHMENT:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_NS_ATTACHMENT));
		break;
	case NVME_ADMIN_OPCODE_KEEP_ALIVE:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_KEEP_ALIVE));
		break;
	case NVME_ADMIN_OPCODE_FORMAT_NVM:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_FORMAT_NVM));
		break;
	case NVME_ADMIN_OPCODE_SECURITY_SEND:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_SECURITY_SEND));
		break;
	case NVME_ADMIN_OPCODE_SECURITY_RECV:
		dprintf (NVME_SUBM_DEBUG, "Admin opcode: %s found\n",
			 STR (NVME_ADMIN_OPCODE_SECURITY_RECV));
		break;
	default:
		dprintf (NVME_SUBM_DEBUG,
			 "Unknown admin opcode: 0x%X\n", req->cmd.opcode);
		break;
	}
}

static void
call_interceptor (struct nvme_host *host,
		  struct nvme_request *req,
		  u8 cmd_opcode)
{
	if (!host->io_interceptor)
		return;

	struct nvme_io_interceptor *io_interceptor;
	io_interceptor = host->io_interceptor;

	if (io_interceptor->on_read &&
	    cmd_opcode == NVME_IO_OPCODE_READ) {
		io_interceptor->on_read (io_interceptor->interceptor,
					 req,
					 req->cmd.nsid,
					 req->lba_start,
					 req->n_lbas);
	} else if (io_interceptor->on_write &&
		   cmd_opcode == NVME_IO_OPCODE_WRITE) {
		io_interceptor->on_write (io_interceptor->interceptor,
					  req,
					  req->cmd.nsid,
					  req->lba_start,
					  req->n_lbas);
	} else if (io_interceptor->on_compare &&
		   cmd_opcode == NVME_IO_OPCODE_COMPARE) {
		io_interceptor->on_compare (io_interceptor->interceptor,
					    req,
					    req->cmd.nsid,
					    req->lba_start,
					    req->n_lbas);
	}
}

static void
handle_rw (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_cmd *g_io_cmd = &req->cmd;

	ASSERT (g_io_cmd->nsid != 0);

	struct nvme_ns_meta *ns_meta = &host->ns_metas[g_io_cmd->nsid];

	req->lba_start = NVME_IO_CMD_GET_LBA_START (g_io_cmd);

	/* + 1 because it is zero based */
	req->n_lbas    = NVME_IO_CMD_GET_N_LBAS (g_io_cmd) + 1;

	req->total_nbytes = req->n_lbas * ns_meta->lba_nbytes;

	call_interceptor (host, req, g_io_cmd->opcode);
}

#define TYPE_DEALLOC (0x4)
static void
handle_data_management (struct nvme_host *host, struct nvme_request *req)
{
	struct nvme_cmd *g_io_cmd = &req->cmd;

	struct nvme_io_interceptor *io_interceptor;
	io_interceptor = host->io_interceptor;

	u32 type = req->cmd.cmd_flags[1];

	if (!io_interceptor ||
	    !io_interceptor->on_data_management ||
	    type != TYPE_DEALLOC)
		return;

	void *interceptor = io_interceptor->interceptor;

	req->h_buf = alloc2 (host->page_nbytes, &req->buf_phys);

	void *g_buf = mapmem_gphys (NVME_CMD_PRP_PTR1 (g_io_cmd),
				    host->page_nbytes,
				    0);

	memcpy (req->h_buf, g_buf, host->page_nbytes);

	unmapmem (g_buf, host->page_nbytes);

	NVME_CMD_PRP_PTR1 (&req->cmd) = req->buf_phys;

	u32 orig_range = req->cmd.cmd_flags[0];

	u32 new_range = io_interceptor->on_data_management (interceptor,
							    req,
							    g_io_cmd->nsid,
							    req->h_buf,
							    host->page_nbytes,
							    orig_range);

	req->cmd.cmd_flags[0] = new_range;
}
#undef TYPE_DEALLOC

static void
io_cmd_handler (struct nvme_host *host,
		struct nvme_request *req)
{
#if 0
	/* Debugging strange commands from Apple firmware */
	if (req->cmd.flags & 0x3C) {
		struct nvme_cmd *g_io_cmd = &req->cmd;
		dprintf (1, "Interesting flags: 0x%x\n", g_io_cmd->flags);
		dprintf (1, "opcode: %u\n", g_io_cmd->opcode);
		dprintf (1, "TX Type: %u\n", NVME_CMD_GET_TX_TYPE (g_io_cmd));
		dprintf (1, "NS ID: %u\n", g_io_cmd->nsid);
		dprintf (1, "meta ptr: 0x%016llX\n", g_io_cmd->meta_ptr);
		dprintf (1, "ptr1: 0x%016llX\n",
			 g_io_cmd->data_ptr.prp_entry.ptr1);
		dprintf (1, "ptr2: 0x%016llX\n",
			 g_io_cmd->data_ptr.prp_entry.ptr2);

		dprintf (1, "cmd_flags0: 0x%08X\n", g_io_cmd->cmd_flags[0]);
		dprintf (1, "cmd_flags1: 0x%08X\n", g_io_cmd->cmd_flags[1]);
		dprintf (1, "cmd_flags2: 0x%08X\n", g_io_cmd->cmd_flags[2]);
		dprintf (1, "cmd_flags3: 0x%08X\n", g_io_cmd->cmd_flags[3]);
		dprintf (1, "cmd_flags4: 0x%08X\n", g_io_cmd->cmd_flags[4]);
		dprintf (1, "cmd_flags5: 0x%08X\n", g_io_cmd->cmd_flags[5]);
	}
#endif
	switch (req->cmd.opcode) {
	case NVME_IO_OPCODE_READ:
	case NVME_IO_OPCODE_WRITE:
	case NVME_IO_OPCODE_COMPARE:
		handle_rw (host, req);
		break;
	case NVME_IO_OPCODE_DATASET_MANAGEMENT:
		handle_data_management (host, req);
		break;
	default:
		break;
	}
}

/*
 * XXX: How should we check for the limit properly?
 * If the guest decides to uses all queues the device can provide and decides
 * to submit lots of commands to every queue at once, how can we calculate
 * the fetch limit rate properly so that BitVisor does not run out of memory?
 */
static uint
get_fetching_limit (struct nvme_host *host,
		    struct nvme_request_hub *hub,
		    u16 queue_id)
{
	if (!host->io_interceptor ||
	    !host->io_interceptor->get_fetching_limit ||
	    queue_id == 0)
		return 32;

	uint n_waiting_g_reqs = hub->n_waiting_g_reqs;

	void *interceptor = host->io_interceptor->interceptor;
	return host->io_interceptor->get_fetching_limit (interceptor,
							 n_waiting_g_reqs);
}

static struct nvme_request *
request_from_g_cmd (struct nvme_cmd *g_cmd)
{
	struct nvme_request *req = zalloc (NVME_REQUEST_NBYTES);
	req->cmd	 = *g_cmd;
	req->orig_cmd_id = g_cmd->cmd_id;
	req->g_data_ptr  = req->cmd.data_ptr;
	return req;
}

static uint
fetch_requests_from_guest (struct nvme_host *host, u16 queue_id)
{
	struct nvme_queue_info *g_subm_queue_info;
	struct nvme_request_hub *req_hub;
	uint n_fetchable;
	uint count = 0;

	g_subm_queue_info = host->g_queue.subm_queue_info[queue_id];
	req_hub		  = host->h_queue.request_hub[queue_id];
	n_fetchable	  = get_fetching_limit (host, req_hub, queue_id);

	u16 g_cur_tail = g_subm_queue_info->cur_pos.tail;
	u16 g_new_tail = g_subm_queue_info->new_pos.tail;

	uint n_entries = g_subm_queue_info->n_entries;

	while (g_cur_tail != g_new_tail &&
	       n_fetchable != 0) {
		struct nvme_cmd *g_cmd;
		g_cmd = &g_subm_queue_info->queue.subm[g_cur_tail];

		struct nvme_request *req;
		req = request_from_g_cmd (g_cmd);
		req->queue_id = queue_id;

		if (queue_id == 0)
		      admin_cmd_handler (host, req);
		else
		      io_cmd_handler (host, req);

		nvme_register_request (req_hub, req);

		count++;
		g_cur_tail++;
		g_cur_tail %= n_entries;

		n_fetchable--;
	}

	g_subm_queue_info->cur_pos.tail = g_cur_tail;

	return count;
}

static void
try_fetch_req_lock (struct nvme_host *host)
{
	if (host->serialize_queue_fetch)
		spinlock_lock (&host->fetch_req_lock);
}

static void
try_fetch_req_unlock (struct nvme_host *host)
{
	if (host->serialize_queue_fetch)
		spinlock_unlock (&host->fetch_req_lock);
}

uint
nvme_try_process_requests (struct nvme_host *host, u16 queue_id)
{
	uint fetched = 0;
	if (!host->pause_fetching_g_reqs) {
		try_fetch_req_lock (host);
		fetched = fetch_requests_from_guest (host, queue_id);
		try_fetch_req_unlock (host);
	}

	nvme_submit_queuing_requests (host, queue_id);
	return fetched;
}
