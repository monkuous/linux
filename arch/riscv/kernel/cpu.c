// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <linux/cpu.h>
#include <linux/ctype.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <asm/smp.h>
#include <asm/pgtable.h>

bool arch_match_cpu_phys_id(int cpu, u64 phys_id)
{
	return phys_id == cpuid_to_hartid_map(cpu);
}

/*
 * Returns the hart ID of the given device tree node, or -ENODEV if the node
 * isn't an enabled and valid RISC-V hart node.
 */
int riscv_of_processor_hartid(struct device_node *node, unsigned long *hart)
{
	int cpu;

	*hart = (unsigned long)of_get_cpu_hwid(node, 0);
	if (*hart == ~0UL) {
		pr_warn("Found CPU without hart ID\n");
		return -ENODEV;
	}

	cpu = riscv_hartid_to_cpuid(*hart);
	if (cpu < 0)
		return cpu;

	if (!cpu_possible(cpu))
		return -ENODEV;

	return 0;
}

int __init riscv_early_of_processor_hartid(struct device_node *node, unsigned long *hart)
{
	if (!of_device_is_compatible(node, "riscv")) {
		pr_warn("Found incompatible CPU\n");
		return -ENODEV;
	}

	*hart = (unsigned long)of_get_cpu_hwid(node, 0);
	if (*hart == ~0UL) {
		pr_warn("Found CPU without hart ID\n");
		return -ENODEV;
	}

	if (!of_device_is_available(node))
		return -ENODEV;

	return 0;
}

/*
 * Find hart ID of the CPU DT node under which given DT node falls.
 *
 * To achieve this, we walk up the DT tree until we find an active
 * RISC-V core (HART) node and extract the cpuid from it.
 */
int riscv_of_parent_hartid(struct device_node *node, unsigned long *hartid)
{
	for (; node; node = node->parent) {
		if (of_device_is_compatible(node, "riscv")) {
			*hartid = (unsigned long)of_get_cpu_hwid(node, 0);
			if (*hartid == ~0UL) {
				pr_warn("Found CPU without hart ID\n");
				return -ENODEV;
			}
			return 0;
		}
	}

	return -1;
}

#ifdef CONFIG_PROC_FS

#define ALL_CPUS -1

static void *c_start(struct seq_file *m, loff_t *pos)
{
	if (*pos == nr_cpu_ids)
		return NULL;

	*pos = cpumask_next(*pos - 1, cpu_online_mask);
	if ((*pos) < nr_cpu_ids)
		return (void *)(uintptr_t)(1 + *pos);
	return NULL;
}

static void *c_next(struct seq_file *m, void *v, loff_t *pos)
{
	(*pos)++;
	return c_start(m, pos);
}

static void c_stop(struct seq_file *m, void *v)
{
}

static int c_show(struct seq_file *m, void *v)
{
	unsigned long cpu_id = (unsigned long)v - 1;
	struct device_node *node;
	const char *compat;

	seq_printf(m, "processor\t: %lu\n", cpu_id);
	seq_printf(m, "hart\t\t: %lu\n", cpuid_to_hartid_map(cpu_id));

	node = of_get_cpu_node(cpu_id, NULL);

	if (!of_property_read_string(node, "compatible", &compat) &&
	    strcmp(compat, "riscv"))
		seq_printf(m, "uarch\t\t: %s\n", compat);

	of_node_put(node);

	return 0;
}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= c_show
};

#endif /* CONFIG_PROC_FS */
