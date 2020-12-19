#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/pwm.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <mach/rtk_platform.h>
#include <mach/pcbMgr.h>
#include <linux/fs.h> /*file operators*/
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <rtk_kdriver/rtk_gpio.h>
#include <rtk_kdriver/rtk_pwm-reg.h>
#include <rtk_kdriver/rtk_pwm_crt.h>
#include <rtk_kdriver/rtk_pwm_func.h>
#include <rtk_kdriver/rtk_pwm_local_dimming.h>
#include <rtk_kdriver/rtk_pwm_attr.h>
#include <rtk_kdriver/rtk_v4l2_pwm.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

static int _v4l2_pwm_get_all_param_ex(R_CHIP_T *pchip2,struct v4l2_ext_vbe_pwm_param* param_info)
{
    int ret = PWM_SUCCESS;
    
    if (m_ioctl_disable_get())
        return PWM_SUCCESS;    

    param_info->pstPWMParam->pwm_duty = pchip2->rtk_duty;
    param_info->pstPWMParam->pwm_adapt_freq_param.pwm_adapt_freq_enable = pchip2->rtk_adpt_en;
    param_info->pstPWMParam->pwm_frequency = pchip2->rtk_freq;
    param_info->pstPWMParam->pwm_enable = pchip2->rtk_enable;
    param_info->pstPWMParam->pwm_pos_start = pchip2->rtk_pos_start;
    param_info->pstPWMParam->pwm_lock = pchip2->rtk_vsync;
    param_info->pstPWMParam->pwm_adapt_freq_param.pwmfreq_48nHz = pchip2->lg_freq48n;
    param_info->pstPWMParam->pwm_adapt_freq_param.pwmfreq_50nHz = pchip2->lg_freq50n;
    param_info->pstPWMParam->pwm_adapt_freq_param.pwmfreq_60nHz = pchip2->lg_freq60n;

    PWM_INFO("pwm index %d  pwm_duty:%d pwm_freq:%d\n",param_info->pwmIndex ,
                                                                        param_info->pstPWMParam->pwm_duty, param_info->pstPWMParam->pwm_frequency);
    return ret;
}

int rtk_v4l2_pwm_init(void)
{
    return 0;
}

int rtk_v4l2_pwm_set_duty(struct v4l2_ext_vbe_pwm_duty *v4l2_pwm_duty)
{
    int ret = PWM_SUCCESS;
    RTK_PWM_PARAM_EX_T param;
    int index;
    int pwmindex = v4l2_pwm_duty->pwmIndex;
    R_CHIP_T *pchip2[5];
    
    if (m_ioctl_disable_get())
        return PWM_SUCCESS;

    for(index=0; index<5; index++)
    {
        if(pwmindex & 0x01)
        {
            pchip2[index] = rtk_pwm_chip_get_by_index(index,PWM_MISC);
            if(pchip2[index]  == NULL){
                ret = (-PWM_PCB_ENUM_ERROR);
                PWM_ERR("PWM %d ,index:%d chip get error!\n",v4l2_pwm_duty->pwmIndex,index);
                return ret;
            }
            param.id = PWM_PARAM_DUTY;
            param.val = v4l2_pwm_duty->pwm_duty;
            ret = rtk_pwm_set_param_ex(pchip2[index],&param);
        }
        pwmindex = pwmindex >> 1;
    }
    return ret;

}


int rtk_v4l2_pwm_ApplyParamSet(struct v4l2_ext_vbe_pwm_param *v4l2_pwm_param)
{
    RTK_PWM_INFO_T stpwminfo;
    stpwminfo.m_index = v4l2_pwm_param->pwmIndex;
    stpwminfo.m_run = v4l2_pwm_param->pstPWMParam->pwm_enable;
    stpwminfo.m_duty = v4l2_pwm_param->pstPWMParam->pwm_duty;
    stpwminfo.m_freq = v4l2_pwm_param->pstPWMParam->pwm_frequency;
    stpwminfo.m_adpt_en = v4l2_pwm_param->pstPWMParam->pwm_adapt_freq_param.pwm_adapt_freq_enable;
    stpwminfo.m_freq48n = v4l2_pwm_param->pstPWMParam->pwm_adapt_freq_param.pwmfreq_48nHz;
    stpwminfo.m_freq50n = v4l2_pwm_param->pstPWMParam->pwm_adapt_freq_param.pwmfreq_50nHz;
    stpwminfo.m_freq60n = v4l2_pwm_param->pstPWMParam->pwm_adapt_freq_param.pwmfreq_60nHz;
    stpwminfo.m_vsync = v4l2_pwm_param->pstPWMParam->pwm_lock;
    stpwminfo.m_pos = v4l2_pwm_param->pstPWMParam->pwm_pos_start;
    
    if (m_ioctl_disable_get())
        return PWM_SUCCESS;

    PWM_INFO("%s index:%d pwm freq: %d ,duty:%d , output:%d lock:%d \n", __func__,v4l2_pwm_param->pwmIndex, v4l2_pwm_param->pstPWMParam->pwm_frequency,
    v4l2_pwm_param->pstPWMParam->pwm_duty,
    v4l2_pwm_param->pstPWMParam->pwm_enable,
    v4l2_pwm_param->pstPWMParam->pwm_lock);

    return rtk_pwm_set_info(&stpwminfo);
}

int rtk_v4l2_pwm_ApplyParamGet(struct v4l2_ext_vbe_pwm_param *v4l2_pwm_param)
{
    int ret = PWM_SUCCESS;

    R_CHIP_T *pchip2 = rtk_pwm_chip_get(v4l2_pwm_param->pwmIndex);
    if(pchip2 == NULL){
        ret = (-PWM_PCB_ENUM_ERROR);
        PWM_ERR("PWM %d chip get error!\n",v4l2_pwm_param->pwmIndex);
        return ret;
    }
    
    if (m_ioctl_disable_get())
        return PWM_SUCCESS;

    ret = _v4l2_pwm_get_all_param_ex(pchip2, v4l2_pwm_param);
    PWM_INFO("%s pchip index:%d index:%d pwm freq: %d ,duty:%d , output:%d lock:%d \n",__func__, pchip2->index,v4l2_pwm_param->pwmIndex,
                                        v4l2_pwm_param->pstPWMParam->pwm_frequency,
                                        v4l2_pwm_param->pstPWMParam->pwm_duty,
                                        v4l2_pwm_param->pstPWMParam->pwm_enable,
                                        v4l2_pwm_param->pstPWMParam->pwm_lock);
    return ret;
}

int rtk_v4l2_pwm_ApplyParam(enum v4l2_ext_vbe_pwm_pin_sel_mask pwmIndex_mask)
{
    int ret = PWM_SUCCESS;
    int index;
    int pwm_index;
    R_CHIP_T *pchip2;
    
    if (m_ioctl_disable_get())
        return PWM_SUCCESS;  

    for(index=0; index<MAX_PWM_NODE; index++)
    {
        if((1<<index)& pwmIndex_mask)
        {
            pwm_index = index;
            PWM_INFO("PWM index : %d\n",pwm_index);
            pchip2 = rtk_pwm_chip_get(pwm_index);
            if(pchip2  == NULL){
                ret = (-PWM_PCB_ENUM_ERROR);
                PWM_ERR("PWM %d ,index:%d chip get error!\n",pwm_index,index);
                return ret;
            }
            rtk_pwm_db_wb(pchip2);
        }
    }
    return ret;

}

