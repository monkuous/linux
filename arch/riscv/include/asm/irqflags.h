/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */


#ifndef _ASM_RISCV_IRQFLAGS_H
#define _ASM_RISCV_IRQFLAGS_H

#include <asm/csr.h>

/* read interrupt enabled status */
static inline unsigned long arch_local_save_flags(void)
{
	return csr_read(CSR_RS);
}

/* unconditionally enable interrupts */
static inline void arch_local_irq_enable(void)
{
	unsigned long flags = csr_read(CSR_RS);
	flags |= RS_I;
	csr_write(CSR_RS, flags);
}

/* unconditionally disable interrupts */
static inline void arch_local_irq_disable(void)
{
	unsigned long flags = csr_read(CSR_RS);
	flags &= ~RS_I;
	csr_write(CSR_RS, flags);
}

/* get status and disable interrupts */
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags = csr_read(CSR_RS);
	csr_write(CSR_RS, flags & ~RS_I);
	return flags;
}

/* test flags */
static inline int arch_irqs_disabled_flags(unsigned long flags)
{
	return !(flags & RS_I);
}

/* test hardware interrupt enable bit */
static inline int arch_irqs_disabled(void)
{
	return arch_irqs_disabled_flags(arch_local_save_flags());
}

/* set interrupt enabled status */
static inline void arch_local_irq_restore(unsigned long flags)
{
	/*
	 * This also resets other flags in RS, but they're all either constant
	 * across all points in the kernel that can use arch_local_irq_save()
	 * (U, M, T), or not cared about (ECAUSE), or restored before being used
	 * (mode stack).
	 */
	csr_write(CSR_RS, flags);
}

#endif /* _ASM_RISCV_IRQFLAGS_H */
