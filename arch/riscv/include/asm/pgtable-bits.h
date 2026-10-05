/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_BITS_H
#define _ASM_RISCV_PGTABLE_BITS_H

#define _PAGE_ACCESSED_OFFSET 0

#define _PAGE_ACCESSED	(1 << 0)	/* Readable by hardware */
#define _PAGE_DIRTY	(1 << 1)	/* Writable by hardware */
#define _PAGE_KMODE	(1 << 2)	/* Not accessible by user mode */
#define _PAGE_IO	(1 << 3)	/* Not cached */
#define _PAGE_GLOBAL	(1 << 4)	/* Global */
#define _PAGE_SOFT	(0x7fU << 25)	/* Reserved for software */

#define _PAGE_PRESENT	(1 << 25)	/* Readable (RSW: 0x1) */
#define _PAGE_WRITE	(1 << 26)	/* Writable (RSW: 0x2) */
#define _PAGE_EXEC	(1 << 27)	/* Executable (RSW: 0x4) */
#define _PAGE_MODIFIED	(1 << 28)	/* Modified (RSW: 0x8) */
#define _PAGE_SPECIAL	(1 << 29)	/* RSW: 0x10 */

#define _PAGE_TABLE     (_PAGE_ACCESSED | _PAGE_KMODE | _PAGE_PRESENT)

/*
 * _PAGE_PROT_NONE is set on not-present pages (and ignored by the hardware) to
 * distinguish them from swapped out pages
 */
#define _PAGE_PROT_NONE _PAGE_KMODE

/* Used for swap PTEs only. */
#define _PAGE_SWP_EXCLUSIVE _PAGE_IO

#define _PAGE_PFN_SHIFT 5

#endif /* _ASM_RISCV_PGTABLE_BITS_H */
