/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Base on arch/riscv/lib/strlen.S
 *
 * Copyright (C) Feng Jiang <jiangfeng@kylinos.cn>
 */

#include <linux/linkage.h>
#include <asm/asm.h>

/* size_t strnlen(const char *s, size_t count) */
SYM_FUNC_START(strnlen)


	/*
	 * Returns
	 *   a0 - String length
	 *
	 * Parameters
	 *   a0 - String to measure
	 *   a1 - Max length of string
	 *
	 * Clobbers
	 *   t0, t1, t2
	 */
	addi	t1, a0, -1
	add	t2, a0, a1
1:
	addi	t1, t1, 1
	beq	t1, t2, 2f
	lbu	t0, 0(t1)
	bnez	t0, 1b
2:
	sub	a0, t1, a0
	ret

SYM_FUNC_END(strnlen)
EXPORT_SYMBOL(strnlen)
