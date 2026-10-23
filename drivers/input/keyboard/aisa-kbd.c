/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/amtsu.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/keyboard.h>
#include <linux/module.h>

#define AISA_KBD_SCANCODES	86

#define AISA_KBD_POP		0x01
#define AISA_KBD_RESET		0x02

static const unsigned short aisa_kbd_keycodes[AISA_KBD_SCANCODES] = {
	[0]	= KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
		  KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
		  KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
	[26]	= KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8,
		  KEY_9,
	[36]	= KEY_SEMICOLON, KEY_SPACE, KEY_TAB,
	[39]	= KEY_MINUS, KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE,
		  KEY_BACKSLASH,
	[45]	= KEY_SLASH, KEY_DOT, KEY_APOSTROPHE, KEY_COMMA, KEY_GRAVE,
	[50]	= KEY_ENTER, KEY_BACKSPACE, KEY_CAPSLOCK, KEY_ESC,
	[54]	= KEY_LEFT, KEY_RIGHT, KEY_DOWN, KEY_UP,
	[80]	= KEY_LEFTCTRL, KEY_RIGHTCTRL, KEY_LEFTSHIFT, KEY_RIGHTSHIFT,
		  KEY_LEFTALT, KEY_RIGHTALT,
};

static irqreturn_t aisa_kbd_int(int irq, void *dev_id)
{
	struct amtsu_device *adev = dev_id;
	struct input_dev *dev = adev->priv;

	for (;;) {
		u32 scancode;
		adev->bus->command(adev, AISA_KBD_POP, AMTSU_CMD_READ_A,
				   &scancode, NULL);
		if (scancode == 0xffff)
			break;

		input_event(dev, EV_MSC, MSC_RAW, scancode);

		bool released = scancode & 0x8000;
		scancode = scancode & 0x7fff;

		if (scancode >= ARRAY_SIZE(aisa_kbd_keycodes))
			goto sync;

		unsigned short keycode = aisa_kbd_keycodes[scancode];
		if (!keycode)
			goto sync;

		input_event(dev, EV_MSC, MSC_SCAN, scancode);
		input_event(dev, EV_KEY, keycode, !released);
	sync:
		input_sync(dev);
	}

	return IRQ_HANDLED;
}

static int aisa_kbd_probe(struct amtsu_device *adev)
{
	struct input_dev *dev = devm_input_allocate_device(&adev->dev);
	if (!dev)
		return -ENOMEM;

	dev->name = dev_name(&adev->dev);
	dev->phys = "aisa-kbd/input0";
	dev->id.bustype = BUS_AMTSU;
	dev->id.vendor = 0x0001;
	dev->id.product = 0x0001;
	dev->id.version = 0x0100;

	dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP) | BIT_MASK(EV_MSC);

	for (int i = 0; i < AISA_KBD_SCANCODES; i++)
		set_bit(i, dev->keybit);

	dev->mscbit[0] = BIT_MASK(MSC_RAW);

	adev->priv = dev;

	int ret;
	dev->keycode = kmalloc(sizeof(aisa_kbd_keycodes), GFP_KERNEL);
	if (!dev->keycode) {
		ret = -ENOMEM;
		goto fail_free_dev;
	}
	dev->keycodesize = sizeof(*aisa_kbd_keycodes);
	dev->keycodemax = ARRAY_SIZE(aisa_kbd_keycodes);
	memcpy(dev->keycode, aisa_kbd_keycodes, dev->keycodemax);

	for (int i = 0; i < ARRAY_SIZE(aisa_kbd_keycodes); i++) {
		if (aisa_kbd_keycodes[i] != KEY_RESERVED) {
			__set_bit(aisa_kbd_keycodes[i], dev->keybit);
		}
	}

	adev->bus->command(adev, AISA_KBD_RESET, 0, NULL, NULL);

	ret = devm_request_irq(&adev->dev, adev->irq, aisa_kbd_int, 0,
			       "aisa-keyboard", adev);
	if (ret)
		goto fail_free_keycode;

	ret = input_register_device(dev);
	if (ret)
		goto fail_free_irq;

	adev->bus->enable_irq(adev);

	return 0;

fail_free_irq:
	devm_free_irq(&adev->dev, adev->irq, dev);
fail_free_keycode:
	kfree(dev->keycode);
fail_free_dev:
	input_free_device(dev);
	return ret;
}

static const struct amtsu_device_id aisa_kbd_ids[] = {
	{ .model_id = 0x8fc48fc4 },
	{ },
};
MODULE_DEVICE_TABLE(amtsu, aisa_kbd_ids);

static struct amtsu_driver aisa_kbd_driver = {
	.driver.name = "aisa-keyboard",
	.id_table = aisa_kbd_ids,
	.probe = aisa_kbd_probe,
};

static int __init aisa_kbd_init(void)
{
	return amtsu_driver_register(&aisa_kbd_driver);
}

static void __exit aisa_kbd_exit(void)
{
	amtsu_driver_unregister(&aisa_kbd_driver);
}

module_init(aisa_kbd_init);
module_exit(aisa_kbd_exit);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("XR/arch AISA keyboard driver");
MODULE_LICENSE("GPL");
