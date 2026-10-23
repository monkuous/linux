/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/amtsu.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyboard.h>
#include <linux/module.h>

#define AISA_MOUSE_POP		0x01
#define AISA_MOUSE_RESET	0x02

#define AISA_MOUSE_PRESSED	1
#define AISA_MOUSE_RELEASED	2
#define AISA_MOUSE_MOVED	3

#define AISA_BUTTON_LEFT	1
#define AISA_BUTTON_MIDDLE	2
#define AISA_BUTTON_RIGHT	3

static unsigned int aisa_button_to_input(u32 button)
{
	switch (button) {
	case AISA_BUTTON_LEFT:
		return BTN_LEFT;
	case AISA_BUTTON_MIDDLE:
		return BTN_MIDDLE;
	case AISA_BUTTON_RIGHT:
		return BTN_RIGHT;
	default:
		return KEY_UNKNOWN;
	}
}

static irqreturn_t aisa_mouse_int(int irq, void *dev_id)
{
	struct amtsu_device *adev = dev_id;
	struct input_dev *dev = adev->priv;

	for (;;) {
		u32 type;
		u32 arg;
		adev->bus->command(adev, AISA_MOUSE_POP,
				   AMTSU_CMD_READ_A | AMTSU_CMD_READ_B,
				   &type, &arg);
		if (type == 0)
			break;

		switch (type) {
		case AISA_MOUSE_PRESSED:
			input_report_key(dev, aisa_button_to_input(arg), 1);
			break;
		case AISA_MOUSE_RELEASED:
			input_report_key(dev, aisa_button_to_input(arg), 0);
			break;
		case AISA_MOUSE_MOVED: {
			int dx = (s16)((arg >> 16) & 0xffff);
			int dy = (s16)(arg & 0xffff);

			if (dx) {
				input_report_rel(dev, REL_X, dx);
			}

			if (dy) {
				input_report_rel(dev, REL_Y, dy);
			}

			break;
		}
		}

		input_sync(dev);
	}

	return IRQ_HANDLED;
}

static int aisa_mouse_probe(struct amtsu_device *adev)
{
	int ret;

	struct input_dev *dev = devm_input_allocate_device(&adev->dev);
	if (!dev)
		return -ENOMEM;

	dev->name = dev_name(&adev->dev);
	dev->phys = "aisa-mouse/input0";
	dev->id.bustype = BUS_AMTSU;
	dev->id.vendor = 0x0001;
	dev->id.product = 0x0002;
	dev->id.version = 0x0100;

	input_set_capability(dev, EV_KEY, BTN_LEFT);
	input_set_capability(dev, EV_KEY, BTN_MIDDLE);
	input_set_capability(dev, EV_KEY, BTN_RIGHT);

	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);

	adev->priv = dev;

	adev->bus->command(adev, AISA_MOUSE_RESET, 0, NULL, NULL);

	ret = devm_request_irq(&adev->dev, adev->irq, aisa_mouse_int, 0,
			       "aisa-mouse", adev);
	if (ret)
		goto fail_free_dev;

	ret = input_register_device(dev);
	if (ret)
		goto fail_free_irq;

	adev->bus->enable_irq(adev);

	return 0;

fail_free_irq:
	devm_free_irq(&adev->dev, adev->irq, dev);
fail_free_dev:
	input_free_device(dev);
	return ret;
}

static const struct amtsu_device_id aisa_mouse_ids[] = {
	{ .model_id = 0x4d4f5553 },
	{ },
};
MODULE_DEVICE_TABLE(amtsu, aisa_mouse_ids);

static struct amtsu_driver aisa_mouse_driver = {
	.driver.name = "aisa-mouse",
	.id_table = aisa_mouse_ids,
	.probe = aisa_mouse_probe,
};

static int __init aisa_mouse_init(void)
{
	return amtsu_driver_register(&aisa_mouse_driver);
}

static void __exit aisa_mouse_exit(void)
{
	amtsu_driver_unregister(&aisa_mouse_driver);
}

module_init(aisa_mouse_init);
module_exit(aisa_mouse_exit);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("XR/arch AISA mouse driver");
MODULE_LICENSE("GPL");
