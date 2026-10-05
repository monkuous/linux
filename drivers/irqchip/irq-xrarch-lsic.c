/* SPDX-License-Identifier: GPL-2.0-only */
#define pr_fmt(fmt) "xrarch-lsic: " fmt
#include <asm/smp.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>

#define LSIC_IRQS 64

#define LSIC_STRIDE 0x20

#define LSIC_DISA 0x00
#define LSIC_PEND 0x08
#define LSIC_COMPL 0x10
#define LSIC_CLAIM 0x10
#define LSIC_IPL 0x14

struct lsic_priv {
	struct fwnode_handle *fwnode;
	struct irq_domain *domain;
	struct cpumask lmask;
	irq_hw_number_t ipi_h;
	unsigned ipi_v;
};

struct lsic_target {
	struct lsic_priv *priv;
	void __iomem *regs;
	spinlock_t mask_lock;
};

static DEFINE_PER_CPU(struct lsic_target, lsic_targets);

static void lsic_handle_irq(struct pt_regs *desc)
{
	struct lsic_target *target = this_cpu_ptr(&lsic_targets);
	irq_hw_number_t hwirq;

	while ((hwirq = readl(target->regs + LSIC_CLAIM)) != 0) {
		int err = generic_handle_domain_irq(target->priv->domain,
						    hwirq);

		if (unlikely(err)) {
			pr_warn_ratelimited("%pfwP: can't find mapping for hwirq %lu",
					    target->priv->fwnode, hwirq);
		}
	}
}

static void lsic_irq_set_masked(struct irq_data *d, bool masked)
{
	int cpu;
	unsigned long flags;
	unsigned int hwirq = d->hwirq;
	unsigned int offset = hwirq / 32 * 4;
	unsigned int mask = 1U << (hwirq % 32);

	for_each_cpu(cpu, irq_data_get_effective_affinity_mask(d)) {
		struct lsic_target *handler = per_cpu_ptr(&lsic_targets, cpu);

		if (hwirq == handler->priv->ipi_h) {
			continue;
		}

		spin_lock_irqsave(&handler->mask_lock, flags);

		void __iomem *reg = handler->regs + LSIC_DISA + offset;
		unsigned int value = readl(reg);

		if (masked)
			writel(value | mask, reg);
		else
			writel(value & ~mask, reg);

		spin_unlock_irqrestore(&handler->mask_lock, flags);
	}
}

static void lsic_irq_mask(struct irq_data *d)
{
	lsic_irq_set_masked(d, true);
}

static void lsic_irq_unmask(struct irq_data *d)
{
	lsic_irq_set_masked(d, false);
}

static void lsic_irq_eoi(struct irq_data *d)
{
	struct lsic_target *handler = this_cpu_ptr(&lsic_targets);
	writel(d->hwirq, handler->regs + LSIC_COMPL);
}

static int lsic_set_affinity(struct irq_data *d,
			     const struct cpumask *mask_val, bool force)
{
	struct lsic_priv *priv = irq_data_get_irq_chip_data(d);
	unsigned int cpu;

	if (force)
		cpu = cpumask_first_and(&priv->lmask, mask_val);
	else
		cpu = cpumask_first_and_and(&priv->lmask, mask_val,
					    cpu_online_mask);

	if (cpu >= nr_cpu_ids)
		return -EINVAL;

	lsic_irq_mask(d);
	irq_data_update_effective_affinity(d, cpumask_of(cpu));
	if (!irqd_irq_masked(d))
		lsic_irq_unmask(d);

	return IRQ_SET_MASK_OK_DONE;
}

static struct irq_chip lsic_chip = {
	.name			= "XR/arch LSIC",
	.irq_mask		= lsic_irq_mask,
	.irq_unmask		= lsic_irq_unmask,
	.irq_eoi		= lsic_irq_eoi,
	.irq_set_affinity	= lsic_set_affinity,
	.flags			= IRQCHIP_SKIP_SET_WAKE |
				  IRQCHIP_AFFINITY_PRE_STARTUP,
};

static int lsic_domain_map(struct irq_domain *d, unsigned int irq,
			   irq_hw_number_t hwirq)
{
	struct lsic_priv *priv = d->host_data;

	irq_domain_set_info(d, irq, hwirq, &lsic_chip, priv, handle_fasteoi_irq,
			    NULL, NULL);
	irq_set_noprobe(irq);
	irq_set_affinity(irq, &priv->lmask);

	return 0;
}

static int lsic_domain_alloc(struct irq_domain *domain, unsigned int virq,
			     unsigned int nr_irqs, void *arg)
{
	struct irq_fwspec *fwspec = arg;
	irq_hw_number_t hwirq;
	unsigned int type;

	int ret = irq_domain_translate_onecell(domain, fwspec, &hwirq, &type);
	if (ret)
		return ret;

	if (hwirq + nr_irqs >= LSIC_IRQS)
		return -EINVAL;

	for (unsigned int i = 0; i < nr_irqs; i++) {
		ret = lsic_domain_map(domain, virq + i, hwirq + i);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct irq_domain_ops lsic_domain_ops = {
	.map		= lsic_domain_map,
	.alloc		= lsic_domain_alloc,
	.translate	= irq_domain_translate_onecell,
	.free		= irq_domain_free_irqs_top,
};

static int lsic_parse_target(struct device_node *device, u32 idx, int *cpu)
{
	unsigned long cpu_hwid;
	struct of_phandle_args cpu_data;
	int error = of_parse_phandle_with_args(device, "xrarch,targets", NULL,
					       idx, &cpu_data);
	if (error)
		return error;

	error = riscv_of_parent_hartid(cpu_data.np, &cpu_hwid);
	if (error < 0) {
		pr_warn("unable to find cpu hwid for %pOF\n", cpu_data.np);
		return error;
	}

	*cpu = riscv_hartid_to_cpuid(cpu_hwid);
	return 0;
}

#ifdef CONFIG_SMP
static void lsic_send_ipi(unsigned int cpu)
{
	struct lsic_target *target = per_cpu_ptr(&lsic_targets, cpu);

	if (target->priv == NULL) {
		pr_warn_ratelimited("cannot send IPI to CPU %u\n", cpu);
		return;
	}

	unsigned idx = target->priv->ipi_h / 32;
	unsigned mask = 1U << (target->priv->ipi_h % 32);

	writel(mask, target->regs + LSIC_PEND + idx * 4);
}

static void lsic_clear_ipi(void)
{
	struct lsic_target *target = this_cpu_ptr(&lsic_targets);

	if (target->priv == NULL) {
		return;
	}

	writel(target->priv->ipi_h, target->regs + LSIC_CLAIM);
}

static void lsic_handle_ipi(struct irq_desc *desc)
{
	lsic_clear_ipi();
	ipi_mux_process();
}

static int lsic_starting_cpu(unsigned int cpu)
{
	struct lsic_target *target = this_cpu_ptr(&lsic_targets);

	if (target->priv != NULL) {
		enable_percpu_irq(target->priv->ipi_v,
				  irq_get_trigger_type(target->priv->ipi_v));
	}

	return 0;
}
#endif /* CONFIG_SMP */

static int __init lsic_init(struct device_node *device,
			    struct device_node *parent)
{
	static bool lsic_global_init_done;

	void __iomem *regs = of_iomap(device, 0);
	if (!regs)
		return -ENOMEM;

	int error, hwi_error;

	struct fwnode_handle *fwnode = &device->fwnode;

	error = of_count_phandle_with_args(device, "xrarch,targets", NULL);
	if (error < 0) {
		pr_err("%pfwP: failed to parse xrarch,targets\n", fwnode);
		goto fail_free_regs;
	}

	u32 nr_targets = error;
	if (WARN_ON(!nr_targets)) {
		pr_err("%pfwP: no upstream targets available\n", fwnode);
		error = -EINVAL;
		goto fail_free_regs;
	}

	struct lsic_priv *priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		error = -ENOMEM;
		goto fail_free_regs;
	}

	priv->fwnode = fwnode;
	priv->domain = irq_domain_create_linear(fwnode, LSIC_IRQS,
						&lsic_domain_ops, priv);
	if (!priv->domain) {
		error = -ENOMEM;
		goto fail_free_priv;
	}

	unsigned int hwirq;
	hwi_error = of_property_read_u32(device, "xrarch,ipi-irq", &hwirq);

	for (u32 idx = 0; idx < nr_targets; idx++) {
		int cpu;

		error = lsic_parse_target(device, idx, &cpu);
		if (error) {
			pr_warn("%pfwP: hwirq for target %u not found\n",
				fwnode, idx);
			continue;
		}

		void __iomem *target_regs = regs + idx * LSIC_STRIDE;
		unsigned long low_mask = ~0UL;
		unsigned long high_mask = ~0UL;

		if (hwi_error >= 0) {
			if (hwirq < 32) {
				low_mask &= ~(1UL << hwirq);
			} else {
				high_mask &= ~(1UL << (hwirq - 32));
			}
		}

		writel(low_mask, target_regs + LSIC_DISA);
		writel(high_mask, target_regs + LSIC_DISA + 4);
		writel(0, target_regs + LSIC_PEND);
		writel(0, target_regs + LSIC_PEND + 4);
		writel(LSIC_IRQS - 1, target_regs + LSIC_IPL);

		if (cpu < 0) {
			pr_warn("%pfwP: invalid cpuid for target %u\n",
				fwnode, idx);
			continue;
		}

		cpumask_set_cpu(cpu, &priv->lmask);

		struct lsic_target *target = per_cpu_ptr(&lsic_targets, cpu);
		target->priv = priv;
		target->regs = target_regs;
		spin_lock_init(&target->mask_lock);
	}

#ifdef CONFIG_SMP
	if (hwi_error >= 0) {
		priv->ipi_h = hwirq;
		priv->ipi_v = irq_create_mapping(priv->domain, hwirq);

		if (priv->ipi_v == 0) {
			pr_warn("%pfwP: failed to create IPI IRQ\n", device);
			goto no_ipi;
		}

		error = ipi_mux_create(BITS_PER_BYTE, lsic_send_ipi);

		if (error <= 0) {
			pr_warn("%pfwP: failed to create muxed IPIs (%d)\n",
				device, error);
			irq_dispose_mapping(priv->ipi_v);
			goto no_ipi;
		}

		irq_set_chained_handler(priv->ipi_v, lsic_handle_ipi);
		riscv_ipi_set_virq_range(error, BITS_PER_BYTE);
		lsic_clear_ipi();

		for (u32 idx = 0; idx < nr_targets; idx++) {

		}
	} else {
		pr_warn("%pfwP: failed to get IPI IRQ (%d)", device, error);
	}
no_ipi:
#endif /* CONFIG_SMP */

	if (!lsic_global_init_done) {
#ifdef CONFIG_SMP
		error = cpuhp_setup_state(CPUHP_AP_IRQ_XRARCH_LSIC_STARTING,
					  "irqchip/xrarch/lsic:starting",
					  lsic_starting_cpu, NULL);
		if (error < 0) {
			pr_warn("%pfwP: cpuhp setup state failed (%d)\n", device,
				error);
		}
#endif /* CONFIG_SMP */

		error = set_handle_irq(lsic_handle_irq);

		if (error < 0) {
			pr_warn("%pfwP: failed to set global IRQ handler (%d)",
				device, error);
		}

		lsic_global_init_done = true;
	}

	pr_info("%pfwP: mapped %u targets\n", fwnode, nr_targets);
	return 0;

fail_free_priv:
	kfree(priv);
fail_free_regs:
	iounmap(regs);
	return error;
}

IRQCHIP_DECLARE(xrarch_lsic, "xrarch,lsic", lsic_init);
