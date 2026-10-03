/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */


#ifndef _ASM_RISCV_MMU_H
#define _ASM_RISCV_MMU_H

#ifndef __ASSEMBLER__

typedef struct {
	atomic_long_t id;
	void *vdso;
#ifdef CONFIG_SMP
	/* A local icache flush is needed before user execution can resume. */
	cpumask_t icache_stale_mask;
	/* Force local icache flush on all migrations. */
	bool force_icache_flush;
#endif
	unsigned long flags;
} mm_context_t;

/* Lock the pointer masking mode because this mm is multithreaded */
#define MM_CONTEXT_LOCK_PMLEN	0

#define cntx2asid(cntx)		((cntx) & SATP_ASID_MASK)
#define cntx2version(cntx)	((cntx) & ~SATP_ASID_MASK)

#endif /* __ASSEMBLER__ */

#endif /* _ASM_RISCV_MMU_H */
