/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _ASM_RISCV_PROTOTYPES_H
#define _ASM_RISCV_PROTOTYPES_H

#include <linux/ftrace.h>
#include <asm-generic/asm-prototypes.h>

#define DECLARE_DO_ERROR_INFO(name)	asmlinkage void name(struct pt_regs *regs)

DECLARE_DO_ERROR_INFO(do_trap_unknown);
DECLARE_DO_ERROR_INFO(do_trap_syscall);
DECLARE_DO_ERROR_INFO(do_trap_bus_error);
DECLARE_DO_ERROR_INFO(do_trap_break);
DECLARE_DO_ERROR_INFO(do_trap_insn_illegal);
DECLARE_DO_ERROR_INFO(do_trap_unaligned);

asmlinkage void ret_from_fork_kernel(void *fn_arg, int (*fn)(void *), struct pt_regs *regs);
asmlinkage void ret_from_fork_user(struct pt_regs *regs);
asmlinkage void handle_bad_stack(struct pt_regs *regs);
asmlinkage void do_page_fault(struct pt_regs *regs);
asmlinkage void do_irq(struct pt_regs *regs);

#endif /* _ASM_RISCV_PROTOTYPES_H */
