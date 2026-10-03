/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 SiFive
 */

#ifndef _ASM_RISCV_CACHEINFO_H
#define _ASM_RISCV_CACHEINFO_H

#include <linux/cacheinfo.h>

uintptr_t get_cache_size(u32 level, enum cache_type type);
uintptr_t get_cache_geometry(u32 level, enum cache_type type);

#endif /* _ASM_RISCV_CACHEINFO_H */
