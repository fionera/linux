#ifndef __ARCH_LG1K_COMMON_H
#define __ARCH_LG1K_COMMON_H

#define REG(n)	((n) << 2)

#ifndef CONFIG_CACHE_L2X0
static inline void lg115x_init_l2cc(void) { }
#else
extern void __init lg115x_init_l2cc(void);
#endif

#ifndef CONFIG_CMA
static inline void lg1k_reserve_cma(void) { }
#else
extern void __init lg1k_reserve_cma(void);
#endif

#ifndef CONFIG_SMP
static inline void lg1k_reserve_smp(void) { }
#else
extern void __init lg1k_reserve_smp(void);
#endif

#ifndef CONFIG_SPI_PL022
static inline void lg115x_init_spi(void) { }
#else
extern void __init lg115x_init_spi(void);
#endif

#ifndef CONFIG_ETHERNET
static inline void lg115x_reserve_eth(void) { }
#else
extern void __init lg115x_reserve_eth(void);
#endif
#endif	/* __ARCH_LG1K_COMMON_H */
