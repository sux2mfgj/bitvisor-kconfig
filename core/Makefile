# config macros
CONSTANTS-$(CONFIG_64) +=
CONSTANTS-$(CONFIG_DEBUG_GDB) += -DDEBUG_GDB
CONSTANTS-$(CONFIG_SPINLOCK_DEBUG) += -DSPINLOCK_DEBUG
CONSTANTS-$(CONFIG_TTY_SERIAL) += -DTTY_SERIAL
CONSTANTS-$(CONFIG_CPU_MMU_SPT_1) += -DCPU_MMU_SPT_1
CONSTANTS-$(CONFIG_CPU_MMU_SPT_2) += -DCPU_MMU_SPT_2
CONSTANTS-$(CONFIG_CPU_MMU_SPT_3) += -DCPU_MMU_SPT_3
CONSTANTS-$(CONFIG_CPU_MMU_SPT_USE_PAE) += -DCPU_MMU_SPT_USE_PAE
CONSTANTS-$(CONFIG_CPU_MMU_SPT_DISABLE) += -DCPU_MMU_SPT_DISABLE
CONSTANTS-$(CONFIG_PS2KBD_F11PANIC) += -DF11PANIC
CONSTANTS-$(CONFIG_PS2KBD_F12MSG) += -DF12MSG
CONSTANTS-$(CONFIG_AUTO_REBOOT) += -DAUTO_REBOOT
CONSTANTS-$(CONFIG_DBGSH) += -DDBGSH
CONSTANTS-$(CONFIG_STATUS) += -DSTATUS
CONSTANTS-$(CONFIG_LOG_TO_GUEST) += -DLOG_TO_GUEST
CONSTANTS-$(CONFIG_CRYPTO) += -DCRYPTO
CONSTANTS-$(CONFIG_VPN) += -DVPN
CONSTANTS-$(CONFIG_PS2KBD_F10USB) += -DF10USBTEST
CONSTANTS-$(CONFIG_PS2KBD_F12USB) += -DF12DUMPEHCI
CONSTANTS-$(CONFIG_ACPI_DSDT) += -DACPI_DSDT
CONSTANTS-$(CONFIG_DISABLE_SLEEP) += -DDISABLE_SLEEP
CONSTANTS-$(CONFIG_ENABLE_ASSERT) += -DENABLE_ASSERT
CONSTANTS-$(CONFIG_CARDSTATUS) += -DCARDSTATUS
CONSTANTS-$(CONFIG_IDMAN) += -DIDMAN
CONSTANTS-$(CONFIG_VPN_VE) += -DVPN_VE
CONSTANTS-$(CONFIG_VTD_TRANS) += -DVTD_TRANS
CONSTANTS-$(CONFIG_DISABLE_TCG_BIOS) += -DDISABLE_TCG_BIOS
CONSTANTS-$(CONFIG_ACPI_TIME_SOURCE) += -DACPI_TIME_SOURCE
CONSTANTS-$(CONFIG_TCG_BIOS) += -DTCG_BIOS
CONSTANTS-$(CONFIG_BACKTRACE) += -DBACKTRACE
CONSTANTS-$(CONFIG_TTY_VGA) += -DTTY_VGA
CONSTANTS-$(CONFIG_SHIFT_KEY_DEBUG) += -DSHIFT_KEY_DEBUG
CONSTANTS-$(CONFIG_THREAD_1CPU) += -DTHREAD_1CPU
CONSTANTS-$(CONFIG_ACPI_IGNORE_ERROR) += -DACPI_IGNORE_ERROR
CONSTANTS-$(CONFIG_MAP_UEFI_MMIO) += -DMAP_UEFI_MMIO
CONSTANTS-$(CONFIG_DISABLE_VTD) += -DDISABLE_VTD
CONSTANTS-$(CONFIG_DMAR_PASS_THROUGH) += -DDMAR_PASS_THROUGH
CONSTANTS-$(CONFIG_USE_SYSCALL64) += -DUSE_SYSCALL64

CONSTANTS-y += -DUSE_PAE

CFLAGS += -Ivpn/lib
CFLAGS += -Iedk/Foundation/Efi/Include/ -Iedk/Foundation/Framework/Include/
CFLAGS += -Iedk/Foundation/Include/ -Iedk/Foundation/Efi/

objs-y += acpi.o acpi_dsdt.o ap.o assert.o beep.o cache.o callrealmode.o
objs-y += calluefi.o config.o cpu.o cpu_emul.o cpu_interpreter.o cpu_mmu.o
objs-y += cpu_mmu_spt.o cpu_seg.o cpu_stack.o cpuid.o cpuid_pass.o current.o
objs-y += debug.o exint_pass.o gmm_access.o gmm_pass.o i386-stub.o iccard.o
objs-y += initfunc.o int.o io_io.o io_iohook.o io_iopass.o keyboard.o
objs-y += loadbootsector.o localapic.o main.o mm.o mmio.o msg.o msr.o
objs-y += msr_pass.o nmi_pass.o osloader.o panic.o pcpu.o printf.o process.o
objs-y += putchar.o random.o reboot.o savemsr.o seg.o serial.o sleep.o
objs-y += strtol.o svm.o svm_exitcode.o svm_init.o svm_io.o svm_main.o
objs-y += svm_msr.o svm_np.o svm_paging.o svm_panic.o svm_regs.o
objs-y += initipi_pass.o tcg.o thread.o time.o timer.o tty.o uefi.o vcpu.o
objs-y += vga.o vmmcall.o vmmcall_boot.o vmmcall_dbgsh.o vmmcall_iccard.o
objs-y += vmmcall_log.o vmmcall_status.o vpn_ve.o vramwrite.o vt.o vt_ept.o
objs-y += vt_exitreason.o vt_init.o vt_io.o vt_main.o vt_msr.o vt_paging.o
objs-y += vt_panic.o vt_regs.o wakeup.o xsetbv.o xsetbv_pass.o
objs-y += arith.o asm.o callrealmode_asm.o calluefi_asm.o entry.o
objs-y += guest_bioshook.o int_handler.o process_sysenter.o string.o
objs-y += sx_handler.o thread_switch.o wakeup_entry.o uefiutil.o
objs-y += uefi_param_ext.o nmi_handler.o nmi.o initipi.o
objs-y += vt_shadow_vt.o
