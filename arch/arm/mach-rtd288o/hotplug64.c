/*
 *
 *  Copyright (C) 2011, Realtek
 *  All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <asm/cacheflush.h>
#include <mach/common.h>
#include <mach/rtk_platform.h>
#include <linux/types.h>

#include <asm/cacheflush.h>
#include <asm/cpu_ops.h>
#include <asm/cputype.h>
#include <asm/io.h>
#include <asm/smp_plat.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>

#define dcache_enable()							\
	__asm__ __volatile__ (						\
		"       mrs	x0, sctlr_el1 \n"			\
		"	orr	x0, x0, #1 << 2			// clear SCTLR.C \n" \
		"	msr	sctlr_el1, x0 \n"			\
		"       isb	\n"					\	
		"	dmb	sy 				// ensure ordering with previous memory accesses \n" \
		::: "x0")
#define dcache_disable()							\
	__asm__ __volatile__ (						\
		"       mrs	x0, sctlr_el1 \n"			\
		"	bic	x0, x0, #1 << 2			// clear SCTLR.C \n" \
		"	msr	sctlr_el1, x0 \n"			\
		"       isb	\n"					\	
		"	dmb	sy 				// ensure ordering with previous memory accesses \n" \
		::: "x0")

/*
 *	__flush_dcache_all()
 *
 *	Flush the whole D-cache.
 *
 *	Corrupted registers: x0-x7, x9-x11
 */

#define LEVEL1 "1"
#define LEVEL2 "3"
#define LEVEL3 "5"
#define dcache_disable_and_flush_dcache(level)				\
			 __asm__ __volatile__ (				\
		"	dmb	sy 				// ensure ordering with previous memory accesses \n" \
		"       mrs	x0, clidr_el1	 	       // read clidr  \n" \
		"	mov	x3, "  level  "		// left align loc bit field \n" \
		"	cbz	x3, finished%=			// if loc is 0, then no need to clean \n" \
		"	mov	x10, #0				// start clean at cache level 0 \n" \
		"loop1%=: \n"						\
		"	add	x2, x10, x10, lsr #1		// work out 3x current cache level \n" \
		"	lsr	x1, x0, x2			// extract cache type bits from clidr \n" \
		"	and	x1, x1, #7			// mask of the bits for current cache only \n" \
		"	cmp	x1, #2				// see what cache we have at this level \n" \
		"	b.lt	skip%=				// skip if no cache, or just i-cache \n" \
		"	// save_and_disable_irqs x9		// make CSSELR and CCSIDR access atomic \n" \ 
		"	msr	csselr_el1, x10			// select current cache level in csselr \n" \
		"	isb					// isb to sych the new cssr&csidr \n" \
		"	mrs	x1, ccsidr_el1			// read the new ccsidr \n" \
		"	// restore_irqs x9 \n"				\
		"	and	x2, x1, #7			// extract the length of the cache lines \n" \
		"	add	x2, x2, #4			// add 4 (line length offset) \n" \
		"	mov	x4, #0x3ff \n"				\
		"	and	x4, x4, x1, lsr #3		// find maximum number on the way size \n" \
		"	clz	w5, w4				// find bit position of way size increment \n" \
		"	mov	x7, #0x7fff \n"				\
		"	and	x7, x7, x1, lsr #13		// extract max number of the index size \n" \
		"loop2%=: \n"						\
		"	mov	x9, x7				// create working copy of max index size \n" \
		"loop3%=: \n"						\
		"	lsl	x6, x4, x5 \n"				\
		"	orr	x11, x10, x6			// factor way and cache number into x11 \n" \
		"	lsl	x6, x9, x2 \n"				\
		"	orr	x11, x11, x6			// factor index number into x11 \n" \
		"	dc	cisw, x11			// clean & invalidate by set/way \n" \
		"	subs	x9, x9, #1			// decrement the index \n" \
		"	b.ge	loop3%= \n"				\
		"	subs	x4, x4, #1			// decrement the way \n" \
		"	b.ge	loop2%= \n"				\
		"skip%=: \n"						\
		"	add	x10, x10, #2			// increment cache number \n" \
		"	cmp	x3, x10 \n"				\
		"	b.gt	loop1%= \n"				\
		"finished%=: \n"						\
		"	mov	x10, #0				// swith back to cache level 0 \n" \
		"	msr	csselr_el1, x10			// select current cache level in csselr \n" \
		"	dsb	sy \n"					\
		"	isb \n"						\
		::: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x9", "x10", "x11")

			
	
unsigned long cpu_addr[]={
	0x500,
	0x508,
	0x510,
	0x518
};

static inline void cpu_enter_lowpower(void)
{
        flush_cache_all();
}

static inline void cpu_leave_lowpower(void)
{

}


static inline void platform_do_lowpower(unsigned int cpu, int *spurious)
{
	extern volatile unsigned long secondary_holding_pen_release;;
	extern void secondary_holding_pen(void);
	long logic_cpu = cpu_logical_map(cpu);
	unsigned long return_addr = __pa(secondary_holding_pen);
	/*
         * there is no power-control hardware on this platform, so all
         * we can do is put the core into WFI; this is safe as the calling
         * code will have already disabled interrupts
         */
        __le64 __iomem *release_addr;

        /*
         * The cpu-release-addr may or may not be inside the linear mapping.
         * As ioremap_cache will either give us a new mapping or reuse the
         * existing linear mapping, we can use it to cover both cases. In
         * either case the memory will be MT_NORMAL.
         */
#if 0
	release_addr = ioremap_cache(cpu_addr[cpu],
				     sizeof(*release_addr));
#else
	release_addr = phys_to_virt(cpu_addr[cpu]);
	BUG_ON(!release_addr);
#endif 	
	printk("_lowpower: secondary_holding_pen_release(0x%x) logic_cpu(0x%x)"
	       "release_addr(0x%lx) return_addr(0x%lx) \n",
	       secondary_holding_pen_release, logic_cpu, 
	       (*(volatile long *)release_addr, return_addr));

        for (;;) {
		extern void secondary_holding_pen(void);
		long holding_pen_release = __pa(secondary_holding_pen);
                /*
                 * here's the WFI
                 */
#if 0		
		dcache_disable();
		dcache_disable_and_flush_dcache(LEVEL1);
		wfi();
                if (((secondary_holding_pen_release == logic_cpu)
		     && (*(volatile long *)release_addr == return_addr)))  {
			dcache_enable();
 			__asm__(
				"       mov	sp, %0\n"
				"	mov	x29, #0\n"
				"	b	secondary_start_kernel"
 				:
 				: "r" (task_stack_page(current) + THREAD_START_SP));
		}
		+		}
#else
		dcache_disable();
		dcache_disable_and_flush_dcache(LEVEL1);
		asm (
			"repeat_wfi: \n"
			"isb \n"
			"wfi \n"
			"nop \n"
			"nop \n"
			"ldr x0, [%0] \n"
			"cmp x0, %1 \n"
			"bne repeat_wfi \n"
			"ldr x0, [%2] \n"
			"cmp x0, %3 \n"
			"bne repeat_wfi \n"

			"       mrs	x0, sctlr_el1 \n"		
			"	orr	x0, x0, #1 << 2			// clear SCTLR.C \n" 
			"	msr	sctlr_el1, x0 \n"		
			"       isb	\n"
			"	dmb	sy 				// ensure ordering with previous memory accesses \n" 
			
			"       mov	sp, %4\n"
			"	mov	x29, #0\n"
			"	b	secondary_start_kernel"
			:
			:"r" (&secondary_holding_pen_release), "r"(logic_cpu),
			 "r" (release_addr), "r"(return_addr),
			 "r" (task_stack_page(current) + THREAD_START_SP)
			:"x0");
		
#endif		


                /*
                 * Getting here, means that we have come out of WFI without
                 * having been woken up - this shouldn't happen
                 *
                 * Just note it happening - when we're woken, we can report
                 * its occurrence.
                 */
                (*spurious)++;
        }
//	iounmap(release_addr);
}


int platform_cpu_kill(unsigned int cpu)
{
	
	return 0;
}

int rtk_optee_cpu_suspend(int level);
/*
 * platform-specific code to shutdown a CPU
 *
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{

		
	/*
	 * we're ready for shutdown now, so do it
	 */
#if  defined(CONFIG_OPTEE)
#if defined(CONFIG_LG_SNAPSHOT_BOOT)
        extern bool reserve_boot_memory;
        if (!reserve_boot_memory)
#else
        if (1)
#endif
	{
		int ret;

		/** level:
	 	* 0, core
	 	* 1, cluster
	 	* 2, system/platform. if success, smc do not return.
	 	**/
	
		printk("optee_cpu_suspend: cpu: 0x%x \n", cpu);

		ret = rtk_optee_cpu_suspend((cpu != 0) ? 0 : 2);

		printk("optee_cpu_suspend: ERROR!! should not return 0x%x \n", ret);
		asm ("1: b 1b \n");
	}
	else
	{
		int spurious=0;
		cpu_enter_lowpower();
		platform_do_lowpower(cpu, &spurious);
		if (spurious != 0)
			printk("cpu(%d) wfi wakeup spurious: %d \n", cpu, spurious);
		cpu_leave_lowpower();
	}
#endif
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
