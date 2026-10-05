/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#include <asm-generic/pgtable-nopmd.h>
#include <linux/bits.h>
#include <linux/const.h>

/* Size of region mapped by a page global directory */
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#define MAX_POSSIBLE_PHYSMEM_BITS 32

/*
 * rv32 PTE format:
 * | 31           25 | 24   5  | 4 | 3 | 2 | 1 | 0
 *   reserved for SW     PFN     G   N   K   W   V
 */
#define _PAGE_PFN_MASK  GENMASK(24, 5)

/* Set of bits to preserve across pte_modify() */
#define _PAGE_CHG_MASK  (~(unsigned long)(_PAGE_ACCESSED | _PAGE_DIRTY |	\
					  _PAGE_IO | _PAGE_GLOBAL |	\
					  _PAGE_PRESENT | _PAGE_WRITE |	\
					  _PAGE_EXEC))

#endif /* _ASM_RISCV_PGTABLE_32_H */
