// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/sched/task_stack.h>
#include <asm/cpu_ops.h>
#include <asm/smp.h>

#include "head.h"

/* timeout if a cpu doesn't respond for 100ms (checked in 100us iterations) */
#define CPU_BOOT_TIMEOUT_ITERS 1000
#define CPU_BOOT_TIMEOUT_ITER_US 100

const struct cpu_operations cpu_ops_spinwait;
void *__cpu_spinwait_stack_pointer __section(".data");
void *__cpu_spinwait_task_pointer __section(".data");
atomic_long_t __cpu_spinwait_current_hart __section(".data")
	= ATOMIC_LONG_INIT(INVALID_HARTID);

static DEFINE_SPINLOCK(cpu_spinwait_lock);

static int cpu_update_secondary_bootdata(unsigned int cpuid,
					 struct task_struct *tidle)
{
	unsigned long hartid = cpuid_to_hartid_map(cpuid);
	unsigned long flags;
	int i;

	if (hartid == INVALID_HARTID)
		return -EINVAL;

	spin_lock_irqsave(&cpu_spinwait_lock, flags);

	__cpu_spinwait_stack_pointer = task_pt_regs(tidle);
	__cpu_spinwait_task_pointer = tidle;
	atomic_long_set_release(&__cpu_spinwait_current_hart, hartid);

	for (i = 0; i < CPU_BOOT_TIMEOUT_ITERS; i++) {
		hartid = atomic_long_read_acquire(&__cpu_spinwait_current_hart);

		if (hartid == INVALID_HARTID) {
			break;
		}

		udelay(CPU_BOOT_TIMEOUT_ITER_US);
	}

	if (hartid == INVALID_HARTID) {
		hartid = atomic_long_xchg(&__cpu_spinwait_current_hart,
					  INVALID_HARTID);
	}

	spin_unlock_irqrestore(&cpu_spinwait_lock, flags);
	return hartid == INVALID_HARTID ? 0 : -ETIMEDOUT;
}

static int spinwait_cpu_start(unsigned int cpuid, struct task_struct *tidle)
{
	/*
	 * In this protocol, all cpus boot on their own accord.  _start
	 * selects the first cpu to boot the kernel and causes the remainder
	 * of the cpus to spin in a loop waiting for their stack pointer to be
	 * setup by that main cpu.  Writing to __cpu_spinwait_current_hart
	 * signals to the given cpu that its boot info is ready and it can
	 * continue the boot process. Once it has accepted its boot info,
	 * it will set the variable to INVALID_HARTID, signaling to the boot
	 * cpu that it can continue. This is done using an xchg, and if the
	 * old value doesn't match the hartid (boot cpu decided to time out),
	 * the cpu parks itself without using any of the boot info it read.
	 */
	return cpu_update_secondary_bootdata(cpuid, tidle);
}

const struct cpu_operations cpu_ops_spinwait = {
	.cpu_start	= spinwait_cpu_start,
};
