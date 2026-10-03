/* SPDX-License-Identifier: GPL-2.0-only */

#include <linux/linkage.h>
#include <asm/asm.h>

/* int strlen(const char *s) */
SYM_FUNC_START(strlen)

	/*
	 * Returns
	 *   a0 - string length
	 *
	 * Parameters
	 *   a0 - String to measure
	 *
	 * Clobbers:
	 *   t0, t1
	 */
	addi	t1, a0, -1
1:
	addi	t1, t1, 1
	lbu	t0, 0(t1)
	bnez	t0, 1b
	sub	a0, t1, a0
	ret

SYM_FUNC_END(strlen)
EXPORT_SYMBOL(strlen)
