/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2009 Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_TLBFLUSH_H
#define _ASM_RISCV_TLBFLUSH_H

#include <linux/mm_types.h>
#include <asm/csr.h>
#include <asm/smp.h>

#define FLUSH_TLB_MAX_SIZE      ((unsigned long)-1)
#define FLUSH_TLB_NO_ASID       ((unsigned long)-1)

static inline unsigned long get_mm_asid(struct mm_struct *mm)
{
	return mm ? cntx2asid(atomic_long_read(&mm->context.id)) : FLUSH_TLB_NO_ASID;
}

static inline void local_flush_tlb_all(void)
{
	csr_write(CSR_ITBCTRL, TBCTRL_FLUSH_NON_WIRED);
	csr_write(CSR_DTBCTRL, TBCTRL_FLUSH_NON_WIRED);
}

static inline void local_flush_tlb_all_asid(unsigned long asid)
{
	if (asid != FLUSH_TLB_NO_ASID){
		csr_write(CSR_ITBCTRL, TBCTRL_FLUSH_PRIVATE);
		csr_write(CSR_DTBCTRL, TBCTRL_FLUSH_PRIVATE);
	} else
		local_flush_tlb_all();
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
	unsigned long vpn = (addr >> PAGE_SHIFT) & TBCTRL_VPN_MASK;
	csr_write(CSR_ITBCTRL, (vpn << TBCTRL_VPN_SHIFT) | TBCTRL_FLUSH_ONE);
	csr_write(CSR_DTBCTRL, (vpn << TBCTRL_VPN_SHIFT) | TBCTRL_FLUSH_ONE);

	addr = PGTABLE_START | ((addr >> PAGE_SHIFT) << 2);
	vpn = (addr >> PAGE_SHIFT) & TBCTRL_VPN_MASK;
	csr_write(CSR_ITBCTRL, (vpn << TBCTRL_VPN_SHIFT) | TBCTRL_FLUSH_ONE);
	csr_write(CSR_DTBCTRL, (vpn << TBCTRL_VPN_SHIFT) | TBCTRL_FLUSH_ONE);
}

static inline void local_flush_tlb_page_asid(unsigned long addr,
					     unsigned long asid)
{
	local_flush_tlb_page(addr);
}

void flush_tlb_all(void);
void flush_tlb_mm(struct mm_struct *mm);
void flush_tlb_mm_range(struct mm_struct *mm, unsigned long start,
			unsigned long end, unsigned int page_size);
void flush_tlb_page(struct vm_area_struct *vma, unsigned long addr);
void flush_tlb_range(struct vm_area_struct *vma, unsigned long start,
		     unsigned long end);
void flush_tlb_kernel_range(unsigned long start, unsigned long end);
void local_flush_tlb_kernel_range(unsigned long start, unsigned long end);

bool arch_tlbbatch_should_defer(struct mm_struct *mm);
void arch_tlbbatch_add_pending(struct arch_tlbflush_unmap_batch *batch,
		struct mm_struct *mm, unsigned long start, unsigned long end);
void arch_tlbbatch_flush(struct arch_tlbflush_unmap_batch *batch);

extern unsigned long tlb_flush_all_threshold;

#endif /* _ASM_RISCV_TLBFLUSH_H */
