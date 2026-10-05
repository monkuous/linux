// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Regents of the University of California
 * Copyright (C) 2017 SiFive
 */

#include <linux/of_clk.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
#include <linux/delay.h>
#include <asm/processor.h>

void __init time_init(void)
{
	of_clk_init(NULL);

	timer_probe();

	tick_setup_hrtimer_broadcast();
}
