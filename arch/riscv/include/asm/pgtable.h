/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_H
#define _ASM_RISCV_PGTABLE_H

#include <linux/mmzone.h>
#include <linux/sizes.h>

#include <asm/pgtable-bits.h>

#define ADDRESS_SPACE_END	(UL(-1))

#define KERNEL_LINK_ADDR	PAGE_OFFSET

/* Number of entries in the page global directory */
#define PTRS_PER_PGD    (PAGE_SIZE / sizeof(pgd_t))
/* Number of entries in the page table */
#define PTRS_PER_PTE    (PAGE_SIZE / sizeof(pte_t))

/*
 * Half of the kernel address space (1/4 of the entries of the page global
 * directory) is for the direct mapping.
 */
#define KERN_VIRT_SIZE          ((PTRS_PER_PGD / 2 * PGDIR_SIZE) / 2)

#define VMALLOC_SIZE     (KERN_VIRT_SIZE >> 1)
#define VMALLOC_END      PAGE_OFFSET
#define VMALLOC_START    (PAGE_OFFSET - VMALLOC_SIZE)

#define MODULES_VADDR		VMALLOC_START
#define MODULES_END		VMALLOC_END

#define VA_BITS_SV32 32
#define VA_BITS		VA_BITS_SV32

/* Needed to limit get_free_mem_region() */
#if defined(CONFIG_FLATMEM)
#define DIRECT_MAP_PHYSMEM_END (phys_ram_base + KERN_VIRT_SIZE - 1)
#elif defined(CONFIG_SPARSEMEM)
/* DIRECT_MAP_PHYSMEM_END is not limited by VA space assignment in this case */
#endif

#define PCI_IO_SIZE      SZ_16M
#define PCI_IO_END       VMALLOC_START
#define PCI_IO_START     (PCI_IO_END - PCI_IO_SIZE)

#define FIXADDR_TOP      PCI_IO_START
#define MAX_FDT_SIZE	 PGDIR_SIZE
#define FIX_FDT_SIZE	 MAX_FDT_SIZE
#define FIXADDR_SIZE     (PGDIR_SIZE + FIX_FDT_SIZE)
#define FIXADDR_START    (FIXADDR_TOP - FIXADDR_SIZE)

#ifndef __ASSEMBLER__

#include <asm/page.h>
#include <asm/tlbflush.h>
#include <linux/mm_types.h>

#define __page_val_to_pfn(_val)  (((_val) & _PAGE_PFN_MASK) >> _PAGE_PFN_SHIFT)

#include <asm/pgtable-32.h>

#include <linux/page_table_check.h>

struct pt_alloc_ops {
	pte_t *(*get_pte_virt)(phys_addr_t pa);
	phys_addr_t (*alloc_pte)(uintptr_t va);
};

extern struct pt_alloc_ops pt_ops __meminitdata;

/* Number of PGD entries that a user-mode program can use */
#define USER_PTRS_PER_PGD   (TASK_SIZE / PGDIR_SIZE)

/* Page protection bits */
#define _PAGE_BASE	(_PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_USER)

#define PAGE_NONE		__pgprot(_PAGE_PROT_NONE | _PAGE_READ)
#define PAGE_READ		__pgprot(_PAGE_BASE | _PAGE_READ)
#define PAGE_WRITE		__pgprot(_PAGE_BASE | _PAGE_READ | _PAGE_WRITE)
#define PAGE_EXEC		__pgprot(_PAGE_BASE | _PAGE_EXEC)
#define PAGE_READ_EXEC		__pgprot(_PAGE_BASE | _PAGE_READ | _PAGE_EXEC)
#define PAGE_WRITE_EXEC		__pgprot(_PAGE_BASE | _PAGE_READ |	\
					 _PAGE_EXEC | _PAGE_WRITE)
#define PAGE_SHADOWSTACK       __pgprot(_PAGE_BASE | _PAGE_WRITE)

#define PAGE_COPY		PAGE_READ
#define PAGE_COPY_EXEC		PAGE_READ_EXEC
#define PAGE_SHARED		PAGE_WRITE
#define PAGE_SHARED_EXEC	PAGE_WRITE_EXEC

#define _PAGE_KERNEL		(_PAGE_READ \
				| _PAGE_WRITE \
				| _PAGE_PRESENT \
				| _PAGE_ACCESSED \
				| _PAGE_DIRTY \
				| _PAGE_GLOBAL)

#define PAGE_KERNEL		__pgprot(_PAGE_KERNEL)
#define PAGE_KERNEL_READ	__pgprot(_PAGE_KERNEL & ~_PAGE_WRITE)
#define PAGE_KERNEL_EXEC	__pgprot(_PAGE_KERNEL | _PAGE_EXEC)
#define PAGE_KERNEL_READ_EXEC	__pgprot((_PAGE_KERNEL & ~_PAGE_WRITE) \
					 | _PAGE_EXEC)

#define PAGE_TABLE		__pgprot(_PAGE_TABLE)

#define _PAGE_KERNEL_NC ((_PAGE_KERNEL & ~_PAGE_MTMASK) | _PAGE_NOCACHE)
#define _PAGE_IOREMAP	((_PAGE_KERNEL & ~_PAGE_MTMASK) | _PAGE_IO)
#define PAGE_KERNEL_IO		__pgprot(_PAGE_IOREMAP)

extern pgd_t swapper_pg_dir[];
extern pgd_t trampoline_pg_dir[];
extern pgd_t early_pg_dir[];

static inline int pmd_present(pmd_t pmd)
{
	return (pmd_val(pmd) & (_PAGE_PRESENT | _PAGE_PROT_NONE));
}

static inline int pmd_none(pmd_t pmd)
{
	return (pmd_val(pmd) == 0);
}

static inline int pmd_bad(pmd_t pmd)
{
	return !pmd_present(pmd) || (pmd_val(pmd) & _PAGE_LEAF);
}

static inline void set_pmd(pmd_t *pmdp, pmd_t pmd)
{
	WRITE_ONCE(*pmdp, pmd);
}

static inline void pmd_clear(pmd_t *pmdp)
{
	set_pmd(pmdp, __pmd(0));
}

static inline pgd_t pfn_pgd(unsigned long pfn, pgprot_t prot)
{
	unsigned long prot_val = pgprot_val(prot);

	return __pgd((pfn << _PAGE_PFN_SHIFT) | prot_val);
}

static inline unsigned long _pgd_pfn(pgd_t pgd)
{
	return __page_val_to_pfn(pgd_val(pgd));
}

static inline struct page *pmd_page(pmd_t pmd)
{
	return pfn_to_page(__page_val_to_pfn(pmd_val(pmd)));
}

static inline unsigned long pmd_page_vaddr(pmd_t pmd)
{
	return (unsigned long)pfn_to_virt(__page_val_to_pfn(pmd_val(pmd)));
}

static inline pte_t pmd_pte(pmd_t pmd)
{
	return __pte(pmd_val(pmd));
}

static inline pte_t pud_pte(pud_t pud)
{
	return __pte(pud_val(pud));
}

/* Yields the page frame number (PFN) of a page table entry */
static inline unsigned long pte_pfn(pte_t pte)
{
	return __page_val_to_pfn(pte_val(pte));
}

#define pte_page(x)     pfn_to_page(pte_pfn(x))

/* Constructs a page table entry */
static inline pte_t pfn_pte(unsigned long pfn, pgprot_t prot)
{
	unsigned long prot_val = pgprot_val(prot);

	return __pte((pfn << _PAGE_PFN_SHIFT) | prot_val);
}

#define pte_pgprot pte_pgprot
static inline pgprot_t pte_pgprot(pte_t pte)
{
	unsigned long pfn = pte_pfn(pte);

	return __pgprot(pte_val(pfn_pte(pfn, __pgprot(0))) ^ pte_val(pte));
}

static inline int pte_present(pte_t pte)
{
	return (pte_val(pte) & (_PAGE_PRESENT | _PAGE_PROT_NONE));
}

#define pte_accessible pte_accessible
static inline unsigned long pte_accessible(struct mm_struct *mm, pte_t a)
{
	if (pte_val(a) & _PAGE_PRESENT)
		return true;

	if ((pte_val(a) & _PAGE_PROT_NONE) &&
	    atomic_read(&mm->tlb_flush_pending))
		return true;

	return false;
}

static inline int pte_none(pte_t pte)
{
	return (pte_val(pte) == 0);
}

static inline int pte_write(pte_t pte)
{
	return pte_val(pte) & _PAGE_WRITE;
}

static inline int pte_exec(pte_t pte)
{
	return pte_val(pte) & _PAGE_EXEC;
}

static inline int pte_user(pte_t pte)
{
	return pte_val(pte) & _PAGE_USER;
}

static inline int pte_huge(pte_t pte)
{
	return pte_present(pte) && (pte_val(pte) & _PAGE_LEAF);
}

static inline int pte_dirty(pte_t pte)
{
	return pte_val(pte) & _PAGE_DIRTY;
}

static inline int pte_young(pte_t pte)
{
	return pte_val(pte) & _PAGE_ACCESSED;
}

static inline int pte_special(pte_t pte)
{
	return pte_val(pte) & _PAGE_SPECIAL;
}

/* static inline pte_t pte_rdprotect(pte_t pte) */

static inline pte_t pte_wrprotect(pte_t pte)
{
	return __pte((pte_val(pte) & ~(_PAGE_WRITE)) | (_PAGE_READ));
}

/* static inline pte_t pte_mkread(pte_t pte) */

struct vm_area_struct;
pte_t pte_mkwrite(pte_t pte, struct vm_area_struct *vma);
#define pte_mkwrite pte_mkwrite

static inline pte_t pte_mkwrite_novma(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_WRITE);
}

static inline pte_t pte_mkwrite_shstk(pte_t pte)
{
	return __pte((pte_val(pte) & ~(_PAGE_LEAF)) | _PAGE_WRITE);
}

/* static inline pte_t pte_mkexec(pte_t pte) */

static inline pte_t pte_mkdirty(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_DIRTY);
}

static inline pte_t pte_mkclean(pte_t pte)
{
	return __pte(pte_val(pte) & ~(_PAGE_DIRTY));
}

static inline pte_t pte_mkyoung(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_ACCESSED);
}

static inline pte_t pte_mkold(pte_t pte)
{
	return __pte(pte_val(pte) & ~(_PAGE_ACCESSED));
}

static inline pte_t pte_mkspecial(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_SPECIAL);
}

static inline pte_t pte_mkhuge(pte_t pte)
{
	return pte;
}

#ifdef CONFIG_ARCH_HAS_PTE_PROTNONE
static inline int pte_protnone(pte_t pte)
{
	return (pte_val(pte) & (_PAGE_PRESENT | _PAGE_PROT_NONE)) == _PAGE_PROT_NONE;
}

static inline int pmd_protnone(pmd_t pmd)
{
	return pte_protnone(pmd_pte(pmd));
}
#endif /* CONFIG_ARCH_HAS_PTE_PROTNONE */

/* Modify page protection bits */
static inline pte_t pte_modify(pte_t pte, pgprot_t newprot)
{
	unsigned long newprot_val = pgprot_val(newprot);

	return __pte((pte_val(pte) & _PAGE_CHG_MASK) | newprot_val);
}

#define pgd_ERROR(e) \
	pr_err("%s:%d: bad pgd " PTE_FMT ".\n", __FILE__, __LINE__, pgd_val(e))


/* Commit new configuration to MMU hardware */
static inline void update_mmu_cache_range(struct vm_fault *vmf,
		struct vm_area_struct *vma, unsigned long address,
		pte_t *ptep, unsigned int nr)
{
	unsigned long asid = get_mm_asid(vma->vm_mm);

	/*
	 * The kernel assumes that TLBs don't cache invalid entries, but
	 * in RISC-V, SFENCE.VMA specifies an ordering constraint, not a
	 * cache flush; it is necessary even after writing invalid entries.
	 * Relying on flush_tlb_fix_spurious_fault would suffice, but
	 * the extra traps reduce performance.  So, eagerly SFENCE.VMA.
	 */
	while (nr--)
		local_flush_tlb_page_asid(address + nr * PAGE_SIZE, asid);

}
#define update_mmu_cache(vma, addr, ptep) \
	update_mmu_cache_range(NULL, vma, addr, ptep, 1)

#define update_mmu_tlb_range(vma, addr, ptep, nr) \
	update_mmu_cache_range(NULL, vma, addr, ptep, nr)

static inline void update_mmu_cache_pmd(struct vm_area_struct *vma,
		unsigned long address, pmd_t *pmdp)
{
	pte_t *ptep = (pte_t *)pmdp;

	update_mmu_cache(vma, address, ptep);
}

#define __HAVE_ARCH_PTE_SAME
static inline int pte_same(pte_t pte_a, pte_t pte_b)
{
	return pte_val(pte_a) == pte_val(pte_b);
}

/*
 * Certain architectures need to do special things when PTEs within
 * a page table are directly modified.  Thus, the following hook is
 * made available.
 */
static inline void set_pte(pte_t *ptep, pte_t pteval)
{
	WRITE_ONCE(*ptep, pteval);
}

void flush_icache_pte(struct mm_struct *mm, pte_t pte);

static inline void __set_pte_at(struct mm_struct *mm, pte_t *ptep, pte_t pteval)
{
	if (pte_present(pteval) && pte_exec(pteval))
		flush_icache_pte(mm, pteval);

	set_pte(ptep, pteval);
}

#define PFN_PTE_SHIFT		_PAGE_PFN_SHIFT

static inline void set_ptes(struct mm_struct *mm, unsigned long addr,
		pte_t *ptep, pte_t pteval, unsigned int nr)
{
	page_table_check_ptes_set(mm, addr, ptep, pteval, nr);

	for (;;) {
		__set_pte_at(mm, ptep, pteval);
		if (--nr == 0)
			break;
		ptep++;
		pte_val(pteval) += 1 << _PAGE_PFN_SHIFT;
	}
}
#define set_ptes set_ptes

static inline void pte_clear(struct mm_struct *mm,
	unsigned long addr, pte_t *ptep)
{
	__set_pte_at(mm, ptep, __pte(0));
}

#define __HAVE_ARCH_PTEP_SET_ACCESS_FLAGS	/* defined in mm/pgtable.c */
extern int ptep_set_access_flags(struct vm_area_struct *vma, unsigned long address,
				 pte_t *ptep, pte_t entry, int dirty);
#define __HAVE_ARCH_PTEP_TEST_AND_CLEAR_YOUNG	/* defined in mm/pgtable.c */
bool ptep_test_and_clear_young(struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep);

#define __HAVE_ARCH_PTEP_GET_AND_CLEAR
static inline pte_t ptep_get_and_clear(struct mm_struct *mm,
				       unsigned long address, pte_t *ptep)
{
#ifdef CONFIG_SMP
	pte_t pte = __pte(xchg(&ptep->pte, 0));
#else
	pte_t pte = *ptep;

	set_pte(ptep, __pte(0));
#endif

	page_table_check_pte_clear(mm, address, pte);

	return pte;
}

#define __HAVE_ARCH_PTEP_SET_WRPROTECT
static inline void ptep_set_wrprotect(struct mm_struct *mm,
				      unsigned long address, pte_t *ptep)
{
	pte_t read_pte = READ_ONCE(*ptep);
	/*
	 * ptep_set_wrprotect can be called for shadow stack ranges too.
	 * shadow stack memory is XWR = 010 and thus clearing _PAGE_WRITE will lead to
	 * encoding 000b which is wrong encoding with V = 1. This should lead to page fault
	 * but we dont want this wrong configuration to be set in page tables.
	 */
	atomic_long_set((atomic_long_t *)ptep,
			((pte_val(read_pte) & ~(unsigned long)_PAGE_WRITE) | _PAGE_READ));
}

#define __HAVE_ARCH_PTEP_CLEAR_YOUNG_FLUSH
static inline bool ptep_clear_flush_young(struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep)
{
	/*
	 * This comment is borrowed from x86, but applies equally to RISC-V:
	 *
	 * Clearing the accessed bit without a TLB flush
	 * doesn't cause data corruption. [ It could cause incorrect
	 * page aging and the (mistaken) reclaim of hot pages, but the
	 * chance of that should be relatively low. ]
	 *
	 * So as a performance optimization don't flush the TLB when
	 * clearing the accessed bit, it will eventually be flushed by
	 * a context switch or a VM operation anyway. [ In the rare
	 * event of it not getting flushed for a long time the delay
	 * shouldn't really matter because there's no real memory
	 * pressure for swapout to react to. ]
	 */
	return ptep_test_and_clear_young(vma, address, ptep);
}

#define pgprot_nx pgprot_nx
static inline pgprot_t pgprot_nx(pgprot_t _prot)
{
	return __pgprot(pgprot_val(_prot) & ~_PAGE_EXEC);
}

#define pgprot_noncached pgprot_noncached
static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot &= ~_PAGE_MTMASK;
	prot |= _PAGE_IO;

	return __pgprot(prot);
}

#define pgprot_writecombine pgprot_writecombine
static inline pgprot_t pgprot_writecombine(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);

	prot &= ~_PAGE_MTMASK;
	prot |= _PAGE_NOCACHE;

	return __pgprot(prot);
}

#define pgprot_dmacoherent pgprot_writecombine

#define __pmd_to_phys(pmd)  (__page_val_to_pfn(pmd_val(pmd)) << PAGE_SHIFT)

static inline unsigned long pmd_pfn(pmd_t pmd)
{
	return ((__pmd_to_phys(pmd) & PMD_MASK) >> PAGE_SHIFT);
}

/*
 * Encode/decode swap entries and swap PTEs. Swap PTEs are all PTEs that
 * are !pte_none() && !pte_present().
 *
 * Format of swap PTE:
 *	bit            0:	_PAGE_PRESENT (zero)
 *	bit       1 to 2:	(zero)
 *	bit            3:	_PAGE_SWP_SOFT_DIRTY
 *	bit            4:	_PAGE_SWP_UFFD
 *	bit            5:	_PAGE_PROT_NONE (zero)
 *	bit            6:	exclusive marker
 *	bits      7 to 11:	swap type
 *	bits 12 to XLEN-1:	swap offset
 */
#define __SWP_TYPE_SHIFT	7
#define __SWP_TYPE_BITS		5
#define __SWP_TYPE_MASK		((1UL << __SWP_TYPE_BITS) - 1)
#define __SWP_OFFSET_SHIFT	(__SWP_TYPE_BITS + __SWP_TYPE_SHIFT)

#define MAX_SWAPFILES_CHECK()	\
	BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > __SWP_TYPE_BITS)

#define __swp_type(x)	(((x).val >> __SWP_TYPE_SHIFT) & __SWP_TYPE_MASK)
#define __swp_offset(x)	((x).val >> __SWP_OFFSET_SHIFT)
#define __swp_entry(type, offset) ((swp_entry_t) \
	{ (((type) & __SWP_TYPE_MASK) << __SWP_TYPE_SHIFT) | \
	  ((offset) << __SWP_OFFSET_SHIFT) })

#define __pte_to_swp_entry(pte)	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(x)	((pte_t) { (x).val })

static inline bool pte_swp_exclusive(pte_t pte)
{
	return pte_val(pte) & _PAGE_SWP_EXCLUSIVE;
}

static inline pte_t pte_swp_mkexclusive(pte_t pte)
{
	return __pte(pte_val(pte) | _PAGE_SWP_EXCLUSIVE);
}

static inline pte_t pte_swp_clear_exclusive(pte_t pte)
{
	return __pte(pte_val(pte) & ~_PAGE_SWP_EXCLUSIVE);
}

/*
 * In the RV64 Linux scheme, we give the user half of the virtual-address space
 * and give the kernel the other (upper) half.
 */
#define KERN_VIRT_START	FIXADDR_START

/*
 * Task size is 0x4000000000 for RV64 or 0x9fc00000 for RV32.
 * Note that PGDIR_SIZE must evenly divide TASK_SIZE.
 * Task size is:
 * -        0x9fc00000	(~2.5GB) for RV32.
 * -      0x4000000000	( 256GB) for RV64 using SV39 mmu
 * -    0x800000000000	( 128TB) for RV64 using SV48 mmu
 * - 0x100000000000000	(  64PB) for RV64 using SV57 mmu
 *
 * Note that PGDIR_SIZE must evenly divide TASK_SIZE since "RISC-V
 * Instruction Set Manual Volume II: Privileged Architecture" states that
 * "load and store effective addresses, which are 64bits, must have bits
 * 63–48 all equal to bit 47, or else a page-fault exception will occur."
 * Similarly for SV57, bits 63–57 must be equal to bit 56.
 */
#define TASK_SIZE	FIXADDR_START

extern char _start[];
extern void *_dtb_early_va;
extern uintptr_t _dtb_early_pa;
#define dtb_early_va	_dtb_early_va
#define dtb_early_pa	_dtb_early_pa

void paging_init(void);
void misc_mem_init(void);

/*
 * Use set_p*_safe(), and elide TLB flushing, when confident that *no*
 * TLB flush will be required as a result of the "set". For example, use
 * in scenarios where it is known ahead of time that the routine is
 * setting non-present entries, or re-setting an existing entry to the
 * same value. Otherwise, use the typical "set" helpers and flush the
 * TLB.
 */

#define set_pgd_safe(pgdp, pgd) \
({ \
	WARN_ON_ONCE(pgd_present(*pgdp) && !pgd_same(*pgdp, pgd)); \
	set_pgd(pgdp, pgd); \
})
#endif /* !__ASSEMBLER__ */

#endif /* _ASM_RISCV_PGTABLE_H */
