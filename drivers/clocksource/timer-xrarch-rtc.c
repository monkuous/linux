/* SPDX-License-Identifier: GPL-2.0-only */
#define pr_fmt(fmt) "xrarch-rtc: " fmt
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/rtc.h>
#include <linux/platform_device.h>

#define XR_RTC_LATCH	((1000UL + (HZ / 2)) / HZ)

#define XR_RTC_CMD	0x00
#define XR_RTC_DATA	0x04

#define XR_RTC_SET_IRQ		0x01
#define XR_RTC_GET_TIME		0x02
#define XR_RTC_GET_TIME_MS	0x03
#define XR_RTC_SET_TIME		0x04
#define XR_RTC_SET_TIME_MS	0x05

struct xrarch_rtc_timer {
	struct clock_event_device base;
	void __iomem *regs;
};

DEFINE_SPINLOCK(xrarch_rtc_lock);

static irqreturn_t xrarch_rtc_irq(int irq, void *dev_id)
{
	struct xrarch_rtc_timer *device = dev_id;
	device->base.event_handler(&device->base);
	return IRQ_HANDLED;
}

static int xrarch_rtc_shutdown(struct clock_event_device *evt)
{
	struct xrarch_rtc_timer *device = (struct xrarch_rtc_timer *)evt;
	unsigned long flags;

	spin_lock_irqsave(&xrarch_rtc_lock, flags);
	writel(0, device->regs + XR_RTC_DATA);
	writel(XR_RTC_SET_IRQ, device->regs + XR_RTC_CMD);
	spin_unlock_irqrestore(&xrarch_rtc_lock, flags);

	return 0;
}

static int xrarch_rtc_periodic(struct clock_event_device *evt)
{
	struct xrarch_rtc_timer *device = (struct xrarch_rtc_timer *)evt;
	unsigned long flags;

	spin_lock_irqsave(&xrarch_rtc_lock, flags);
	writel(XR_RTC_LATCH, device->regs + XR_RTC_DATA);
	writel(XR_RTC_SET_IRQ, device->regs + XR_RTC_CMD);
	spin_unlock_irqrestore(&xrarch_rtc_lock, flags);

	return 0;
}

static int __init xrarch_rtc_timer_init(struct device_node *np)
{
	BUG_ON(XR_RTC_LATCH < 1);

	void __iomem *regs = of_iomap(np, 0);
	if (!regs)
		return -ENOMEM;

	int error;

	struct xrarch_rtc_timer *data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data) {
		error = -ENOMEM;
		goto fail_free_regs;
	}

	data->base.name = "xrarch-rtc";
	data->base.features = CLOCK_EVT_FEAT_PERIODIC;
	data->base.set_state_shutdown = xrarch_rtc_shutdown;
	data->base.set_state_periodic = xrarch_rtc_periodic;
	data->base.cpumask = cpumask_of(smp_processor_id());
	data->regs = regs;

	xrarch_rtc_shutdown(&data->base);

	int irq = of_irq_get(np, 0);
	if (irq <= 0) {
		error = irq ? irq : -ENODEV;
		goto fail_free_data;
	}

	error = request_irq(irq, xrarch_rtc_irq, IRQF_NOBALANCING |
			    IRQF_IRQPOLL | IRQF_TIMER, "xrarch-rtc", data);
	if (error)
		goto fail_free_data;

	clockevents_config_and_register(&data->base, 1000, 0, 0);

	return 0;

fail_free_data:
	kfree(data);
fail_free_regs:
	iounmap(regs);
	return error;
}

TIMER_OF_DECLARE(xrarch_rtc, "xrarch,rtc", xrarch_rtc_timer_init);

#ifdef CONFIG_RTC_CLASS

struct xrarch_rtc {
	struct rtc_device *rtc;
	void __iomem *regs;
};

static int xrarch_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct xrarch_rtc *device;
	unsigned long flags;
	s32 time;

	device = dev_get_drvdata(dev);;

	spin_lock_irqsave(&xrarch_rtc_lock, flags);

	writel(XR_RTC_GET_TIME, device->regs + XR_RTC_CMD);
	time = readl(device->regs + XR_RTC_DATA);

	spin_unlock_irqrestore(&xrarch_rtc_lock, flags);

	rtc_time64_to_tm(time, tm);

	return 0;
}

static int xrarch_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct xrarch_rtc *device;
	unsigned long flags;
	s32 time;

	device = dev_get_drvdata(dev);
	time = rtc_tm_to_time64(tm);

	spin_lock_irqsave(&xrarch_rtc_lock, flags);

	writel(time, device->regs + XR_RTC_DATA);
	writel(XR_RTC_SET_TIME, device->regs + XR_RTC_CMD);
	writel(0, device->regs + XR_RTC_DATA);
	writel(XR_RTC_SET_TIME, device->regs + XR_RTC_CMD);

	spin_unlock_irqrestore(&xrarch_rtc_lock, flags);

	return 0;
}

static const struct rtc_class_ops xrarch_rtc_ops = {
	.read_time	= xrarch_rtc_read_time,
	.set_time	= xrarch_rtc_set_time,
};

static int xrarch_rtc_probe(struct platform_device *pdev)
{
	struct xrarch_rtc *device;

	device = devm_kzalloc(&pdev->dev, sizeof(*device), GFP_KERNEL);
	if (!device)
		return -ENOMEM;

	platform_set_drvdata(pdev, device);
	device->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(device->regs))
		return PTR_ERR(device->regs);

	device->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(device->rtc))
		return PTR_ERR(device->rtc);

	device->rtc->ops = &xrarch_rtc_ops;
	device->rtc->range_min = S32_MIN;
	device->rtc->range_max = S32_MAX;

	return devm_rtc_register_device(device->rtc);
}

static const struct of_device_id xrarch_rtc_of_match[] = {
	{ .compatible = "xrarch,rtc", },
	{},
};
MODULE_DEVICE_TABLE(of, xrarch_rtc_of_match);

static struct platform_driver xrarch_rtc = {
	.probe = xrarch_rtc_probe,
	.driver = {
		.name = "xrarch_rtc",
		.of_match_table = xrarch_rtc_of_match,
	},
};

module_platform_driver(xrarch_rtc);

#endif /* CONFIG_RTC_CLASS */

MODULE_DESCRIPTION("XR/arch Real Time Clock driver");
MODULE_LICENSE("GPL v2");
