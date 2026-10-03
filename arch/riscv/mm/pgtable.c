// SPDX-License-Identifier: GPL-2.0

#include <asm/pgalloc.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/pgtable.h>

int ptep_set_access_flags(struct vm_area_struct *vma,
			  unsigned long address, pte_t *ptep,
			  pte_t entry, int dirty)
{
	if (!pte_same(ptep_get(ptep), entry))
		__set_pte_at(vma->vm_mm, ptep, entry);
	/*
	 * update_mmu_cache will unconditionally execute, handling both
	 * the case that the PTE changed and the spurious fault case.
	 */
	return true;
}

bool ptep_test_and_clear_young(struct vm_area_struct *vma,
		unsigned long address, pte_t *ptep)
{
	if (!pte_young(ptep_get(ptep)))
		return false;
	return test_and_clear_bit(_PAGE_ACCESSED_OFFSET, &pte_val(*ptep));
}
EXPORT_SYMBOL_GPL(ptep_test_and_clear_young);

pte_t pte_mkwrite(pte_t pte, struct vm_area_struct *vma)
{
	if (vma->vm_flags & VM_SHADOW_STACK)
		return pte_mkwrite_shstk(pte);

	return pte_mkwrite_novma(pte);
}
