/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PROCESSOR_H
#define _ASM_RISCV_PROCESSOR_H

#include <linux/cache.h>
#include <linux/prctl.h>

#include <vdso/processor.h>

#include <asm/ptrace.h>

#define arch_get_mmap_end(addr, len, flags)			\
({								\
	STACK_TOP_MAX;						\
})

#define arch_get_mmap_base(addr, base)				\
({								\
	base;							\
})

#define DEFAULT_MAP_WINDOW	TASK_SIZE
#define STACK_TOP_MAX		TASK_SIZE
#define STACK_ALIGN		16

#define STACK_TOP		DEFAULT_MAP_WINDOW

#define user_max_virt_addr() arch_get_mmap_end(ULONG_MAX, 0, 0)

/*
 * This decides where the kernel will search for a free chunk of vm
 * space during mmap's.
 */
#define TASK_UNMAPPED_BASE	PAGE_ALIGN(TASK_SIZE / 3)

#ifndef __ASSEMBLER__

struct task_struct;
struct pt_regs;

/* CPU-specific state of a task */
struct thread_struct {
	/* Callee-saved registers */
	unsigned long ra;
	unsigned long sp;	/* Kernel mode stack */
	unsigned long s[12];	/* s[0]: frame pointer */
	unsigned long bad_cause;
	unsigned long envcfg;
	unsigned long sum;
	unsigned long align_ctl;
#ifdef CONFIG_SMP
	/* Flush the icache on migration */
	bool force_icache_flush;
	/* A forced icache flush is not needed if migrating to the previous cpu. */
	unsigned int prev_cpu;
#endif
};

/* Whitelist the fstate from the task_struct for hardened usercopy */
static inline void arch_thread_struct_whitelist(unsigned long *offset,
						unsigned long *size)
{
	*offset = 0;
	*size = 0;
}

#define INIT_THREAD {					\
	.sp = sizeof(init_stack) + (long)&init_stack,	\
	.align_ctl = PR_UNALIGN_NOPRINT,		\
}

#define task_pt_regs(tsk)						\
	((struct pt_regs *)(task_stack_page(tsk) + THREAD_SIZE		\
			    - ALIGN(sizeof(struct pt_regs), STACK_ALIGN)))

#define KSTK_EIP(tsk)		(task_pt_regs(tsk)->epc)
#define KSTK_ESP(tsk)		(task_pt_regs(tsk)->sp)

/* Do necessary setup to start up a newly executed thread. */
extern void start_thread(struct pt_regs *regs,
			unsigned long pc, unsigned long sp);

extern unsigned long __get_wchan(struct task_struct *p);


static inline void wait_for_interrupt(void)
{
	__asm__ __volatile__ ("wfi");
}

extern phys_addr_t dma32_phys_limit;

struct device_node;
int riscv_of_processor_hartid(struct device_node *node, unsigned long *hartid);
int riscv_early_of_processor_hartid(struct device_node *node, unsigned long *hartid);
int riscv_of_parent_hartid(struct device_node *node, unsigned long *hartid);

extern int arch_dup_task_struct(struct task_struct *dst, struct task_struct *src);

extern unsigned long signal_minsigstksz __ro_after_init;

extern int get_unalign_ctl(struct task_struct *tsk, unsigned long addr);
extern int set_unalign_ctl(struct task_struct *tsk, unsigned int val);

#define GET_UNALIGN_CTL(tsk, addr)	get_unalign_ctl((tsk), (addr))
#define SET_UNALIGN_CTL(tsk, val)	set_unalign_ctl((tsk), (val))

#define RISCV_SET_ICACHE_FLUSH_CTX(arg1, arg2)	riscv_set_icache_flush_ctx(arg1, arg2)
extern int riscv_set_icache_flush_ctx(unsigned long ctx, unsigned long per_thread);

#endif /* __ASSEMBLER__ */

#endif /* _ASM_RISCV_PROCESSOR_H */
