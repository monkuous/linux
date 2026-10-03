/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _ASM_RISCV_IRQ_STACK_H
#define _ASM_RISCV_IRQ_STACK_H

#include <linux/bug.h>
#include <linux/gfp.h>
#include <linux/kconfig.h>
#include <linux/vmalloc.h>
#include <linux/pgtable.h>
#include <asm/thread_info.h>

DECLARE_PER_CPU(ulong *, irq_stack_ptr);

asmlinkage void call_on_irq_stack(struct pt_regs *regs,
				  void (*func)(struct pt_regs *));

#endif /* _ASM_RISCV_IRQ_STACK_H */
