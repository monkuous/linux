/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015 Regents of the University of California
 */

#ifndef _ASM_RISCV_CSR_H
#define _ASM_RISCV_CSR_H

#include <asm/asm.h>
#include <linux/bits.h>

/* Status register flags */
#define RS_I			_AC(0x00000002, UL) /* Interrupt enable */
#define RS_M			_AC(0x00000004, UL) /* MMU enable */
#define RS_PU			_AC(0x00000100, UL) /* Previously User mode */
#define RS_PI			_AC(0x00000200, UL) /* Previous Interrupt enable */
#define RS_PM			_AC(0x00000400, UL) /* Previous MMU enable */

/* TBCTRL flags */
#define TBCTRL_FLUSH_ONE	_AC(0, UL)
#define TBCTRL_FLUSH_NON_WIRED	_AC(1, UL)
#define TBCTRL_FLUSH_PRIVATE	_AC(2, UL)
#define TBCTRL_FLUSH_ALL	_AC(3, UL)

#define TBCTRL_VPN_SHIFT	12
#define TBCTRL_VPN_MASK		_AC(0xFFFFF, UL)

/* CACHECTRL flags */
#define CACHECTRL_LINES_SHIFT	0
#define CACHECTRL_LINES_MASK	_AC(0xFF, UL)
#define CACHECTRL_WAYS_SHIFT	8
#define CACHECTRL_WAYS_MASK	_AC(0xFF, UL)
#define CACHECTRL_LSIZE_SHIFT	16
#define CACHECTRL_LSIZE_MASK	_AC(0xFF, UL)

#define CACHECTRL_INVAL_PAGE	_AC(0, UL)
#define CACHECTRL_INVAL_ALL	_AC(3, UL)

#define CACHECTRL_PPN_SHIFT	12
#define CACHECTRL_PPN_MASK	_AC(0xFFFFF, UL)

/* TBTAG flags */
#define TBTAG_VPN_SHIFT		0
#define TBTAG_VPN_MASK		_AC(0xFFFFF, UL)
#define TBTAG_ASID_BITS		12
#define TBTAG_ASID_SHIFT	20
#define TBTAG_ASID_MASK		_AC(0xFFF, UL)

/* Special TBINDEX values */
#define TBINDEX_NUM_WIRED	4

/* Exception causes */
#define EXC_INT			1
#define EXC_SYS			2
#define EXC_BUS			4
#define EXC_NMI			5
#define EXC_BRK			6
#define EXC_INV			7
#define EXC_PRV			8
#define EXC_UNA			9
#define EXC_PGF			12
#define EXC_PFW			13

/* symbolic CSR names: */
#define CSR_RS			0
#define CSR_WHAMI		1
#define CSR_EB			5
#define CSR_EPC			6
#define CSR_EBADADDR		7
#define CSR_TBMISSADDR		9
#define CSR_TBPC		10
#define CSR_SCRATCH0		11
#define CSR_SCRATCH1		12
#define CSR_SCRATCH2		13
#define CSR_SCRATCH3		14
#define CSR_SCRATCH4		15
#define CSR_ITBPTE		16
#define CSR_ITBTAG		17
#define CSR_ITBINDEX		18
#define CSR_ITBCTRL		19
#define CSR_ICACHECTRL		20
#define CSR_ITBADDR		21
#define CSR_DTBPTE		24
#define CSR_DTBTAG		25
#define CSR_DTBINDEX		26
#define CSR_DTBCTRL		27
#define CSR_DCACHECTRL		28
#define CSR_DTBADDR		29

#ifndef __ASSEMBLER__

#define csr_read(csr)						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("csrr %0, " __ASM_STR(csr)	\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define csr_write(csr, val)					\
({								\
	unsigned long __v = (unsigned long)(val);		\
	__asm__ __volatile__ ("csrw " __ASM_STR(csr) ", %0"	\
			      : : "rK" (__v)			\
			      : "memory");			\
})

#endif /* __ASSEMBLER__ */

#endif /* _ASM_RISCV_CSR_H */
