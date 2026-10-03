/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_TIMEX_H
#define _ASM_RISCV_TIMEX_H

#include <asm/csr.h>

typedef unsigned long cycles_t;

static inline cycles_t get_cycles(void)
{
	return csr_read(CSR_TIME);
}
#define get_cycles get_cycles

static inline u32 get_cycles_hi(void)
{
	return csr_read(CSR_TIMEH);
}
#define get_cycles_hi get_cycles_hi

static inline u64 get_cycles64(void)
{
	u32 hi, lo;

	do {
		hi = get_cycles_hi();
		lo = get_cycles();
	} while (hi != get_cycles_hi());

	return ((u64)hi << 32) | lo;
}

#endif /* _ASM_RISCV_TIMEX_H */
