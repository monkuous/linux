// SPDX-License-Identifier: GPL-2.0-only
/*
 * SBI initialilization and all extension implementation.
 *
 * Copyright (c) 2020 Western Digital Corporation or its affiliates.
 */

#include <linux/bits.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/pm.h>
#include <linux/reboot.h>
#include <asm/sbi.h>
#include <asm/smp.h>
#include <asm/tlbflush.h>

/* default SBI version is 0.1 */
unsigned long sbi_spec_version __ro_after_init = SBI_SPEC_VERSION_DEFAULT;
EXPORT_SYMBOL(sbi_spec_version);

static void (*__sbi_set_timer)(uint64_t stime) __ro_after_init;
static void (*__sbi_send_ipi)(unsigned int cpu) __ro_after_init;

static unsigned long __sbi_v01_cpumask_to_hartmask(const struct cpumask *cpu_mask)
{
	unsigned long cpuid, hartid;
	unsigned long hmask = 0;

	/*
	 * There is no maximum hartid concept in RISC-V and NR_CPUS must not be
	 * associated with hartid. As SBI v0.1 is only kept for backward compatibility
	 * and will be removed in the future, there is no point in supporting hartid
	 * greater than BITS_PER_LONG (32 for RV32 and 64 for RV64). Ideally, SBI v0.2
	 * should be used for platforms with hartid greater than BITS_PER_LONG.
	 */
	for_each_cpu(cpuid, cpu_mask) {
		hartid = cpuid_to_hartid_map(cpuid);
		if (hartid >= BITS_PER_LONG) {
			pr_warn("Unable to send any request to hartid > BITS_PER_LONG for SBI v0.1\n");
			break;
		}
		hmask |= BIT(hartid);
	}

	return hmask;
}

/**
 * __sbi_set_timer_v01() - Program the timer for next timer event.
 * @stime_value: The value after which next timer event should fire.
 *
 * Return: None
 */
static void __sbi_set_timer_v01(uint64_t stime_value)
{
	sbi_ecall(SBI_EXT_0_1_SET_TIMER, 0, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
}

static void __sbi_send_ipi_v01(unsigned int cpu)
{
	unsigned long hart_mask =
		__sbi_v01_cpumask_to_hartmask(cpumask_of(cpu));
	sbi_ecall(SBI_EXT_0_1_SEND_IPI, 0, (unsigned long)(&hart_mask),
		  0, 0, 0, 0, 0);
}

static void __sbi_set_timer_v02(uint64_t stime_value)
{
	sbi_ecall(SBI_EXT_TIME, SBI_EXT_TIME_SET_TIMER, stime_value,
		  stime_value >> 32, 0, 0, 0, 0);
}

static void __sbi_send_ipi_v02(unsigned int cpu)
{
	int result;
	struct sbiret ret = {0};

	ret = sbi_ecall(SBI_EXT_IPI, SBI_EXT_IPI_SEND_IPI,
			1UL, cpuid_to_hartid_map(cpu), 0, 0, 0, 0);
	if (ret.error) {
		result = sbi_err_map_linux_errno(ret.error);
		pr_err("%s: hbase = [%lu] failed (error [%d])\n",
			__func__, cpuid_to_hartid_map(cpu), result);
	}
}

/**
 * sbi_set_timer() - Program the timer for next timer event.
 * @stime_value: The value after which next timer event should fire.
 *
 * Return: None.
 */
void sbi_set_timer(uint64_t stime_value)
{
	__sbi_set_timer(stime_value);
}

/**
 * sbi_send_ipi() - Send an IPI to any hart.
 * @cpu: Logical id of the target CPU.
 */
void sbi_send_ipi(unsigned int cpu)
{
	__sbi_send_ipi(cpu);
}
EXPORT_SYMBOL(sbi_send_ipi);

/**
 * sbi_probe_extension() - Check if an SBI extension ID is supported or not.
 * @extid: The extension ID to be probed.
 *
 * Return: 1 or an extension specific nonzero value if yes, 0 otherwise.
 */
long sbi_probe_extension(int extid)
{
	struct sbiret ret;

	ret = sbi_ecall(SBI_EXT_BASE, SBI_EXT_BASE_PROBE_EXT, extid,
			0, 0, 0, 0, 0);
	if (!ret.error)
		return ret.value;

	return 0;
}
EXPORT_SYMBOL(sbi_probe_extension);

static inline long sbi_get_spec_version(void)
{
	return __sbi_base_ecall(SBI_EXT_BASE_GET_SPEC_VERSION);
}

void __init sbi_init(void)
{
	int ret;

	ret = sbi_get_spec_version();
	if (ret > 0)
		sbi_spec_version = ret;

	pr_info("SBI specification v%lu.%lu detected\n",
		sbi_major_version(), sbi_minor_version());

	if (!sbi_spec_is_0_1()) {
		if (sbi_probe_extension(SBI_EXT_TIME)) {
			__sbi_set_timer = __sbi_set_timer_v02;
			pr_info("SBI TIME extension detected\n");
		} else {
			__sbi_set_timer = __sbi_set_timer_v01;
		}
		if (sbi_probe_extension(SBI_EXT_IPI)) {
			__sbi_send_ipi	= __sbi_send_ipi_v02;
			pr_info("SBI IPI extension detected\n");
		} else {
			__sbi_send_ipi	= __sbi_send_ipi_v01;
		}
	} else {
		__sbi_set_timer = __sbi_set_timer_v01;
		__sbi_send_ipi	= __sbi_send_ipi_v01;
	}
}
