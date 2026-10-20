/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 StarFive Technology Co., Ltd.
 *
 * Author: Jee Heng Sia <jeeheng.sia@starfivetech.com>
 */

#ifndef __ASSEMBLER__
#error "Only include this from assembly code"
#endif

#ifndef __ASM_ASSEMBLER_H
#define __ASM_ASSEMBLER_H

#include <asm/asm.h>
#include <asm/asm-offsets.h>

/*
 * copy_page - copy 1 page (4KB) of data from source to destination
 * @a0 - destination
 * @a1 - source
 */
	.macro	copy_page a0, a1
		lui	a2, 0x1
		add	a2, a2, a0
1 :
		REG_L	t0, 0(a1)
		REG_L	t1, SZREG(a1)

		REG_S	t0, 0(a0)
		REG_S	t1, SZREG(a0)

		addi	a0, a0, 2 * SZREG
		addi	a1, a1, 2 * SZREG
		bne	a2, a0, 1b
	.endm

#endif	/* __ASM_ASSEMBLER_H */
