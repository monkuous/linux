/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2009 Chen Liqin <liqin.chen@sunplusct.com>
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 * Copyright (C) 2017 XiaojingZhu <zhuxiaoj@ict.ac.cn>
 */

#ifndef _ASM_RISCV_PAGE_H
#define _ASM_RISCV_PAGE_H

#include <linux/pfn.h>
#include <linux/const.h>

#include <vdso/page.h>

/*
 * PAGE_OFFSET -- the first address of the first page of memory.
 * When not using MMU this corresponds to the first free page in
 * physical memory (aligned on a page boundary).
 */
#define PAGE_OFFSET		_AC(0xc0000000, UL)

#define PGTABLE_SIZE	 0x400000
#define PGTABLE_END	 PAGE_OFFSET
#define PGTABLE_START	 (PGTABLE_END - PGTABLE_SIZE)
#define PGDIR_START	 (PGTABLE_START | ((PGTABLE_START >> PAGE_SHIFT) << 2))

#ifndef __ASSEMBLER__

#define clear_page(pgaddr)			memset((pgaddr), 0, PAGE_SIZE)
#define copy_page(to, from)			memcpy((to), (from), PAGE_SIZE)

#define copy_user_page(vto, vfrom, vaddr, topg) copy_page(vto, vfrom)

/*
 * Use struct definitions to apply C type checking
 */

/* Page Global Directory entry */
typedef struct {
	unsigned long pgd;
} pgd_t;

/* Page Table entry */
typedef struct {
	unsigned long pte;
} pte_t;

typedef struct {
	unsigned long pgprot;
} pgprot_t;

typedef struct page *pgtable_t;

#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t) { (x) })
#define __pgd(x)	((pgd_t) { (x) })
#define __pgprot(x)	((pgprot_t) { (x) })

#define PTE_FMT "%08lx"

#define ARCH_PFN_OFFSET		(PFN_DOWN((unsigned long)phys_ram_base))

struct kernel_mapping {
	uintptr_t phys_addr;
	uintptr_t size;
	/* Offset between linear mapping virtual address and kernel load address */
	unsigned long va_pa_offset;
	/* Offset between kernel mapping virtual address and kernel load address */
	unsigned long page_offset;
	unsigned long va_kernel_pa_offset;
};

extern struct kernel_mapping kernel_map;
extern phys_addr_t phys_ram_base;

#define is_kernel_mapping(x)	\
	((x) >= KERNEL_LINK_ADDR && (x) < (KERNEL_LINK_ADDR + kernel_map.size))

#define is_linear_mapping(x)	\
	((x) >= PAGE_OFFSET)

#ifndef CONFIG_DEBUG_VIRTUAL
#define linear_mapping_pa_to_va(x)	((void *)((unsigned long)(x) + kernel_map.va_pa_offset))
#else
void *linear_mapping_pa_to_va(unsigned long x);
#endif

#define kernel_mapping_pa_to_va(y) ((void *)((unsigned long)(y) + kernel_map.va_kernel_pa_offset))

#define __pa_to_va_nodebug(x)		linear_mapping_pa_to_va(x)

#ifndef CONFIG_DEBUG_VIRTUAL
#define linear_mapping_va_to_pa(x)	((unsigned long)(x) - kernel_map.va_pa_offset)
#else
phys_addr_t linear_mapping_va_to_pa(unsigned long x);
#endif

#define kernel_mapping_va_to_pa(y) ((unsigned long)(y) - kernel_map.va_kernel_pa_offset)

#define __va_to_pa_nodebug(x)	({						\
	unsigned long _x = x;							\
	is_linear_mapping(_x) ?							\
		linear_mapping_va_to_pa(_x) : kernel_mapping_va_to_pa(_x);	\
	})

#ifdef CONFIG_DEBUG_VIRTUAL
extern phys_addr_t __virt_to_phys(unsigned long x);
extern phys_addr_t __phys_addr_symbol(unsigned long x);
#else
#define __virt_to_phys(x)	__va_to_pa_nodebug(x)
#define __phys_addr_symbol(x)	__va_to_pa_nodebug(x)
#endif /* CONFIG_DEBUG_VIRTUAL */

#define __pa_symbol(x)	__phys_addr_symbol(RELOC_HIDE((unsigned long)(x), 0))
#define __pa(x)		__virt_to_phys((unsigned long)(x))
#define __va(x)		((void *)__pa_to_va_nodebug((phys_addr_t)(x)))

#define phys_to_pfn(phys)	(PFN_DOWN(phys))
#define pfn_to_phys(pfn)	(PFN_PHYS(pfn))

#define virt_to_pfn(vaddr)	(phys_to_pfn(__pa(vaddr)))
#define pfn_to_virt(pfn)	(__va(pfn_to_phys(pfn)))

#define virt_to_page(vaddr)	(pfn_to_page(virt_to_pfn(vaddr)))
#define page_to_virt(page)	(pfn_to_virt(page_to_pfn(page)))

#define sym_to_pfn(x)           __phys_to_pfn(__pa_symbol(x))

static __always_inline void *pfn_to_kaddr(unsigned long pfn)
{
	return __va(pfn << PAGE_SHIFT);
}

#endif /* __ASSEMBLER__ */

#define virt_addr_valid(vaddr)	({						\
	unsigned long _addr = (unsigned long)vaddr;				\
	(unsigned long)(_addr) >= PAGE_OFFSET && pfn_valid(virt_to_pfn(_addr));	\
})

#define VMA_DATA_DEFAULT_FLAGS	VMA_DATA_FLAGS_NON_EXEC

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* _ASM_RISCV_PAGE_H */
