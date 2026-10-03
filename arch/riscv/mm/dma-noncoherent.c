// SPDX-License-Identifier: GPL-2.0-only
/*
 * RISC-V specific functions to support DMA for non-coherent devices
 *
 * Copyright (c) 2021 Western Digital Corporation or its affiliates.
 */

#include <linux/dma-direct.h>
#include <linux/dma-map-ops.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>

#define CMO_OP(_op, _start, _size, _cachesize)				\
asm volatile(								\
	"mv a0, %1\n\t"							\
	"j 2f\n\t"							\
	"3:\n\t"							\
	".option push\n\t"						\
	".option arch,+zicbom\n\t"					\
	_op "(a0)\n\t"							\
	".option pop\n\t"						\
	"add a0, a0, %0\n\t"						\
	"2:\n\t"							\
	"bltu a0, %2, 3b\n\t"						\
	: : "r"(_cachesize),						\
	    "r"((unsigned long)(_start) & ~((_cachesize) - 1UL)),	\
	    "r"((unsigned long)(_start) + (_size))			\
	: "a0")

static inline void arch_dma_cache_wback(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	CMO_OP("cbo.clean", vaddr, size, riscv_cbom_block_size);
}

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	CMO_OP("cbo.inval", vaddr, size, riscv_cbom_block_size);
}

static inline void arch_dma_cache_wback_inv(phys_addr_t paddr, size_t size)
{
	void *vaddr = phys_to_virt(paddr);

	CMO_OP("cbo.flush", vaddr, size, riscv_cbom_block_size);
}

static inline bool arch_sync_dma_clean_before_fromdevice(void)
{
	return true;
}

static inline bool arch_sync_dma_cpu_needs_post_dma_flush(void)
{
	return true;
}

void arch_sync_dma_for_device(phys_addr_t paddr, size_t size,
			      enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		arch_dma_cache_wback(paddr, size);
		break;

	case DMA_FROM_DEVICE:
		if (!arch_sync_dma_clean_before_fromdevice()) {
			arch_dma_cache_inv(paddr, size);
			break;
		}
		fallthrough;

	case DMA_BIDIRECTIONAL:
		/* Skip the invalidate here if it's done later */
		if (arch_sync_dma_cpu_needs_post_dma_flush())
			arch_dma_cache_wback(paddr, size);
		else
			arch_dma_cache_wback_inv(paddr, size);
		break;

	default:
		break;
	}
}

void arch_sync_dma_for_cpu(phys_addr_t paddr, size_t size,
			   enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_TO_DEVICE:
		break;

	case DMA_FROM_DEVICE:
	case DMA_BIDIRECTIONAL:
		/* FROM_DEVICE invalidate needed if speculative CPU prefetch only */
		if (arch_sync_dma_cpu_needs_post_dma_flush())
			arch_dma_cache_inv(paddr, size);
		break;

	default:
		break;
	}
}

void arch_dma_prep_coherent(struct page *page, size_t size)
{
	void *flush_addr = page_address(page);

	CMO_OP("cbo.flush", flush_addr, size, riscv_cbom_block_size);
}

void arch_setup_dma_ops(struct device *dev, bool coherent)
{
	WARN_TAINT(!coherent && riscv_cbom_block_size > ARCH_DMA_MINALIGN,
		   TAINT_CPU_OUT_OF_SPEC,
		   "%s %s: ARCH_DMA_MINALIGN smaller than riscv,cbom-block-size (%d < %d)",
		   dev_driver_string(dev), dev_name(dev),
		   ARCH_DMA_MINALIGN, riscv_cbom_block_size);

	dev_assign_dma_coherent(dev, coherent);
}
