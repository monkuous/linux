# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the RISC-V Linux kernel
#

CFLAGS_syscall_table.o	+= $(call cc-disable-warning, override-init)
CFLAGS_compat_syscall_table.o += $(call cc-disable-warning, override-init)

always-$(KBUILD_BUILTIN) += vmlinux.lds

obj-y	+= head.o
obj-y	+= cpu.o
obj-y	+= entry.o
obj-y	+= irq.o
obj-y	+= process.o
obj-y	+= ptrace.o
obj-y	+= reset.o
obj-y	+= setup.o
obj-y	+= signal.o
obj-y	+= syscall_table.o
obj-y	+= sys_riscv.o
obj-y	+= sys_hwprobe.o
obj-y	+= time.o
obj-y	+= traps.o
obj-y	+= riscv_ksyms.o
obj-y	+= stacktrace.o
obj-y	+= cacheinfo.o
obj-y	+= patch.o
obj-y	+= probes/
obj-y	+= tests/
obj-y	+= vdso.o vdso/

obj-y	+= traps_misaligned.o

obj-$(CONFIG_SMP)		+= smpboot.o
obj-$(CONFIG_SMP)		+= smp.o
obj-$(CONFIG_SMP)		+= cpu_ops.o

obj-$(CONFIG_SMP)		+= cpu_ops_spinwait.o
obj-$(CONFIG_MODULES)		+= module.o
obj-$(CONFIG_MODULE_SECTIONS)	+= module-sections.o

obj-$(CONFIG_PERF_EVENTS)	+= perf_callchain.o
obj-$(CONFIG_HAVE_PERF_REGS)	+= perf_regs.o

obj-$(CONFIG_JUMP_LABEL)	+= jump_label.o
