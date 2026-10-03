/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Rivos Inc.
 */

.text
.global test_sub64
test_sub64:
	lw	a0, sub64
	addi	a0, a0, -32
	ret
first:
	.space 32
second:

.data
sub64:
	.reloc		sub64, R_RISCV_ADD64, second
	.reloc		sub64, R_RISCV_SUB64, first
	.word		0
	.word		0
