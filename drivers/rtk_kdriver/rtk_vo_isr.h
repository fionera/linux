#ifndef _LINUX_RTK_VO_ISR_H
#define _LINUX_RTK_VO_ISR_H

#include <linux/module.h>
#include <linux/kernel.h>

typedef struct {
	int dummy;
} dummy_vo_isr_struct;

/* Use 'O' as magic number */
#define VO_ISR_IOC_MAGIC  'O'

#define VO_ISR_IOCT_DUMMY		_IOW(VO_ISR_IOC_MAGIC, 1, dummy_vo_isr_struct)


#define WarningSec( _sec, format, ...)      \
{\
  static unsigned int  LastPrintPTS = 0;			\
  static unsigned int  WarningCount = 0;			\
  unsigned int	     CurrentPTS = (*(volatile unsigned int *)TIMER_VCPU_CLK90K_LO_reg );	\
  if( CurrentPTS - LastPrintPTS > 90090 * _sec )	\
  {	\
    printk(KERN_NOTICE "[%d]:", WarningCount+1);	\
    printk(KERN_NOTICE format, ##__VA_ARGS__);	\
    LastPrintPTS = CurrentPTS;	\
    WarningCount = 0;		\
  }	\
  else	\
    WarningCount ++;	\
}


#endif
