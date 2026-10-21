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
#include <asm/csr.h>

static inline void arch_dma_cache_inv(phys_addr_t paddr, size_t size)
{
	size += paddr % PAGE_SIZE;

	for (size_t offset = 0; offset < size; offset += PAGE_SIZE) {
		unsigned long ppn = (paddr + offset) >> PAGE_SHIFT;
		csr_write(CSR_DCACHECTRL, (ppn << CACHECTRL_PPN_SHIFT)
					  | CACHECTRL_INVAL_PAGE);
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
		arch_dma_cache_inv(paddr, size);
		break;

	default:
		break;
	}
}

void arch_setup_dma_ops(struct device *dev, bool coherent)
{
	dev_assign_dma_coherent(dev, coherent);
}
