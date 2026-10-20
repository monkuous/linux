// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2019 Western Digital Corporation or its affiliates.
 * Copyright (C) 2020 FORTH-ICS/CARV
 *  Nick Kossifidis <mick@ics.forth.gr>
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#include <linux/initrd.h>
#include <linux/swap.h>
#include <linux/swiotlb.h>
#include <linux/sizes.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/libfdt.h>
#include <linux/set_memory.h>
#include <linux/dma-map-ops.h>
#include <linux/crash_dump.h>
#include <linux/hugetlb.h>
#include <linux/kfence.h>
#include <linux/execmem.h>

#include <asm/fixmap.h>
#include <asm/io.h>
#include <asm/module.h>
#include <asm/numa.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/sparsemem.h>
#include <asm/tlbflush.h>

#include "../kernel/head.h"

struct kernel_mapping kernel_map __ro_after_init;
EXPORT_SYMBOL(kernel_map);

phys_addr_t phys_ram_base __ro_after_init;
EXPORT_SYMBOL(phys_ram_base);

extern char _start[];
void *_dtb_early_va __initdata;
uintptr_t _dtb_early_pa __initdata;

phys_addr_t dma32_phys_limit __initdata;

void __init arch_zone_limits_init(unsigned long *max_zone_pfns)
{
	max_zone_pfns[ZONE_NORMAL] = max_low_pfn;
}

#if defined(CONFIG_DEBUG_VM)

#define LOG2_SZ_1K  ilog2(SZ_1K)
#define LOG2_SZ_1M  ilog2(SZ_1M)
#define LOG2_SZ_1G  ilog2(SZ_1G)

static inline void print_mlk(char *name, unsigned long b, unsigned long t)
{
	pr_notice("%12s : 0x%08lx - 0x%08lx   (%4ld kB)\n", name, b, t,
		  (((t) - (b)) >> LOG2_SZ_1K));
}

static inline void print_mlm(char *name, unsigned long b, unsigned long t)
{
	pr_notice("%12s : 0x%08lx - 0x%08lx   (%4ld MB)\n", name, b, t,
		  (((t) - (b)) >> LOG2_SZ_1M));
}

static inline void print_mlg(char *name, unsigned long b, unsigned long t)
{
	pr_notice("%12s : 0x%08lx - 0x%08lx   (%4ld GB)\n", name, b, t,
		   (((t) - (b)) >> LOG2_SZ_1G));
}

static inline void print_ml(char *name, unsigned long b, unsigned long t)
{
	unsigned long diff = t - b;

	if ((diff >> LOG2_SZ_1G) >= 10)
		print_mlg(name, b, t);
	else if ((diff >> LOG2_SZ_1M) >= 10)
		print_mlm(name, b, t);
	else
		print_mlk(name, b, t);
}

static void __init print_vm_layout(void)
{
	pr_notice("Virtual kernel memory layout:\n");
	print_ml("fixmap", (unsigned long)FIXADDR_START,
		(unsigned long)FIXADDR_TOP);
	print_ml("pci io", (unsigned long)PCI_IO_START,
		(unsigned long)PCI_IO_END);
	print_ml("vmalloc", (unsigned long)VMALLOC_START,
		(unsigned long)VMALLOC_END);
	print_ml("lowmem", (unsigned long)PAGE_OFFSET,
		(unsigned long)high_memory);
}
#else
static void print_vm_layout(void) { }
#endif /* CONFIG_DEBUG_VM */

void __init arch_mm_preinit(void)
{
	bool swiotlb = max_pfn > PFN_DOWN(dma32_phys_limit) &&
		       memblock_start_of_DRAM() < dma32_phys_limit;
	unsigned int swiotlb_flags = SWIOTLB_VERBOSE;
#ifdef CONFIG_FLATMEM
	BUG_ON(!mem_map);
#endif /* CONFIG_FLATMEM */

	swiotlb_init(swiotlb, swiotlb_flags);

	print_vm_layout();
}

/* Limit the memory size via mem. */
static phys_addr_t memory_limit;

static int __init early_mem(char *p)
{
	u64 size;

	if (!p)
		return 1;

	size = memparse(p, &p) & PAGE_MASK;
	memory_limit = min_t(u64, size, memory_limit);

	pr_notice("Memory limited to %lldMB\n", (u64)memory_limit >> 20);

	return 0;
}
early_param("mem", early_mem);

static void __init setup_bootmem(void)
{
	phys_addr_t vmlinux_end = __pa_symbol(&_end);
	phys_addr_t max_mapped_addr;
	phys_addr_t phys_ram_end, vmlinux_start;

	vmlinux_start = __pa_symbol(&_start);

	memblock_enforce_memory_limit(memory_limit);
	/*
	 * Reserve from the start of the kernel to the end of the kernel
	 */
	memblock_reserve(vmlinux_start, vmlinux_end - vmlinux_start);

	/*
	 * Make sure we align the start of the memory on a Ppage boundary.
	 */
	phys_ram_base = memblock_start_of_DRAM() & PAGE_MASK;

	/*
	 * Reserve physical address space that would be mapped to virtual
	 * addresses greater than (void *)(-PAGE_SIZE) because:
	 *  - This memory would overlap with ERR_PTR
	 *  - This memory belongs to high memory, which is not supported
	 */
	max_mapped_addr = __va_to_pa_nodebug(-PAGE_SIZE);
	if (memblock_end_of_DRAM() > max_mapped_addr) {
		memblock_cap_memory_range(phys_ram_base,
					  max_mapped_addr - phys_ram_base);
		pr_warn("Physical memory overflows the linear mapping size: "
			"region above %pa removed\n", &max_mapped_addr);
	}

	phys_ram_end = memblock_end_of_DRAM();
	min_low_pfn = PFN_UP(phys_ram_base);
	max_low_pfn = max_pfn = PFN_DOWN(phys_ram_end);

	dma32_phys_limit = min(4UL * SZ_1G, (unsigned long)PFN_PHYS(max_low_pfn));

	reserve_initrd_mem();

	/*
	 * No allocation should be done before reserving the memory as defined
	 * in the device tree, otherwise the allocation could end up in a
	 * reserved region.
	 */
	early_init_fdt_scan_reserved_mem();

	/*
	 * If DTB is built in, no need to reserve its memblock.
	 * Otherwise, do reserve it but avoid using
	 * early_init_fdt_reserve_self() since __pa() does
	 * not work for DTB pointers that are fixmap addresses
	 */
	memblock_reserve(dtb_early_pa, fdt_totalsize(dtb_early_va));
}

struct pt_alloc_ops pt_ops __meminitdata;

pgd_t swapper_pg_dir[PTRS_PER_PGD] __page_aligned_bss;
pgd_t trampoline_pg_dir[PTRS_PER_PGD] __page_aligned_bss;
static pte_t fixmap_pte[PTRS_PER_PTE] __page_aligned_bss;
static pte_t fdt_pte[PTRS_PER_PTE] __page_aligned_bss;

/*
 * Statically allocate enough PTEs to map the kernel.
 */
#define NUM_EARLY_PTES 16

pgd_t early_pg_dir[PTRS_PER_PGD] __initdata __aligned(PAGE_SIZE);
pte_t early_pte[NUM_EARLY_PTES][PTRS_PER_PTE] __initdata __aligned(PAGE_SIZE);

static const pgprot_t protection_map[16] = {
	[VM_NONE]					= PAGE_NONE,
	[VM_READ]					= PAGE_READ,
	[VM_WRITE]					= PAGE_SHADOWSTACK,
	[VM_WRITE | VM_READ]				= PAGE_COPY,
	[VM_EXEC]					= PAGE_EXEC,
	[VM_EXEC | VM_READ]				= PAGE_READ_EXEC,
	[VM_EXEC | VM_WRITE]				= PAGE_COPY_EXEC,
	[VM_EXEC | VM_WRITE | VM_READ]			= PAGE_COPY_EXEC,
	[VM_SHARED]					= PAGE_NONE,
	[VM_SHARED | VM_READ]				= PAGE_READ,
	[VM_SHARED | VM_WRITE]				= PAGE_SHARED,
	[VM_SHARED | VM_WRITE | VM_READ]		= PAGE_SHARED,
	[VM_SHARED | VM_EXEC]				= PAGE_EXEC,
	[VM_SHARED | VM_EXEC | VM_READ]			= PAGE_READ_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE]		= PAGE_SHARED_EXEC,
	[VM_SHARED | VM_EXEC | VM_WRITE | VM_READ]	= PAGE_SHARED_EXEC
};
DECLARE_VM_GET_PAGE_PROT

void __set_fixmap(enum fixed_addresses idx, phys_addr_t phys, pgprot_t prot)
{
	unsigned long addr = __fix_to_virt(idx);
	pte_t *ptep;

	BUG_ON(idx <= FIX_HOLE || idx >= __end_of_fixed_addresses);

	ptep = &fixmap_pte[pte_index(addr)];

	if (pgprot_val(prot))
		set_pte(ptep, pfn_pte(phys >> PAGE_SHIFT, prot));
	else
		pte_clear(&init_mm, addr, ptep);
	local_flush_tlb_page(addr);
}

static inline pte_t *__init get_pte_virt_early(phys_addr_t pa)
{
	return (pte_t *)((uintptr_t)pa);
}

static inline pte_t *__init get_pte_virt_fixmap(phys_addr_t pa)
{
	clear_fixmap(FIX_PTE);
	return (pte_t *)set_fixmap_offset(FIX_PTE, pa);
}

static inline pte_t *__meminit get_pte_virt_late(phys_addr_t pa)
{
	return (pte_t *) __va(pa);
}

static inline phys_addr_t __init alloc_pte_early(uintptr_t va)
{
	static size_t next = 0;

	/*
	 * We allocate enough PTEs that a reasonably sized kernel won't run out.
	 */
	if (next >= NUM_EARLY_PTES)
		BUG();

	return (uintptr_t)early_pte[next++];
}

static inline phys_addr_t __init alloc_pte_fixmap(uintptr_t va)
{
	return memblock_phys_alloc(PAGE_SIZE, PAGE_SIZE);
}

static phys_addr_t __meminit alloc_pte_late(uintptr_t va)
{
	struct ptdesc *ptdesc = pagetable_alloc(GFP_KERNEL, 0);

	/*
	 * We do not know which mm the PTE page is associated to at this point.
	 * Passing NULL to the ctor is the safe option, though it may result
	 * in unnecessary work (e.g. initialising the ptlock for init_mm).
	 */
	BUG_ON(!ptdesc || !pagetable_pte_ctor(NULL, ptdesc));
	return __pa((pte_t *)ptdesc_address(ptdesc));
}

static void __meminit create_pte_mapping(pte_t *ptep, uintptr_t va, phys_addr_t pa,
					 pgprot_t prot)
{
	uintptr_t pte_idx = pte_index(va);

	if (pte_none(ptep[pte_idx]))
		ptep[pte_idx] = pfn_pte(PFN_DOWN(pa), prot);
}

#define pgd_next_t		pte_t
#define alloc_pgd_next(__va)	pt_ops.alloc_pte(__va)
#define get_pgd_next_virt(__pa)	pt_ops.get_pte_virt(__pa)
#define fixmap_pgd_next		((uintptr_t)fixmap_pte)

static void __meminit create_pgd_mapping(pgd_t *pgdp, uintptr_t va, phys_addr_t pa,
				  	 pgprot_t prot)
{
	pgd_next_t *nextp;
	phys_addr_t next_phys;
	uintptr_t pgd_idx = pgd_index(va);

	if (pgd_val(pgdp[pgd_idx]) == 0) {
		next_phys = alloc_pgd_next(va);
		pgdp[pgd_idx] = pfn_pgd(PFN_DOWN(next_phys), PAGE_TABLE);
		nextp = get_pgd_next_virt(next_phys);
		memset(nextp, 0, PAGE_SIZE);
	} else {
		next_phys = PFN_PHYS(_pgd_pfn(pgdp[pgd_idx]));
		nextp = get_pgd_next_virt(next_phys);
	}

	create_pte_mapping(nextp, va, pa, prot);
}

#ifdef CONFIG_STRICT_KERNEL_RWX
static __meminit pgprot_t pgprot_from_va(uintptr_t va)
{
	if (is_va_kernel_text(va))
		return PAGE_KERNEL_READ_EXEC;

	return PAGE_KERNEL;
}

void mark_rodata_ro(void)
{
	set_kernel_memory(__start_rodata, _data, set_memory_ro);
}
#else
static __meminit pgprot_t pgprot_from_va(uintptr_t va)
{
	return PAGE_KERNEL_EXEC;
}
#endif /* CONFIG_STRICT_KERNEL_RWX */

/*
 * setup_vm() is called from head.S with MMU-off.
 *
 * Following requirements should be honoured for setup_vm() to work
 * correctly:
 * 1) It should use PC-relative addressing for accessing kernel symbols.
 *    To achieve this we always use GCC cmodel=medany.
 * 2) The compiler instrumentation for FTRACE will not work for setup_vm()
 *    so disable compiler instrumentation when FTRACE is enabled.
 *
 * Currently, the above requirements are honoured by using custom CFLAGS
 * for init.o in mm/Makefile.
 */

#ifndef __riscv_cmodel_medany
#error "setup_vm() is called from head.S before relocate so it should not use absolute addressing."
#endif

static void __init create_kernel_page_table(pgd_t *pgdir, bool early)
{
	uintptr_t va, end_va;

	end_va = KERNEL_LINK_ADDR + kernel_map.size;
	for (va = KERNEL_LINK_ADDR; va < end_va; va += PAGE_SIZE)
		create_pgd_mapping(pgdir, va,
				   kernel_map.phys_addr + (va - KERNEL_LINK_ADDR),
				   early ?
					PAGE_KERNEL_EXEC : pgprot_from_va(va));
}

/*
 * Setup a 4MB mapping that encompasses the device tree.
 */
static void __init create_fdt_early_page_table(uintptr_t fix_fdt_va,
					       uintptr_t dtb_pa)
{
	uintptr_t pa = dtb_pa & ~(PAGE_SIZE - 1);
	unsigned long idx, offset;

	/* Make sure the fdt fixmap address is always aligned on PMD size */
	BUILD_BUG_ON(FIX_FDT % (PMD_SIZE / PAGE_SIZE));

	idx = pgd_index(fix_fdt_va);
	early_pg_dir[idx] = pfn_pgd(PFN_DOWN((uintptr_t)fdt_pte), PAGE_TABLE);

	for (offset = 0; offset < PMD_SIZE; offset += PAGE_SIZE) {
		create_pgd_mapping(early_pg_dir, fix_fdt_va + offset,
				   pa + offset, PAGE_KERNEL);
	}

	dtb_early_va = (void *)fix_fdt_va + (dtb_pa & (PAGE_SIZE - 1));

	dtb_early_pa = dtb_pa;
}

/*
 * MMU is not enabled, the page tables are allocated directly using
 * early_pmd/pud/p4d and the address returned is the physical one.
 */
static void __init pt_ops_set_early(void)
{
	pt_ops.alloc_pte = alloc_pte_early;
	pt_ops.get_pte_virt = get_pte_virt_early;
}

/*
 * MMU is enabled but page table setup is not complete yet.
 * fixmap page table alloc functions must be used as a means to temporarily
 * map the allocated physical pages since the linear mapping does not exist yet.
 *
 * Note that this is called with MMU disabled, hence kernel_mapping_pa_to_va,
 * but it will be used as described above.
 */
static void __init pt_ops_set_fixmap(void)
{
	pt_ops.alloc_pte = kernel_mapping_pa_to_va(alloc_pte_fixmap);
	pt_ops.get_pte_virt = kernel_mapping_pa_to_va(get_pte_virt_fixmap);
}

/*
 * MMU is enabled and page table setup is complete, so from now, we can use
 * generic page allocation functions to setup page table.
 */
static void __init pt_ops_set_late(void)
{
	pt_ops.alloc_pte = alloc_pte_late;
	pt_ops.get_pte_virt = get_pte_virt_late;
}

asmlinkage void __init setup_vm(uintptr_t dtb_pa)
{
	pmd_t __maybe_unused fix_bmap_spmd, fix_bmap_epmd;
	unsigned long idx;

	kernel_map.phys_addr = (uintptr_t)(&_start);
	kernel_map.size = (uintptr_t)(&_end) - kernel_map.phys_addr;
	kernel_map.va_kernel_pa_offset = KERNEL_LINK_ADDR - kernel_map.phys_addr;
	kernel_map.va_pa_offset = PAGE_OFFSET - kernel_map.phys_addr;

	memory_limit = KERN_VIRT_SIZE;

	/* Sanity check alignment and size */
	BUG_ON((PAGE_OFFSET % PGDIR_SIZE) != 0);
	BUG_ON((kernel_map.phys_addr % PMD_SIZE) != 0);

	pt_ops_set_early();

	/* Setup early PGD for fixmap */
	idx = pgd_index(FIXADDR_START);
	early_pg_dir[idx] = pfn_pgd(PFN_DOWN(fixmap_pgd_next), PAGE_TABLE);

	/* Setup trampoline PTEs */
	create_pgd_mapping(trampoline_pg_dir, kernel_map.phys_addr,
			   kernel_map.phys_addr, PAGE_KERNEL_EXEC);
	create_pgd_mapping(trampoline_pg_dir, KERNEL_LINK_ADDR,
			   kernel_map.phys_addr, PAGE_KERNEL_EXEC);

	/*
	 * Setup early PTEs covering entire kernel which will allow
	 * us to reach paging_init(). We map all memory banks later
	 * in setup_vm_final() below.
	 */
	create_kernel_page_table(early_pg_dir, true);

	/* Setup early mapping for FDT early scan */
	create_fdt_early_page_table(__fix_to_virt(FIX_FDT), dtb_pa);

	/*
	 * Bootime fixmap only can handle PMD_SIZE mapping. Thus, boot-ioremap
	 * range can not span multiple pmds.
	 */
	BUG_ON((__fix_to_virt(FIX_BTMAP_BEGIN) >> PMD_SHIFT)
		     != (__fix_to_virt(FIX_BTMAP_END) >> PMD_SHIFT));

	pt_ops_set_fixmap();
}

static void __meminit create_linear_mapping_range(phys_addr_t start,
						  phys_addr_t end)
{
	phys_addr_t pa = start;
	uintptr_t va = (uintptr_t)__va(pa);
	unsigned long pgd_idx = pgd_index(va);
	unsigned long pte_idx = pte_index(va);

	while (pgd_idx < PTRS_PER_PGD && pa < end) {
		pte_t *ptep;
		phys_addr_t next_phys;

		if (pgd_val(swapper_pg_dir[pgd_idx]) == 0) {
			next_phys = alloc_pgd_next(va);
			swapper_pg_dir[pgd_idx] = pfn_pgd(PFN_DOWN(next_phys),
							  PAGE_TABLE);
			ptep = get_pgd_next_virt(next_phys);
			memset(ptep, 0, PAGE_SIZE);
		} else {
			next_phys = PFN_PHYS(_pgd_pfn(swapper_pg_dir[pgd_idx]));
			ptep = get_pgd_next_virt(next_phys);
		}

		while (pte_idx < PTRS_PER_PTE && pa < end) {
			if (pte_none(ptep[pte_idx]))
				ptep[pte_idx] = pfn_pte(PFN_DOWN(pa),
							pgprot_from_va(va));

			va += PAGE_SIZE;
			pa += PAGE_SIZE;
			pte_idx += 1;
		}

		pte_idx = 0;
		pgd_idx += 1;
	}
}

static void __init create_linear_mapping_page_table(void)
{
	phys_addr_t start, end;
	phys_addr_t kfence_pool __maybe_unused;
	u64 i;

#ifdef CONFIG_STRICT_KERNEL_RWX
	phys_addr_t ktext_start = __pa_symbol(_start);
	phys_addr_t ktext_size = __init_data_begin - _start;
	phys_addr_t krodata_start = __pa_symbol(__start_rodata);
	phys_addr_t krodata_size = _data - __start_rodata;

	/* Isolate kernel text and rodata so they don't get mapped with a PUD */
	memblock_mark_nomap(ktext_start,  ktext_size);
	memblock_mark_nomap(krodata_start, krodata_size);
#endif

	/* Map all memory banks in the linear mapping */
	for_each_mem_range(i, &start, &end) {
		if (start <= __pa(PAGE_OFFSET) &&
		    __pa(PAGE_OFFSET) < end)
			start = __pa(PAGE_OFFSET);

		create_linear_mapping_range(start, end);
	}

#ifdef CONFIG_STRICT_KERNEL_RWX
	create_linear_mapping_range(ktext_start, ktext_start + ktext_size);
	create_linear_mapping_range(krodata_start, krodata_start + krodata_size);

	memblock_clear_nomap(ktext_start,  ktext_size);
	memblock_clear_nomap(krodata_start, krodata_size);
#endif
}

static void __init setup_vm_final(void)
{
	/*
	 * The device tree lies in a pgd entry, so it must be copied
	 * directly in swapper_pg_dir in addition to the pgd entry that points
	 * to fixmap_pte.
	 */
	unsigned long idx = pgd_index(__fix_to_virt(FIX_FDT));
	set_pgd(&swapper_pg_dir[idx], early_pg_dir[idx]);

	/* Setup swapper PGD for fixmap */
	idx = pgd_index(FIXADDR_START);
	set_pgd(&swapper_pg_dir[idx], early_pg_dir[idx]);

	/* Map the linear mapping */
	create_linear_mapping_page_table();

	/* Clear fixmap PTE and PMD mappings */
	clear_fixmap(FIX_PTE);
	clear_fixmap(FIX_PMD);

	/* Move to swapper page table */
	csr_write(CSR_DTBADDR, PFN_DOWN(__pa_symbol(swapper_pg_dir)));
	local_flush_tlb_all();

	pt_ops_set_late();
}

/*
 * reserve_crashkernel() - reserves memory for crash kernel
 *
 * This function reserves memory area given in "crashkernel=" kernel command
 * line parameter. The memory reserved is used by dump capture kernel when
 * primary kernel is crashing.
 */
static void __init arch_reserve_crashkernel(void)
{
	unsigned long long low_size = 0, cma_size = 0;
	unsigned long long crash_base, crash_size;
	bool high = false;
	int ret;

	if (!IS_ENABLED(CONFIG_CRASH_RESERVE))
		return;

	ret = parse_crashkernel(boot_command_line, memblock_phys_mem_size(),
				&crash_size, &crash_base,
				&low_size, &cma_size, &high);
	if (ret)
		return;

	reserve_crashkernel_generic(crash_size, crash_base, low_size, high);
	reserve_crashkernel_cma(cma_size);
}

void __init paging_init(void)
{
	setup_bootmem();
	setup_vm_final();

	/* Depend on that Linear Mapping is ready */
	memblock_allow_resize();
}

void __init misc_mem_init(void)
{
	early_memtest(min_low_pfn << PAGE_SHIFT, max_low_pfn << PAGE_SHIFT);
	arch_numa_init();
	dma_contiguous_reserve(dma32_phys_limit);
	arch_reserve_crashkernel();
	memblock_dump_all();
}

#ifdef CONFIG_EXECMEM
static struct execmem_info execmem_info __ro_after_init;

struct execmem_info __init *execmem_arch_setup(void)
{
	execmem_info = (struct execmem_info){
		.ranges = {
			[EXECMEM_DEFAULT] = {
				.start	= MODULES_VADDR,
				.end	= MODULES_END,
				.pgprot	= PAGE_KERNEL,
				.alignment = 1,
			},
			[EXECMEM_KPROBES] = {
				.start	= VMALLOC_START,
				.end	= VMALLOC_END,
				.pgprot	= IS_ENABLED(CONFIG_STRICT_MODULE_RWX) ?
					  PAGE_KERNEL_READ_EXEC :
					  PAGE_KERNEL_EXEC,
				.alignment = 1,
			},
		},
	};

	return &execmem_info;
}
#endif /* CONFIG_EXECMEM */
