/*
 *  Copyright (C) 2013, Loongson Technology Corporation Limited, Inc.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 */
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>
#include <asm/mipsregs.h>
#include <asm/delay.h>
#include <boot_param.h>
#include <linux/cpumask.h>
#include <irq.h>
#include <loongson.h>
#include <loongson2k.h>


#define LS2K_IPS (CAUSEF_IP2 | CAUSEF_IP3 | CAUSEF_IP5)
#define UNUSED_IPS (CAUSEF_IP0 | CAUSEF_IP1 | CAUSEF_IP4)

void mach_irq_dispatch(unsigned int pending)
{
	if (pending & CAUSEF_IP7)
		do_IRQ(MIPS_CPU_IRQ_BASE + 7);
#ifdef CONFIG_SMP
	if (pending & CAUSEF_IP6)
		lemote2k_ipi_interrupt(NULL);
#endif
	if (pending & LS2K_IPS)
		loongson_pch->irq_dispatch();
	if (pending & UNUSED_IPS)
		spurious_interrupt();
}

struct irqaction cascade_irqaction = {
	.handler	= no_action,
	.name		= "cascade",
	.flags		= IRQF_NO_THREAD,
};

static inline void mask_lssoc_irq(struct irq_data *d) { }
static inline void unmask_lssoc_irq(struct irq_data *d) { }

 /* For MIPS IRQs which shared by all cores */
static struct irq_chip lssoc_irq_chip = {
	.name		= "Loongson-SoC",
	.irq_ack	= mask_lssoc_irq,
	.irq_mask	= mask_lssoc_irq,
	.irq_mask_ack	= mask_lssoc_irq,
	.irq_unmask	= unmask_lssoc_irq,
	.irq_eoi	= unmask_lssoc_irq,
};


void __init mach_init_irq(void)
{
	irqchip_init()

	/* machine specific irq init */
	loongson_soc->init_irq();

	setup_irq(MIPS_CPU_IRQ_BASE + 2, &cascade_irqaction);
	setup_irq(MIPS_CPU_IRQ_BASE + 3, &cascade_irqaction);
	setup_irq(MIPS_CPU_IRQ_BASE + 5, &cascade_irqaction);

	set_c0_status(STATUSF_IP2 | STATUSF_IP3 | STATUSF_IP6);
}
