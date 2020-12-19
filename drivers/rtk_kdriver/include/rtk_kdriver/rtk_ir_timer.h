#ifndef __RTK_IR_TIMER_H__
#define __RTK_IR_TIMER_H__
#include <mach/irqs.h>
#include <mach/timex.h>
#define IR_HW_TIMER_ID   2
#define IR_HW_TIMER_IRQ  SPI_MISC_TIMER2 
#define HW_TIMER_CNT_PER_US  27  // (27000000 / 1000000)

typedef void (*TIMER_CALLBACK_FUNC)(void);

int rtk_ir_setup_timer(unsigned int us);
int rtk_ir_cancel_timer(void);
int __init rtk_ir_timer_init(TIMER_CALLBACK_FUNC callback);
void __exit rtk_ir_timer_exit(void);
#endif