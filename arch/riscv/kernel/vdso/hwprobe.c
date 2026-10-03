// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 Rivos, Inc
 */

#include <linux/string.h>
#include <linux/types.h>
#include <vdso/datapage.h>
#include <vdso/helpers.h>
#include <asm/hwprobe.h>

extern int riscv_hwprobe(struct riscv_hwprobe *pairs, size_t pair_count,
			 size_t cpusetsize, unsigned long *cpus,
			 unsigned int flags);

/* Add a prototype to avoid -Wmissing-prototypes warning. */
int __vdso_riscv_hwprobe(struct riscv_hwprobe *pairs, size_t pair_count,
			 size_t cpusetsize, unsigned long *cpus,
			 unsigned int flags);

int __vdso_riscv_hwprobe(struct riscv_hwprobe *pairs, size_t pair_count,
			 size_t cpusetsize, unsigned long *cpus,
			 unsigned int flags)
{
	return riscv_hwprobe(pairs, pair_count, cpusetsize, cpus, flags);
}
