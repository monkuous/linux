/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/asm.h>
#include <asm/csr.h>
#include <asm/unistd.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/asm-offsets.h>
#include <linux/sizes.h>

	.section .irqentry.text, "ax"
	.balign 4096

SYM_CODE_START_NOALIGN(exception_block)

.macro trap_handler_start name
.balign 256
SYM_CODE_START_NOALIGN(entry_\name)
.endm

.macro trap_handler_end name
SYM_CODE_END(entry_\name)
ASM_NOKPROBE(entry_\name)
.endm

.macro trap_handler vec name func
trap_handler_start \name
	/*
	 * If coming from userspace, preserve the user thread pointer and load
	 * the kernel thread pointer.  If we came from the kernel, the scratch
	 * register will contain 0, and we should continue on the current TP.
	 */
	csrw CSR_SCRATCH1, tp
	csrr tp, CSR_SCRATCH0
	bnez tp, 1f
	csrr tp, CSR_SCRATCH1
	REG_S sp, TASK_TI_KERNEL_SP(tp)
1:
	REG_S sp, TASK_TI_USER_SP(tp)
	REG_L sp, TASK_TI_KERNEL_SP(tp)
	addi sp, sp, -(PT_SIZE_ON_STACK)
	REG_S x1,  PT_RA(sp)
	REG_S x3,  PT_GP(sp)
	REG_S x5,  PT_T0(sp)
	save_from_x6_to_x31

	REG_L s0, TASK_TI_USER_SP(tp)
	csrr s1, CSR_RS
	csrr s2, CSR_EPC
	csrr s3, CSR_EBADADDR
	addi s4, zero, \vec
	csrr s5, CSR_SCRATCH1
	REG_S s0, PT_SP(sp)
	REG_S s1, PT_RS(sp)
	REG_S s2, PT_EPC(sp)
	REG_S s3, PT_EBADADDR(sp)
	REG_S s4, PT_CAUSE(sp)
	REG_S s5, PT_TP(sp)

	/*
	 * Set the scratch register to 0, so that if a recursive exception
	 * occurs, the exception vector knows it came from the kernel
	 */
	csrw CSR_SCRATCH0, x0

	/* Load the global pointer */
	load_global_pointer

	move a0, sp /* pt_regs */

	/* Handle trap */
	call \func
	j ret_from_exception
trap_handler_end \name
.endm

trap_handler 0, trap0 do_trap_unknown
trap_handler 1, irq do_irq
trap_handler 2, syscall do_trap_syscall
trap_handler 3, trap3 do_trap_unknown
trap_handler 4, bus_error do_trap_bus_error
trap_handler 5, nmi do_trap_unknown
trap_handler 6, breakpoint do_trap_break
trap_handler 7, invalid_insn do_trap_insn_illegal
trap_handler 8, privileged_insn do_trap_insn_illegal
trap_handler 9, unaligned do_trap_unaligned
trap_handler 10, trap10 do_trap_unknown
trap_handler 11, trap11 do_trap_unknown
trap_handler 12, load_page_fault do_page_fault
trap_handler 13, store_page_fault do_page_fault

.balign 256
SYM_CODE_START_NOALIGN(entry_itb_miss)
	csrr zero, CSR_ITBADDR
	lw zero, (zero)
	csrw CSR_ITBPTE, zero
	sret
SYM_CODE_END(entry_itb_miss)
ASM_NOKPROBE(entry_itb_miss)

.balign 256
SYM_CODE_START_NOALIGN(entry_dtb_miss)
	csrr zero, CSR_DTBADDR
	lw zero, (zero)
	csrw CSR_DTBPTE, zero
	sret
SYM_CODE_END(entry_dtb_miss)
ASM_NOKPROBE(entry_dtb_miss)

SYM_CODE_END(exception_block)
ASM_NOKPROBE(exception_block)

/*
 * The ret_from_exception must be called with interrupt disabled. Here is the
 * caller list:
 *  - exception_block
 *  - ret_from_fork
 */
SYM_CODE_START_NOALIGN(ret_from_exception)
	REG_L s0, PT_RS(sp)
	andi t0, s0, RS_PU
	beqz t0, 1f

#ifdef CONFIG_KSTACK_ERASE
	call	stackleak_erase_on_task_stack
#endif

	/* Save unwound kernel stack pointer in thread_info */
	addi t0, sp, PT_SIZE_ON_STACK
	REG_S t0, TASK_TI_KERNEL_SP(tp)

	/*
	 * Save TP into the scratch register , so we can find the kernel data
	 * structures again.
	 */
	csrw CSR_SCRATCH0, tp
1:
	/*
	 * The current load reservation is effectively part of the processor's
	 * state, in the sense that load reservations cannot be shared between
	 * different hart contexts.  We can't actually save and restore a load
	 * reservation, so instead here we clear any existing reservation --
	 * it's always legal for implementations to clear load reservations at
	 * any point (as long as the forward progress guarantee is kept, but
	 * we'll ignore that here).
	 *
	 * Dangling load reservations can be the result of taking a trap in the
	 * middle of an LR/SC sequence, but can also be the result of a taken
	 * forward branch around an SC -- which is how we implement CAS.  As a
	 * result we need to clear reservations between the last CAS and the
	 * jump back to the new context.  While it is unlikely the store
	 * completes, implementations are allowed to expand reservations to be
	 * arbitrarily large.
	 */
	REG_L  a2, PT_EPC(sp)
	REG_SC x0, a2, PT_EPC(sp)

	csrw CSR_RS, s0
	csrw CSR_EPC, a2

	REG_L x1,  PT_RA(sp)
	REG_L x3,  PT_GP(sp)
	REG_L x4,  PT_TP(sp)
	REG_L x5,  PT_T0(sp)
	restore_from_x6_to_x31

	REG_L x2,  PT_SP(sp)

	sret
SYM_INNER_LABEL(ret_from_exception_end, SYM_L_GLOBAL)
SYM_CODE_END(ret_from_exception)
ASM_NOKPROBE(ret_from_exception)

SYM_CODE_START(ret_from_fork_kernel_asm)
	call schedule_tail
	move a0, s1 /* fn_arg */
	move a1, s0 /* fn */
	move a2, sp /* pt_regs */
	call ret_from_fork_kernel
	j ret_from_exception
SYM_CODE_END(ret_from_fork_kernel_asm)

SYM_CODE_START(ret_from_fork_user_asm)
	call schedule_tail
	move a0, sp /* pt_regs */
	call ret_from_fork_user
	j ret_from_exception
SYM_CODE_END(ret_from_fork_user_asm)

#ifdef CONFIG_IRQ_STACKS
/*
 * void call_on_irq_stack(struct pt_regs *regs,
 * 		          void (*func)(struct pt_regs *));
 *
 * Calls func(regs) using the per-CPU IRQ stack.
 */
SYM_FUNC_START(call_on_irq_stack)
	/* Create a frame record to save ra and s0 (fp) */
	addi	sp, sp, -STACKFRAME_SIZE_ON_STACK
	REG_S	ra, (STACKFRAME_SIZE_ON_STACK + STACKFRAME_RA)(sp)
	REG_S	s0, (STACKFRAME_SIZE_ON_STACK + STACKFRAME_FP)(sp)
	addi	s0, sp, STACKFRAME_SIZE_ON_STACK

	/* Switch to the per-CPU IRQ stack and call the handler */
	load_per_cpu t0, irq_stack_ptr, t1
	li	t1, IRQ_STACK_SIZE
	add	sp, t0, t1
	jalr	a1

	/* Switch back to the thread stack and restore ra and s0 */
	addi	sp, s0, -STACKFRAME_SIZE_ON_STACK
	REG_L	ra, (STACKFRAME_SIZE_ON_STACK + STACKFRAME_RA)(sp)
	REG_L	s0, (STACKFRAME_SIZE_ON_STACK + STACKFRAME_FP)(sp)
	addi	sp, sp, STACKFRAME_SIZE_ON_STACK

	ret
SYM_FUNC_END(call_on_irq_stack)
#endif /* CONFIG_IRQ_STACKS */

/*
 * Integer register context switch
 * The callee-saved registers must be saved and restored.
 *
 *   a0: previous task_struct (must be preserved across the switch)
 *   a1: next task_struct
 *
 * The value of a0 and a1 must be preserved by this function, as that's how
 * arguments are passed to schedule_tail.
 */
SYM_FUNC_START(__switch_to)
	/* Save context into prev->thread */
	li    a4,  TASK_THREAD_RA
	add   a3, a0, a4
	add   a4, a1, a4
	REG_S ra,  TASK_THREAD_RA_RA(a3)
	REG_S sp,  TASK_THREAD_SP_RA(a3)
	REG_S s0,  TASK_THREAD_S0_RA(a3)
	REG_S s1,  TASK_THREAD_S1_RA(a3)
	REG_S s2,  TASK_THREAD_S2_RA(a3)
	REG_S s3,  TASK_THREAD_S3_RA(a3)
	REG_S s4,  TASK_THREAD_S4_RA(a3)
	REG_S s5,  TASK_THREAD_S5_RA(a3)
	REG_S s6,  TASK_THREAD_S6_RA(a3)
	REG_S s7,  TASK_THREAD_S7_RA(a3)
	REG_S s8,  TASK_THREAD_S8_RA(a3)
	REG_S s9,  TASK_THREAD_S9_RA(a3)
	REG_S s10, TASK_THREAD_S10_RA(a3)
	REG_S s11, TASK_THREAD_S11_RA(a3)

	/* Restore context from next->thread */
	REG_L ra,  TASK_THREAD_RA_RA(a4)
	REG_L sp,  TASK_THREAD_SP_RA(a4)
	REG_L s0,  TASK_THREAD_S0_RA(a4)
	REG_L s1,  TASK_THREAD_S1_RA(a4)
	REG_L s2,  TASK_THREAD_S2_RA(a4)
	REG_L s3,  TASK_THREAD_S3_RA(a4)
	REG_L s4,  TASK_THREAD_S4_RA(a4)
	REG_L s5,  TASK_THREAD_S5_RA(a4)
	REG_L s6,  TASK_THREAD_S6_RA(a4)
	REG_L s7,  TASK_THREAD_S7_RA(a4)
	REG_L s8,  TASK_THREAD_S8_RA(a4)
	REG_L s9,  TASK_THREAD_S9_RA(a4)
	REG_L s10, TASK_THREAD_S10_RA(a4)
	REG_L s11, TASK_THREAD_S11_RA(a4)
	/* The offset of thread_info in task_struct is zero. */
	move tp, a1
	ret
SYM_FUNC_END(__switch_to)
