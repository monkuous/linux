/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 */

#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

#include <linux/types.h>
#include <linux/cpumask.h>
#include <linux/jump_label.h>

#ifdef CONFIG_RISCV_SBI
enum sbi_ext_id {
	SBI_EXT_0_1_SET_TIMER = 0x0,
	SBI_EXT_0_1_SEND_IPI = 0x4,
	SBI_EXT_BASE = 0x10,
	SBI_EXT_TIME = 0x54494D45,
	SBI_EXT_IPI = 0x735049,
	SBI_EXT_HSM = 0x48534D,
};

enum sbi_ext_base_fid {
	SBI_EXT_BASE_GET_SPEC_VERSION = 0,
	SBI_EXT_BASE_PROBE_EXT = 3,
};

enum sbi_ext_time_fid {
	SBI_EXT_TIME_SET_TIMER = 0,
};

enum sbi_ext_ipi_fid {
	SBI_EXT_IPI_SEND_IPI = 0,
};

enum sbi_ext_hsm_fid {
	SBI_EXT_HSM_HART_START = 0,
	SBI_EXT_HSM_HART_STOP,
	SBI_EXT_HSM_HART_STATUS,
};

enum sbi_hsm_hart_state {
	SBI_HSM_STATE_STARTED = 0,
	SBI_HSM_STATE_STOPPED,
	SBI_HSM_STATE_START_PENDING,
	SBI_HSM_STATE_STOP_PENDING,
};

/* SBI spec version fields */
#define SBI_SPEC_VERSION_DEFAULT	0x1
#define SBI_SPEC_VERSION_MAJOR_SHIFT	24
#define SBI_SPEC_VERSION_MAJOR_MASK	0x7f
#define SBI_SPEC_VERSION_MINOR_MASK	0xffffff

/* SBI return error codes */
#define SBI_SUCCESS		0
#define SBI_ERR_FAILURE		-1
#define SBI_ERR_NOT_SUPPORTED	-2
#define SBI_ERR_INVALID_PARAM	-3
#define SBI_ERR_DENIED		-4
#define SBI_ERR_INVALID_ADDRESS	-5
#define SBI_ERR_ALREADY_AVAILABLE -6
#define SBI_ERR_ALREADY_STARTED -7
#define SBI_ERR_ALREADY_STOPPED -8
#define SBI_ERR_NO_SHMEM	-9
#define SBI_ERR_INVALID_STATE	-10
#define SBI_ERR_BAD_RANGE	-11
#define SBI_ERR_TIMEOUT		-12
#define SBI_ERR_IO		-13
#define SBI_ERR_DENIED_LOCKED	-14

extern unsigned long sbi_spec_version;
struct sbiret {
	long error;
	long value;
};

void sbi_init(void);
long __sbi_base_ecall(int fid);
struct sbiret __sbi_ecall(unsigned long arg0, unsigned long arg1,
			  unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5,
			  int fid, int ext);
#define sbi_ecall(e, f, a0, a1, a2, a3, a4, a5)	\
		__sbi_ecall(a0, a1, a2, a3, a4, a5, f, e)

void sbi_set_timer(uint64_t stime_value);
void sbi_send_ipi(unsigned int cpu);

long sbi_probe_extension(int ext);

/* Check if current SBI specification version is 0.1 or not */
static inline int sbi_spec_is_0_1(void)
{
	return (sbi_spec_version == SBI_SPEC_VERSION_DEFAULT) ? 1 : 0;
}

/* Get the major version of SBI */
static inline unsigned long sbi_major_version(void)
{
	return (sbi_spec_version >> SBI_SPEC_VERSION_MAJOR_SHIFT) &
		SBI_SPEC_VERSION_MAJOR_MASK;
}

/* Get the minor version of SBI */
static inline unsigned long sbi_minor_version(void)
{
	return sbi_spec_version & SBI_SPEC_VERSION_MINOR_MASK;
}

/* Make SBI version */
static inline unsigned long sbi_mk_version(unsigned long major,
					    unsigned long minor)
{
	return ((major & SBI_SPEC_VERSION_MAJOR_MASK) << SBI_SPEC_VERSION_MAJOR_SHIFT)
		| (minor & SBI_SPEC_VERSION_MINOR_MASK);
}

static inline int sbi_err_map_linux_errno(int err)
{
	switch (err) {
	case SBI_SUCCESS:
		return 0;
	case SBI_ERR_DENIED:
	case SBI_ERR_DENIED_LOCKED:
		return -EPERM;
	case SBI_ERR_INVALID_PARAM:
	case SBI_ERR_INVALID_STATE:
		return -EINVAL;
	case SBI_ERR_BAD_RANGE:
		return -ERANGE;
	case SBI_ERR_INVALID_ADDRESS:
		return -EFAULT;
	case SBI_ERR_NO_SHMEM:
		return -ENOMEM;
	case SBI_ERR_TIMEOUT:
		return -ETIMEDOUT;
	case SBI_ERR_IO:
		return -EIO;
	case SBI_ERR_NOT_SUPPORTED:
	case SBI_ERR_FAILURE:
	default:
		return -ENOTSUPP;
	};
}

#else /* CONFIG_RISCV_SBI */
static inline void sbi_init(void) {}
#endif /* CONFIG_RISCV_SBI */

#if IS_ENABLED(CONFIG_SMP) && IS_ENABLED(CONFIG_RISCV_SBI)
void sbi_ipi_init(void);
#else
static inline void sbi_ipi_init(void) { }
#endif

#endif /* _ASM_RISCV_SBI_H */
