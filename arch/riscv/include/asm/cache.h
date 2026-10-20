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

#define ARCH_DMA_MINALIGN PAGE_SIZE
#define ARCH_KMALLOC_MINALIGN	(8)

#ifndef __ASSEMBLER__

#define dma_get_cache_alignment dma_get_cache_alignment
static inline int dma_get_cache_alignment(void)
{
	return ARCH_DMA_MINALIGN;
}

#endif	/* __ASSEMBLER__ */

#endif /* _ASM_RISCV_CACHE_H */
