// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 */

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/sched.h>
#include <asm/cpu_ops.h>
#include <asm/smp.h>

const struct cpu_operations *cpu_ops __ro_after_init = &cpu_ops_spinwait;

extern const struct cpu_operations cpu_ops_sbi;

void __init cpu_set_ops(void)
{
}
