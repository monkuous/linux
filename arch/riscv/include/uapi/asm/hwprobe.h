/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright 2023-2024 Rivos, Inc
 */

#ifndef _UAPI_ASM_HWPROBE_H
#define _UAPI_ASM_HWPROBE_H

#include <linux/types.h>
#include <linux/const.h>

/*
 * Interface for probing hardware capabilities from userspace, see
 * Documentation/arch/riscv/hwprobe.rst for more information.
 */
struct riscv_hwprobe {
	__s64 key;
	__u64 value;
};

#define RISCV_HWPROBE_KEY_BASE_BEHAVIOR	3
#define		RISCV_HWPROBE_BASE_BEHAVIOR_IMA	_BITULL(0)

#define RISCV_HWPROBE_KEY_CPUPERF_0	5
#define		RISCV_HWPROBE_MISALIGNED_EMULATED	1
#define RISCV_HWPROBE_KEY_HIGHEST_VIRT_ADDRESS	7
#define RISCV_HWPROBE_KEY_TIME_CSR_FREQ	8
#define RISCV_HWPROBE_KEY_MISALIGNED_SCALAR_PERF	9
#define		RISCV_HWPROBE_MISALIGNED_SCALAR_EMULATED	1

/* Increase RISCV_HWPROBE_MAX_KEY when adding items. */

/* Flags */
#define RISCV_HWPROBE_WHICH_CPUS	_BITUL(0)

#endif
