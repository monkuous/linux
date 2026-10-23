/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/amtsu.h>
#include <linux/device.h>
#include <linux/device/bus.h>
#include <linux/device/driver.h>
#include <linux/idr.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#define AMTSU_DEVICES	4

#define AMTSU_SELECT	0x00
#define AMTSU_MID	0x04
#define AMTSU_CMD	0x08
#define AMTSU_A		0x0c
#define AMTSU_B		0x10

#define AMTSU_IDX_NONE (-2)
#define AMTSU_IDX_CTRL (-1)

#define AMTSU_CMD_ENABLE_IRQ	0x1
#define AMTSU_CMD_RESET		0x2
#define AMTSU_CMD_DISABLE_IRQ	0x3

struct amtsu_ctrl {
	struct platform_device *pdev;
	struct amtsu_ctrl_device *devices[AMTSU_DEVICES];
	void __iomem *regs;
	spinlock_t lock;
	int selected;
};

struct amtsu_ctrl_device {
	struct amtsu_device adev;
	struct amtsu_ctrl *ctrl;
	int index;
	bool irq_enabled;
};

#define amtsu_to_ctrl(_dev)	\
	container_of_const(_dev, struct amtsu_ctrl_device, adev)

static DEFINE_IDA(amtsu_index_ida);

static int amtsu_register_device(struct amtsu_device *dev);
static void amtsu_unregister_device(struct amtsu_device *dev);

static void amtsu_ctrl_select(struct amtsu_ctrl *ctrl, int index)
{
	if (ctrl->selected == index)
		return;

	writel(index + 1, ctrl->regs + AMTSU_SELECT);
	ctrl->selected = index;
}

static void amtsu_ctrl_enable_irq(struct amtsu_device *_d)
{
	struct amtsu_ctrl_device *dev = amtsu_to_ctrl(_d);

	unsigned long flags;
	spin_lock_irqsave(&dev->ctrl->lock, flags);

	if (!dev->irq_enabled) {
		amtsu_ctrl_select(dev->ctrl, AMTSU_IDX_CTRL);

		writel(dev->index + 1, dev->ctrl->regs + AMTSU_B);
		writel(AMTSU_CMD_ENABLE_IRQ, dev->ctrl->regs + AMTSU_CMD);

		dev->irq_enabled = true;
	}

	spin_unlock_irqrestore(&dev->ctrl->lock, flags);
}

static void amtsu_ctrl_disable_irq(struct amtsu_device *_d)
{
	struct amtsu_ctrl_device *dev = amtsu_to_ctrl(_d);

	unsigned long flags;
	spin_lock_irqsave(&dev->ctrl->lock, flags);

	if (dev->irq_enabled) {
		amtsu_ctrl_select(dev->ctrl, AMTSU_IDX_CTRL);

		writel(dev->index + 1, dev->ctrl->regs + AMTSU_B);
		writel(AMTSU_CMD_DISABLE_IRQ, dev->ctrl->regs + AMTSU_CMD);

		dev->irq_enabled = false;
	}

	spin_unlock_irqrestore(&dev->ctrl->lock, flags);
}

static void amtsu_ctrl_command(struct amtsu_device *_d, u32 cmd, u32 cmd_flags,
			       u32 *a, u32 *b)
{
	struct amtsu_ctrl_device *dev = amtsu_to_ctrl(_d);

	unsigned long flags;
	spin_lock_irqsave(&dev->ctrl->lock, flags);

	amtsu_ctrl_select(dev->ctrl, dev->index);

	if (cmd_flags & AMTSU_CMD_WRITE_A)
		writel(*a, dev->ctrl->regs + AMTSU_A);
	if (cmd_flags & AMTSU_CMD_WRITE_B)
		writel(*b, dev->ctrl->regs + AMTSU_B);

	writel(cmd, dev->ctrl->regs + AMTSU_CMD);

	if (cmd_flags & AMTSU_CMD_READ_A)
		*a = readl(dev->ctrl->regs + AMTSU_A);
	if (cmd_flags & AMTSU_CMD_READ_B)
		*b = readl(dev->ctrl->regs + AMTSU_B);

	spin_unlock_irqrestore(&dev->ctrl->lock, flags);
}

static const struct amtsu_bus_ops amtsu_ctrl_ops = {
	.enable_irq	= amtsu_ctrl_enable_irq,
	.disable_irq	= amtsu_ctrl_disable_irq,
	.command	= amtsu_ctrl_command,
};

static void amtsu_ctrl_release_dev(struct device *_d)
{
	struct amtsu_ctrl_device *dev = amtsu_to_ctrl(dev_to_amtsu(_d));

	kfree(dev);
}

static int amtsu_ctrl_probe(struct platform_device *pdev)
{
	struct amtsu_ctrl *ctrl = devm_kzalloc(&pdev->dev, sizeof(*ctrl),
					       GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->pdev = pdev;
	spin_lock_init(&ctrl->lock);
	ctrl->selected = AMTSU_IDX_NONE;

	int ret;
	ctrl->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(ctrl->regs)) {
		ret = PTR_ERR(ctrl->regs);
		goto fail_free_ctrl;
	}

	platform_set_drvdata(pdev, ctrl);

	amtsu_ctrl_select(ctrl, AMTSU_IDX_CTRL);

	// disable all device irqs and reset them

	for (int idx = 0; idx < AMTSU_DEVICES; idx++) {
		writel(idx + 1, ctrl->regs + AMTSU_B);
		writel(AMTSU_CMD_DISABLE_IRQ, ctrl->regs + AMTSU_CMD);
	}

	writel(AMTSU_CMD_RESET, ctrl->regs + AMTSU_CMD);

	// discover devices

	for (int idx = 0; idx < AMTSU_DEVICES; idx++) {
		unsigned long flags;
		spin_lock_irqsave(&ctrl->lock, flags);

		amtsu_ctrl_select(ctrl, idx);
		u32 model_id = readl(ctrl->regs + AMTSU_MID);

		spin_unlock_irqrestore(&ctrl->lock, flags);

		if (!model_id)
			continue;

		struct amtsu_ctrl_device *dev = kzalloc(sizeof(*dev),
							GFP_KERNEL);
		if (!dev) {
			dev_warn(&pdev->dev,
				 "failed to allocate amtsu device %d\n", idx);
			continue;
		}

		dev->adev.dev.parent = &pdev->dev;
		dev->adev.dev.release = amtsu_ctrl_release_dev;
		dev->adev.id.model_id = model_id;
		dev->adev.bus = &amtsu_ctrl_ops;
		dev->ctrl = ctrl;
		dev->index = idx;

		ret = platform_get_irq(pdev, idx);
		if (ret < 0) {
			kfree(dev);
			continue;
		}
		dev->adev.irq = ret;

		ret = amtsu_register_device(&dev->adev);
		if (ret)
			put_device(&dev->adev.dev);

		ctrl->devices[idx] = dev;
	}

	return 0;
fail_free_ctrl:
	devm_kfree(&pdev->dev, ctrl);
	return ret;
}

static void amtsu_ctrl_remove(struct platform_device *pdev)
{
	struct amtsu_ctrl *ctrl = platform_get_drvdata(pdev);

	for (int idx = 0; idx < AMTSU_DEVICES; idx++) {
		struct amtsu_ctrl_device *dev = ctrl->devices[idx];

		if (dev) {
			amtsu_unregister_device(&dev->adev);
		}
	}
}

static const struct of_device_id amtsu_ctrl_match[] = {
	{ .compatible = "xrarch,amtsu" },
	{ },
};
MODULE_DEVICE_TABLE(of, amtsu_ctrl_match);

static struct platform_driver amtsu_ctrl_driver = {
	.probe	= amtsu_ctrl_probe,
	.remove	= amtsu_ctrl_remove,
	.driver	= {
		.name		= "amtsu",
		.of_match_table	= amtsu_ctrl_match,
	},
};
module_platform_driver(amtsu_ctrl_driver);

static ssize_t model_id_show(struct device *_d, struct device_attribute *attr,
			     char *buf)
{
	struct amtsu_device *dev = dev_to_amtsu(_d);
	return sysfs_emit(buf, "0x%04x\n", dev->id.model_id);
}
static DEVICE_ATTR_RO(model_id);

static struct attribute *amtsu_dev_attrs[] = {
	&dev_attr_model_id.attr,
	NULL
};
ATTRIBUTE_GROUPS(amtsu_dev);

static int amtsu_dev_match(struct device *_dv, const struct device_driver *_dr)
{
	struct amtsu_device *dev = dev_to_amtsu(_dv);
	const struct amtsu_device_id *ids = drv_to_amtsu(_dr)->id_table;

	for (unsigned int i = 0; ids[i].model_id; i++) {
		if (ids[i].model_id == dev->id.model_id)
			return 1;
	}

	return 0;
}

static int amtsu_dev_probe(struct device *_d)
{
	struct amtsu_device *dev = dev_to_amtsu(_d);
	struct amtsu_driver *drv = drv_to_amtsu(dev->dev.driver);

	return drv->probe(dev);
}

static void amtsu_dev_remove(struct device *_d)
{
	struct amtsu_device *dev = dev_to_amtsu(_d);
	struct amtsu_driver *drv = drv_to_amtsu(dev->dev.driver);

	drv->remove(dev);
}

static void amtsu_dev_shutdown(struct device *_d)
{
	struct amtsu_device *dev = dev_to_amtsu(_d);
	struct amtsu_driver *drv = drv_to_amtsu(dev->dev.driver);

	if (!drv)
		return;

	if (drv->shutdown) {
		drv->shutdown(dev);
		return;
	}

	dev->bus->disable_irq(dev);
}

static const struct bus_type amtsu_bus = {
	.name		= "amtsu",
	.dev_groups	= amtsu_dev_groups,
	.match		= amtsu_dev_match,
	.probe		= amtsu_dev_probe,
	.remove		= amtsu_dev_remove,
	.shutdown	= amtsu_dev_shutdown,
};

static int amtsu_register_device(struct amtsu_device *dev)
{
	dev->dev.bus = &amtsu_bus;
	device_initialize(&dev->dev);

	int ret = ida_alloc(&amtsu_index_ida, GFP_KERNEL);
	if (ret < 0)
		return ret;
	dev->idx = ret;

	ret = dev_set_name(&dev->dev, "amtsu%u", dev->idx);
	if (ret)
		goto fail_free_ida;

	ret = device_add(&dev->dev);
	if (ret)
		goto fail_free_ida;

	return 0;

fail_free_ida:
	ida_free(&amtsu_index_ida, dev->idx);
	return ret;
}

static void amtsu_unregister_device(struct amtsu_device *dev)
{
	ida_free(&amtsu_index_ida, dev->idx);
}

int __amtsu_driver_register(struct amtsu_driver *driver, struct module *owner)
{
	driver->driver.bus = &amtsu_bus;
	driver->driver.owner = owner;

	return driver_register(&driver->driver);
}

void amtsu_driver_unregister(struct amtsu_driver *driver)
{
	driver_unregister(&driver->driver);
}

static int amtsu_init(void)
{
	if (bus_register(&amtsu_bus))
		panic("amtsu bus registration failed");
	return 0;
}

static void __exit amtsu_exit(void)
{
	bus_unregister(&amtsu_bus);
	ida_destroy(&amtsu_index_ida);
}

core_initcall(amtsu_init);
module_exit(amtsu_exit);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("Amtsu bus driver");
MODULE_LICENSE("GPL");
