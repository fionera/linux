#ifndef __RTK_V4L2_PWM_H__
#define __RTK_V4L2_PWM_H__

#include <mach/rtk_log.h>
#include <linux/interrupt.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/v4l2-ext/v4l2-controls-ext.h>
#include <linux/v4l2-ext/videodev2-ext.h>
//#include <tvscalercontrol/scaler_v4l2/V4l2_vbe_ext.h>
#include <linux/videodev2.h>
#include <asm/uaccess.h>

extern  int rtk_v4l2_pwm_init(void);
extern  int rtk_v4l2_pwm_set_duty(struct v4l2_ext_vbe_pwm_duty *v4l2_pwm_duty);


extern int rtk_v4l2_pwm_ApplyParam(enum v4l2_ext_vbe_pwm_pin_sel_mask pwmIndex);
extern int rtk_v4l2_pwm_ApplyParamSet(struct v4l2_ext_vbe_pwm_param *v4l2_pwm_param);

extern int rtk_v4l2_pwm_ApplyParamGet(struct v4l2_ext_vbe_pwm_param *v4l2_pwm_param);

#endif


