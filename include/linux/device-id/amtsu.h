/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LINUX_DEVICE_ID_AMTSU_H
#define LINUX_DEVICE_ID_AMTSU_H

#ifdef __KERNEL__
#include <linux/types.h>
typedef unsigned long kernel_ulong_t;
#endif

/**
 * struct amtsu_device_id - Amtsu device identifier
 * @model_id: model ID
 * @driver_id: driver specific data
 */
struct amtsu_device_id {
	__u32 model_id;
	kernel_ulong_t driver_data;
};

#endif /* LINUX_DEVICE_ID_AMTSU_H */
