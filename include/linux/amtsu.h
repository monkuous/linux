/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_AMTSU_H
#define _LINUX_AMTSU_H

#include <linux/device.h>
#include <linux/mod_devicetable.h>

#define AMTSU_CMD_WRITE_A	0x1 /* write A before command */
#define AMTSU_CMD_WRITE_B	0x2 /* write B before command */
#define AMTSU_CMD_READ_A	0x4 /* read A after command */
#define AMTSU_CMD_READ_B	0x8 /* read B after command */

struct amtsu_device;

/**
 * struct amtsu_bus_ops - bus operations for an amtsu device
 * @enable_irq: enable the irq of this device
 * @disable_irq: disable the irq of this device
 * @command: issue a command to this device
 * 	dev: the amtsu_device
 * 	cmd: the command to issue
 * 	flags: bitset for operations to perform before and after the command
 * 	a: location of the value of the A register
 * 	b: location of the value of the B register
 */
struct amtsu_bus_ops {
	void (*enable_irq)(struct amtsu_device *);
	void (*disable_irq)(struct amtsu_device *);
	void (*command)(struct amtsu_device *dev, u32 cmd, u32 flags, u32 *a,
			u32 *b);
};

struct amtsu_device {
	struct device dev;
	struct amtsu_device_id id;
	unsigned int idx;
	int irq;
	const struct amtsu_bus_ops *bus;
	void *priv;
};

#define dev_to_amtsu(_dev)	container_of_const(_dev, struct amtsu_device, dev)

struct amtsu_driver {
	struct device_driver driver;
	const struct amtsu_device_id *id_table;
	int (*probe)(struct amtsu_device *);
	void (*remove)(struct amtsu_device *);
	void (*shutdown)(struct amtsu_device *);
};

#define drv_to_amtsu(_drv)	container_of_const(_drv, struct amtsu_driver, driver)

#define amtsu_driver_register(drv) __amtsu_driver_register(drv, THIS_MODULE)
int __amtsu_driver_register(struct amtsu_driver *driver, struct module *owner);
void amtsu_driver_unregister(struct amtsu_driver *driver);

#endif /* _LINUX_AMTSU_H */
