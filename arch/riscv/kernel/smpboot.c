// SPDX-License-Identifier: GPL-2.0-only
/*
 * SMP initialisation and IPI support
 * Based on arch/arm64/kernel/smp.c
 *
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#include <linux/arch_topology.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>

#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/numa.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/smp.h>

#include "head.h"

#ifndef CONFIG_HOTPLUG_PARALLEL
static DECLARE_COMPLETION(cpu_running);
#endif

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int cpuid;
	unsigned int curr_cpuid;

	init_cpu_topology();

	curr_cpuid = smp_processor_id();
	store_cpu_topology(curr_cpuid);
	numa_store_cpu_info(curr_cpuid);
	numa_add_cpu(curr_cpuid);

	/* This covers non-smp usecase mandated by "nosmp" option */
	if (max_cpus == 0)
		return;

	for_each_possible_cpu(cpuid) {
		if (cpuid == curr_cpuid)
			continue;
		set_cpu_present(cpuid, true);
		numa_store_cpu_info(cpuid);
	}
}

static void __init of_parse_and_init_cpus(void)
{
	struct device_node *dn;
	unsigned long hart;
	bool found_boot_cpu = false;
	int cpuid = 1;
	int rc;

	for_each_of_cpu_node(dn) {
		rc = riscv_early_of_processor_hartid(dn, &hart);
		if (rc < 0)
			continue;

		if (hart == cpuid_to_hartid_map(0)) {
			BUG_ON(found_boot_cpu);
			found_boot_cpu = 1;
			early_map_cpu_to_node(0, of_node_to_nid(dn));
			continue;
		}
		if (cpuid >= NR_CPUS) {
			pr_warn("Invalid cpuid [%d] for hartid [%lu]\n",
				cpuid, hart);
			continue;
		}

		cpuid_to_hartid_map(cpuid) = hart;
		early_map_cpu_to_node(cpuid, of_node_to_nid(dn));
		cpuid++;
	}

	BUG_ON(!found_boot_cpu);

	if (cpuid > nr_cpu_ids)
		pr_warn("Total number of cpus [%d] is greater than nr_cpus option value [%d]\n",
			cpuid, nr_cpu_ids);
}

void __init setup_smp(void)
{
	int cpuid;

	cpu_set_ops();

	of_parse_and_init_cpus();

	for (cpuid = 1; cpuid < nr_cpu_ids; cpuid++)
		if (cpuid_to_hartid_map(cpuid) != INVALID_HARTID)
			set_cpu_possible(cpuid, true);
}

static int start_secondary_cpu(int cpu, struct task_struct *tidle)
{
	if (cpu_ops->cpu_start)
		return cpu_ops->cpu_start(cpu, tidle);

	return -EOPNOTSUPP;
}

#ifdef CONFIG_HOTPLUG_PARALLEL
int arch_cpuhp_kick_ap_alive(unsigned int cpu, struct task_struct *tidle)
{
	return start_secondary_cpu(cpu, tidle);
}
#else
int __cpu_up(unsigned int cpu, struct task_struct *tidle)
{
	int ret;
	tidle->thread_info.cpu = cpu;

	ret = start_secondary_cpu(cpu, tidle);
	if (!ret) {
		wait_for_completion_timeout(&cpu_running, secs_to_jiffies(1));

		if (!cpu_online(cpu)) {
			pr_crit("CPU%u: failed to come online\n", cpu);
			ret = -EIO;
		}
	} else {
		pr_crit("CPU%u: failed to start\n", cpu);
	}

	return ret;
}
#endif

void __init smp_cpus_done(unsigned int max_cpus)
{
}

/*
 * C entry point for a secondary processor.
 */
asmlinkage __visible void smp_callin(void)
{
	struct mm_struct *mm = &init_mm;
	unsigned int curr_cpuid = smp_processor_id();

	/* All kernel threads share the same mm context.  */
	mmgrab(mm);
	current->active_mm = mm;

#ifdef CONFIG_HOTPLUG_PARALLEL
	cpuhp_ap_sync_alive();
#endif

	store_cpu_topology(curr_cpuid);
	notify_cpu_starting(curr_cpuid);

	csr_set(CSR_IE, IE_EIE);
	riscv_ipi_enable();

	numa_add_cpu(curr_cpuid);

	pr_debug("CPU%u: Booted secondary hartid %lu\n", curr_cpuid,
		cpuid_to_hartid_map(curr_cpuid));

	set_cpu_online(curr_cpuid, true);

	/*
	 * Remote instruction cache and TLB flushes are ignored while the CPU
	 * is offline, so flush them both right now just in case.
	 */
	local_flush_icache_all();
	local_flush_tlb_all();
#ifndef CONFIG_HOTPLUG_PARALLEL
	complete(&cpu_running);
#endif
	local_irq_enable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}
