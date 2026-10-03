/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 Rivos Inc.
 */

.text
.global test_set32
test_set32:
	lw	a0, set32
	la	t0, set32
	sub	a0, a0, t0
	ret
.data
set32:
	.reloc set32, R_RISCV_SET32, set32
	.word 0
