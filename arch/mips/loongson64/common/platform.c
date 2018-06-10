/*
 * Copyright (C) 2009 Lemote Inc.
 * Author: Wu Zhangjin, wuzhangjin@gmail.com
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/err.h>
#include <linux/smp.h>
#include <linux/platform_device.h>
#include <boot_param.h>
#include <loongson.h>

static struct platform_device loongson2_cpufreq_device = {
	.name = "loongson2_cpufreq",
	.id = -1,
};

static struct platform_device loongson3_cpufreq_device = {
	.name = "loongson3_cpufreq",
	.id = -1,
};

static int __init loongson_cpufreq_init(void)
{
#ifdef CONFIG_LEFI_FIRMWARE_INTERFACE
if (!loongson_freqctrl)
	return -ENODEV;
return platform_device_register(&loongson3_cpufreq_device);
#else

	/* Only 2F revision and it's successors support CPUFreq */
	if ((c->processor_id & PRID_REV_MASK) == PRID_REV_LOONGSON2F)
		return platform_device_register(&loongson2_cpufreq_device);


	return -ENODEV;
#endif
}

arch_initcall(loongson_cpufreq_init);
