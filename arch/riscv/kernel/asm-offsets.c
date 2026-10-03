// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */
#define COMPILE_OFFSETS

#include <linux/kbuild.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/thread_info.h>
#include <asm/ptrace.h>
#include <asm/cpu_ops_sbi.h>
#include <asm/stacktrace.h>

void asm_offsets(void);

void asm_offsets(void)
{
	OFFSET(TASK_THREAD_RA, task_struct, thread.ra);
	OFFSET(TASK_THREAD_SP, task_struct, thread.sp);
	OFFSET(TASK_THREAD_S0, task_struct, thread.s[0]);
	OFFSET(TASK_THREAD_S1, task_struct, thread.s[1]);
	OFFSET(TASK_THREAD_S2, task_struct, thread.s[2]);
	OFFSET(TASK_THREAD_S3, task_struct, thread.s[3]);
	OFFSET(TASK_THREAD_S4, task_struct, thread.s[4]);
	OFFSET(TASK_THREAD_S5, task_struct, thread.s[5]);
	OFFSET(TASK_THREAD_S6, task_struct, thread.s[6]);
	OFFSET(TASK_THREAD_S7, task_struct, thread.s[7]);
	OFFSET(TASK_THREAD_S8, task_struct, thread.s[8]);
	OFFSET(TASK_THREAD_S9, task_struct, thread.s[9]);
	OFFSET(TASK_THREAD_S10, task_struct, thread.s[10]);
	OFFSET(TASK_THREAD_S11, task_struct, thread.s[11]);
	OFFSET(TASK_THREAD_SUM, task_struct, thread.sum);

	OFFSET(TASK_TI_CPU, task_struct, thread_info.cpu);
	OFFSET(TASK_TI_PREEMPT_COUNT, task_struct, thread_info.preempt_count);
	OFFSET(TASK_TI_KERNEL_SP, task_struct, thread_info.kernel_sp);
	OFFSET(TASK_TI_USER_SP, task_struct, thread_info.user_sp);

	OFFSET(TASK_TI_CPU_NUM, task_struct, thread_info.cpu);
#ifdef CONFIG_STACKPROTECTOR
	OFFSET(TSK_STACK_CANARY, task_struct, stack_canary);
#endif

	DEFINE(PT_SIZE, sizeof(struct pt_regs));
	OFFSET(PT_EPC, pt_regs, epc);
	OFFSET(PT_RA, pt_regs, ra);
	OFFSET(PT_FP, pt_regs, s0);
	OFFSET(PT_S0, pt_regs, s0);
	OFFSET(PT_S1, pt_regs, s1);
	OFFSET(PT_S2, pt_regs, s2);
	OFFSET(PT_S3, pt_regs, s3);
	OFFSET(PT_S4, pt_regs, s4);
	OFFSET(PT_S5, pt_regs, s5);
	OFFSET(PT_S6, pt_regs, s6);
	OFFSET(PT_S7, pt_regs, s7);
	OFFSET(PT_S8, pt_regs, s8);
	OFFSET(PT_S9, pt_regs, s9);
	OFFSET(PT_S10, pt_regs, s10);
	OFFSET(PT_S11, pt_regs, s11);
	OFFSET(PT_SP, pt_regs, sp);
	OFFSET(PT_TP, pt_regs, tp);
	OFFSET(PT_A0, pt_regs, a0);
	OFFSET(PT_A1, pt_regs, a1);
	OFFSET(PT_A2, pt_regs, a2);
	OFFSET(PT_A3, pt_regs, a3);
	OFFSET(PT_A4, pt_regs, a4);
	OFFSET(PT_A5, pt_regs, a5);
	OFFSET(PT_A6, pt_regs, a6);
	OFFSET(PT_A7, pt_regs, a7);
	OFFSET(PT_T0, pt_regs, t0);
	OFFSET(PT_T1, pt_regs, t1);
	OFFSET(PT_T2, pt_regs, t2);
	OFFSET(PT_T3, pt_regs, t3);
	OFFSET(PT_T4, pt_regs, t4);
	OFFSET(PT_T5, pt_regs, t5);
	OFFSET(PT_T6, pt_regs, t6);
	OFFSET(PT_GP, pt_regs, gp);
	OFFSET(PT_ORIG_A0, pt_regs, orig_a0);
	OFFSET(PT_STATUS, pt_regs, status);
	OFFSET(PT_BADADDR, pt_regs, badaddr);
	OFFSET(PT_CAUSE, pt_regs, cause);

	/*
	 * THREAD_{F,X}* might be larger than a S-type offset can handle, but
	 * these are used in performance-sensitive assembly so we can't resort
	 * to loading the long immediate every time.
	 */
	DEFINE(TASK_THREAD_RA_RA,
		  offsetof(struct task_struct, thread.ra)
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_SP_RA,
		  offsetof(struct task_struct, thread.sp)
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S0_RA,
		  offsetof(struct task_struct, thread.s[0])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S1_RA,
		  offsetof(struct task_struct, thread.s[1])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S2_RA,
		  offsetof(struct task_struct, thread.s[2])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S3_RA,
		  offsetof(struct task_struct, thread.s[3])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S4_RA,
		  offsetof(struct task_struct, thread.s[4])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S5_RA,
		  offsetof(struct task_struct, thread.s[5])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S6_RA,
		  offsetof(struct task_struct, thread.s[6])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S7_RA,
		  offsetof(struct task_struct, thread.s[7])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S8_RA,
		  offsetof(struct task_struct, thread.s[8])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S9_RA,
		  offsetof(struct task_struct, thread.s[9])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S10_RA,
		  offsetof(struct task_struct, thread.s[10])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_S11_RA,
		  offsetof(struct task_struct, thread.s[11])
		- offsetof(struct task_struct, thread.ra)
	);
	DEFINE(TASK_THREAD_SUM_RA,
		  offsetof(struct task_struct, thread.sum)
		- offsetof(struct task_struct, thread.ra)
	);

	/*
	 * We allocate a pt_regs on the stack when entering the kernel.  This
	 * ensures the alignment is sane.
	 */
	DEFINE(PT_SIZE_ON_STACK, ALIGN(sizeof(struct pt_regs), STACK_ALIGN));

	OFFSET(SBI_HART_BOOT_TASK_PTR_OFFSET, sbi_hart_boot_data, task_ptr);
	OFFSET(SBI_HART_BOOT_STACK_PTR_OFFSET, sbi_hart_boot_data, stack_ptr);

	DEFINE(STACKFRAME_SIZE_ON_STACK, ALIGN(sizeof(struct stackframe), STACK_ALIGN));
	DEFINE(STACKFRAME_FP, offsetof(struct stackframe, fp) - sizeof(struct stackframe));
	DEFINE(STACKFRAME_RA, offsetof(struct stackframe, ra) - sizeof(struct stackframe));
}
