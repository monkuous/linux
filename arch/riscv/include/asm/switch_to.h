/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_SWITCH_TO_H
#define _ASM_RISCV_SWITCH_TO_H

#include <linux/jump_label.h>
#include <linux/sched/task_stack.h>
#include <linux/mm_types.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/csr.h>

extern struct task_struct *__switch_to(struct task_struct *,
				       struct task_struct *);

static inline bool switch_to_should_flush_icache(struct task_struct *task)
{
#ifdef CONFIG_SMP
	bool stale_mm = task->mm && task->mm->context.force_icache_flush;
	bool stale_thread = task->thread.force_icache_flush;
	bool thread_migrated = smp_processor_id() != task->thread.prev_cpu;

	return thread_migrated && (stale_mm || stale_thread);
#else
	return false;
#endif
}

#ifdef CONFIG_SMP
#define __set_prev_cpu(thread) ((thread).prev_cpu = smp_processor_id())
#else
#define __set_prev_cpu(thread)
#endif

#define switch_to(prev, next, last)			\
do {							\
	struct task_struct *__prev = (prev);		\
	struct task_struct *__next = (next);		\
	__set_prev_cpu(__prev->thread);			\
	if (switch_to_should_flush_icache(__next))	\
		local_flush_icache_all();		\
	((last) = __switch_to(__prev, __next));		\
} while (0)

#endif /* _ASM_RISCV_SWITCH_TO_H */
