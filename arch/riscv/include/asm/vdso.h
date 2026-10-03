/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Limited
 * Copyright (C) 2014 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#ifndef _ASM_RISCV_VDSO_H
#define _ASM_RISCV_VDSO_H

/*
 * All systems with an MMU have a VDSO, but systems without an MMU don't
 * support shared libraries and therefore don't have one.
 */

#define __VDSO_PAGES    4

#ifndef __ASSEMBLER__

#include <generated/vdso-offsets.h>

#define VDSO_SYMBOL(base, name)							\
	  ((void __user *)((unsigned long)(base) + __vdso_##name##_offset))

extern char vdso_start[], vdso_end[];

#endif /* !__ASSEMBLER__ */

#endif /* _ASM_RISCV_VDSO_H */
