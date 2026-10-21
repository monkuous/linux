/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2017 Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_CACHE_H
#define _ASM_RISCV_CACHE_H

#include <vdso/page.h>

#define L1_CACHE_SHIFT		6

#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#define ARCH_KMALLOC_MINALIGN	(8)

#endif /* _ASM_RISCV_CACHE_H */
