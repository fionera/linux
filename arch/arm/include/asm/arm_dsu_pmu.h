/*
 * ARM DynamIQ Shared Unit (DSU) PMU Low level register access routines.
 *
 * Copyright (C) ARM Limited, 2017.
 *
 * Author: Suzuki K Poulose <suzuki.poulose@arm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 */

#include <asm/cp15.h>
#include <linux/bitops.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0))
#include <linux/build_bug.h>
#else
#include <linux/bug.h>
#endif
#include <linux/compiler.h>
#include <linux/types.h>
#include <asm/barrier.h>

static inline u32 __dsu_pmu_read_pmcr(void)
{
 	u32 val;
	asm volatile("mrc p15, 0, %0, c15, c5, 0" : "=r"(val));
	return val;
}

static inline void __dsu_pmu_write_pmcr(u32 val)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 0" : : "r"(val));
	isb();
}

static inline u32 __dsu_pmu_get_reset_overflow(void)
{
	u32 val;

	asm volatile("mrc p15, 0, %0, c15, c5, 4" : "=r" (val));
	/* Clear the bit */
	asm volatile("mcr p15, 0, %0, c15, c5, 4" : : "r" (val));
	isb();
	return val;
}

static inline void __dsu_pmu_select_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 5" : : "r" (counter));
	isb();
}

static inline u64 __dsu_pmu_read_counter(int counter)
{
    u32 value = 0;
	__dsu_pmu_select_counter(counter);
    asm volatile("mrc p15, 0, %0, c15, c6, 2" : "=r" (value));
	return (u64)value;
}

static inline void __dsu_pmu_write_counter(int counter, u64 val)
{
    u32 value = (u32)val;
	__dsu_pmu_select_counter(counter);
    asm volatile("mcr p15, 0, %0, c15, c6, 2" : : "r" (val));
	isb();
}

static inline void __dsu_pmu_set_event(int counter, u32 event)
{
	__dsu_pmu_select_counter(counter);
	asm volatile("mcr p15, 0, %0, c15, c6, 1" : : "r" (event));
	isb();
}

static inline u64 __dsu_pmu_read_pmccntr(void)
{
    u64 value;
    asm volatile("mrc p15, 0, %0, c15, c6, 0" : "=r" (value));
	return value;
}

static inline void __dsu_pmu_write_pmccntr(u64 val)
{
    asm volatile("mcr p15, 0, %0, c15, c6, 0" : : "r" (val));
	isb();
}

static inline void __dsu_pmu_disable_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 2" : : "r" (BIT(counter)));
	isb();
}

static inline void __dsu_pmu_enable_counter(int counter)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 1" : : "r" (BIT(counter)));
	isb();
}

static inline void __dsu_pmu_counter_interrupt_enable(int counter)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 6" : : "r" (BIT(counter)));
	isb();
}

static inline void __dsu_pmu_counter_interrupt_disable(int counter)
{
	asm volatile("mcr p15, 0, %0, c15, c5, 7" : : "r" (BIT(counter)));
	isb();
}


static inline u32 __dsu_pmu_read_pmceid(int n)
{
    u32 value;
	switch (n) {
	case 0:
        asm volatile("mrc p15, 0, %0, c15, c6, 4" : "=r" (value));
		return value;
	case 1:
        asm volatile("mrc p15, 0, %0, c15, c6, 5" : "=r" (value));
		return value;
	default:
		BUILD_BUG();
		return 0;
	}
}
