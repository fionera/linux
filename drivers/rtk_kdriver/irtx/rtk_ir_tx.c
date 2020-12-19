#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/compat.h>
#include <linux/sysfs.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/freezer.h>
#include <linux/kfifo.h>
#include <rtk_kdriver/rtk_gpio.h>
#include <rtk_kdriver/rtk_ir_tx.h>
#include <rtk_kdriver/rtk_pwm_func.h>

#ifdef CONFIG_KDRV_IRTX_USE_TIMER
#include <rtk_kdriver/rtk_ir_timer.h>
#endif

#define IRRC_MOD_GRAB_CPU_TIME_LIMIT 10000/*10ms*/

static DEFINE_MUTEX(g_irrc_mod_mutex);

#ifdef CONFIG_KDRV_IRTX_USE_TIMER  
static unsigned int g_irrc_mod_use_hw_timer = 0;
static unsigned int g_cur_system_time = 0;//us
static unsigned int g_cur_key_interval = 0;//us
static DECLARE_WAIT_QUEUE_HEAD(g_irrc_mode_wait);
static unsigned int g_irrc_mode_done = 0;
#endif
static IRRC_KEY_LIST *g_irrc_mod_key_list = NULL;
static unsigned int g_cur_key_num = 0;
static unsigned int g_cur_key_times = 0;
static unsigned int g_cur_key_bit_num = 0;
static unsigned int g_cur_modulation_mode = 0; /*0:tone mode, 1: envelop mode*/



#define IRRC_MODE_PWM_PIN_NAME "PIN_IR_MOD"
static unsigned int g_cur_modulation_freq = 38000;
static unsigned int g_cur_modulation_duty = 80;
static R_CHIP_T* g_pwm_chip_t = NULL;
static int irrc_mod_pwm_init(unsigned int pwm_freq, unsigned int duty);




#define irrc_mod_attr(_name) \
static struct kobj_attribute _name##_attr = {   \
        .attr   = {                                               \
                .name = __stringify(_name),            \
                .mode = 0644,                                \
        },                                                          \
        .show   = _name##_show,                      \
        .store  = _name##_store,                       \
}

static ssize_t irrc_mod_moduation_mode_show(struct kobject * kobj, struct kobj_attribute * attr, char *buf)
{
    buf[0] = 0;
    sprintf(buf, "0:tone mode, 1: envelop mode\n current mode:%s\n", 
		(g_cur_modulation_mode == 0) ? "tone mode" : "envelop mode");
    return strlen(buf);
}

static ssize_t irrc_mod_moduation_mode_store(struct kobject * kobj, struct kobj_attribute * attr, const char *buf, size_t n)
{
    unsigned int val;

    if (sscanf(buf, "%u", &val) == 1) {
	mutex_lock(&g_irrc_mod_mutex);
        g_cur_modulation_mode = !!val;
	mutex_unlock(&g_irrc_mod_mutex);
        return n;
    }
    return -EINVAL;
}
irrc_mod_attr(irrc_mod_moduation_mode);
static ssize_t irrc_mod_moduation_freq_show(struct kobject * kobj, struct kobj_attribute * attr, char *buf)
{
    buf[0] = 0;
    sprintf(buf, "current moduation freq:%u\n", g_cur_modulation_freq);
    return strlen(buf);
}
static ssize_t irrc_mod_moduation_freq_store(struct kobject * kobj, struct kobj_attribute * attr, const char *buf, size_t n)
{
    int ret = -EINVAL;
    unsigned int val;
	
    if (sscanf(buf, "%u", &val) == 1) {
	if(val < 12000 || val > 1000000) /*12k-1M*/
		return ret; 
	mutex_lock(&g_irrc_mod_mutex);
        g_cur_modulation_freq = val;
	if(irrc_mod_pwm_init(g_cur_modulation_freq, g_cur_modulation_duty) != 0) {
		rtd_printk(KERN_ERR, TAG_NAME,  "[IRRC_MOD]fail to set moduation freq\n");
		ret = -EIO;
	} else {
		ret = n;
	}
	mutex_unlock(&g_irrc_mod_mutex);
        return ret;
    }
    return ret;
}
irrc_mod_attr(irrc_mod_moduation_freq);


static ssize_t irrc_mod_moduation_duty_show(struct kobject * kobj, struct kobj_attribute * attr, char *buf)
{
    buf[0] = 0;
    sprintf(buf, "current moduation duty:%u\n", g_cur_modulation_duty);
    return strlen(buf);
}

static ssize_t irrc_mod_moduation_duty_store(struct kobject * kobj, struct kobj_attribute * attr, const char *buf, size_t n)
{
    int ret = -EINVAL;
    unsigned int val;

    if (sscanf(buf, "%u", &val) == 1) {
	if(val < 51 || val > 153) /**20%-60%*/
		return ret; 
	mutex_lock(&g_irrc_mod_mutex);
        g_cur_modulation_duty = val;
	if(irrc_mod_pwm_init(g_cur_modulation_freq, g_cur_modulation_duty) != 0) {
		rtd_printk(KERN_ERR, TAG_NAME,  "[IRRC_MOD]fail to set moduation duty\n");
		ret = -EIO;
	} else {
		ret = n;
	}
	mutex_unlock(&g_irrc_mod_mutex);
        return ret;
    }
    return ret;
}
irrc_mod_attr(irrc_mod_moduation_duty);

static struct attribute * irrc_mod_attrs[] = {
    &irrc_mod_moduation_mode_attr.attr,
    &irrc_mod_moduation_freq_attr.attr,	
    &irrc_mod_moduation_duty_attr.attr,	
    NULL,
};

static struct attribute_group irrc_mod_attr_group = {
    .attrs = irrc_mod_attrs,
};


static inline unsigned int get_cur_system_time(void)
{
    struct timeval tv;
    do_gettimeofday(&tv);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

static inline void irrc_mod_generate_signal(unsigned int level)
{
	if(level == 0) {
		if(!g_cur_modulation_mode)
			rtk_change_to_pwm_mode(g_pwm_chip_t);
		else
			rtk_change_to_gpio_mode(g_pwm_chip_t,GPIO_PIN_HIGH);
	} else {
		rtk_change_to_gpio_mode(g_pwm_chip_t,GPIO_PIN_LOW);
	}

}

#ifdef CONFIG_KDRV_IRTX_USE_TIMER 
void irrc_mod_key_htimer_func(void)
{
        unsigned long cur_sys_time = 0;
        unsigned long should_delay_time = 0;
	if(pm_freezing) {
		g_irrc_mode_done = 1;
		wake_up_interruptible(&g_irrc_mode_wait);
		return;
	}

        cur_sys_time = get_cur_system_time();
        if(g_cur_system_time +  g_cur_key_interval > cur_sys_time) {
            should_delay_time = (g_cur_system_time +  g_cur_key_interval -  cur_sys_time);
            should_delay_time = (should_delay_time > 60) ? 60 : should_delay_time;
        }

        if(should_delay_time)
            udelay(should_delay_time);


	g_cur_key_bit_num++;
	if(g_cur_key_bit_num >= g_irrc_mod_key_list->key[g_cur_key_num].key_bit_len) {
		g_cur_key_bit_num = 0;
		g_cur_key_times++;
		if(g_cur_key_times >= g_irrc_mod_key_list->key_times[g_cur_key_num]) {
			g_cur_key_times = 0;
			g_cur_key_num++;
                        if(g_cur_key_num >= g_irrc_mod_key_list->key_num) {
                		g_irrc_mode_done = 1;
                		wake_up_interruptible(&g_irrc_mode_wait);
                		return;
                	}
		}
	}

	

	irrc_mod_generate_signal(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][0]);
        if(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] > 60)
	    rtk_ir_setup_timer(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] - 60);
        else
            rtk_ir_setup_timer(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1]);
        
	g_cur_key_interval = g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1];
        g_cur_system_time = get_cur_system_time();
	return;
}
static int irrc_mod_generate_key_list_by_hw_timer(IRRC_KEY_LIST* p_key_list)
{
    unsigned long flags;
    int ret = -1;
    if(!p_key_list || !p_key_list->key_num)
        return -1;
    mutex_lock(&g_irrc_mod_mutex);

    rtk_ir_cancel_timer();

    g_irrc_mod_key_list = p_key_list;
    g_cur_key_num  = 0;
    g_cur_key_times = 0;
    g_cur_key_bit_num = 0;
    g_irrc_mode_done = 0;
    

    local_irq_save(flags);
    irrc_mod_generate_signal(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][0]);
    if(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] > 60)
	    rtk_ir_setup_timer(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] - 60);
        else
            rtk_ir_setup_timer(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1]);
    g_cur_key_interval = g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1];
    g_cur_system_time = get_cur_system_time();
    local_irq_restore(flags);
	
    wait_event_interruptible(g_irrc_mode_wait, (g_irrc_mode_done || (pm_freezing == true)));
	
    if(g_irrc_mode_done)
	ret = 0;

    rtk_ir_cancel_timer();
	
     irrc_mod_generate_signal(1);
     msleep(20);

    mutex_unlock(&g_irrc_mod_mutex);
    return ret;
}
#endif
static int irrc_mod_generate_key_list_by_grab_cpu(IRRC_KEY_LIST* p_key_list)
{
    unsigned long flags;
    unsigned int remain_us = 0;
    int ret = -1;
    if(!p_key_list || !p_key_list->key_num)
        return -1;
    mutex_lock(&g_irrc_mod_mutex);

    g_irrc_mod_key_list = p_key_list;
    g_cur_key_num  = 0;
    g_cur_key_times = 0;
    g_cur_key_bit_num = 0;
    
    local_irq_save(flags);
    while(1) {
        if(pm_freezing)
		break;
        
         irrc_mod_generate_signal(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][0]);
        if(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] > IRRC_MOD_GRAB_CPU_TIME_LIMIT) {
            local_irq_restore(flags);
            remain_us = g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] % IRRC_MOD_GRAB_CPU_TIME_LIMIT;
            msleep(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1] / IRRC_MOD_GRAB_CPU_TIME_LIMIT);
            if(remain_us)
                usleep_range(remain_us, remain_us);
            local_irq_save(flags);
        } else {
            udelay(g_irrc_mod_key_list->key[g_cur_key_num].key_bit_array[g_cur_key_bit_num][1]);
        }
        
        g_cur_key_bit_num++;
	if(g_cur_key_bit_num >= g_irrc_mod_key_list->key[g_cur_key_num].key_bit_len) {
		g_cur_key_bit_num = 0;
		g_cur_key_times++;
		if(g_cur_key_times >= g_irrc_mod_key_list->key_times[g_cur_key_num]) {
			g_cur_key_times = 0;
			g_cur_key_num++;
                        if(g_cur_key_num >= g_irrc_mod_key_list->key_num) {
                		break;
                	}
		}
	}
    }
    local_irq_restore(flags);	
    
    irrc_mod_generate_signal(1);
    msleep(20);

    mutex_unlock(&g_irrc_mod_mutex);
    return ret;
}


static inline int64_t IRRC_MOD_CLOCK_GetCount(void)
{
    unsigned int ptrlo = rtd_inl(TIMER_SCPU_CLK90K_LO_reg) ;
    unsigned int ptrhi = rtd_inl(TIMER_SCPU_CLK90K_HI_reg) ;
    int64_t ret = ptrlo | (((int64_t)ptrhi) << 32) ;
    return ret ;
}

static int irrc_mod_dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static int irrc_mod_dev_release(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t irrc_mod_dev_read(struct file *file,
                                 char __user *buff, size_t size, loff_t *ofst)
{
    return 0;
}

static ssize_t irrc_mod_dev_write(struct file *file,
                                  const char __user *buff, size_t size, loff_t *ofst)
{
    return 0;
}

static long irrc_mod_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

    int ret = -EFAULT;

    switch(cmd) {

        case IRRC_MOD_SET_GEN_KEY_SEQUENCE: {
		IRRC_KEY_LIST * tmp_key_list = NULL;
		tmp_key_list = kmalloc(sizeof(IRRC_KEY_LIST), GFP_KERNEL);
		if(!tmp_key_list)
			return -ENOMEM;
			
		if (arg == 0 || copy_from_user(tmp_key_list, (void *)arg, sizeof(IRRC_KEY_LIST))) {
			kfree(tmp_key_list);
                	return -EFAULT;
		}
#ifdef CONFIG_KDRV_IRTX_USE_TIMER         
		if(g_irrc_mod_use_hw_timer)
		    ret = irrc_mod_generate_key_list_by_hw_timer(tmp_key_list);
                else
#endif                    
                    ret = irrc_mod_generate_key_list_by_grab_cpu(tmp_key_list);
		kfree(tmp_key_list);
		return ret;
	}		
        default: {
            break;
        }

    }
    return ret;
}



#ifdef CONFIG_COMPAT
static long irrc_mod_dev_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    return irrc_mod_dev_ioctl(file, cmd, arg);
}
#endif

static struct file_operations irrc_mod_dev_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = irrc_mod_dev_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = irrc_mod_dev_compat_ioctl,
#endif
    .open = irrc_mod_dev_open,
    .read = irrc_mod_dev_read,
    .write = irrc_mod_dev_write,
    .release = irrc_mod_dev_release,
};

static struct miscdevice rtk_irrc_mod_miscdev = {
    MISC_DYNAMIC_MINOR,
    "irrc_mod",
    &irrc_mod_dev_fops,
};

//#define __IRRC_MOD_TEST__
#ifdef __IRRC_MOD_TEST__
static int thread_function(void *data)
{
	int i = 0, j = 0;
	IRRC_KEY_LIST  *key_list = NULL;
	key_list = kmalloc(sizeof(IRRC_KEY_LIST), GFP_KERNEL);
	if(!key_list)
		return -1;
	msleep(1000);
	key_list->key_num = 2;
		for(j = 0; j < 2; j++) {
			key_list->key[j].key_bit_len = 68;
			key_list->key[j].key_bit_array[0][0] = 0;
			key_list->key[j].key_bit_array[0][1] = 9000;
			key_list->key[j].key_bit_array[1][0] = 1;
			key_list->key[j].key_bit_array[1][1] = 4500;
			
			for(i = 0; i < 64; i++) {
				if((i % 2) ==0) {
					key_list->key[j].key_bit_array[i + 2][0] = 0;
					key_list->key[j].key_bit_array[i + 2][1] = 560;
				} else {
					key_list->key[j].key_bit_array[i + 2][0] = 1;
					key_list->key[j].key_bit_array[i + 2][1] = 1600;
				}
			}
			key_list->key[j].key_bit_array[66][0] = 0;
			key_list->key[j].key_bit_array[66][1] = 560;
			key_list->key[j].key_bit_array[67][0] = 1;
			key_list->key[j].key_bit_array[67][1] = 26000;
		}
	key_list->key_times[0] = 1;
	key_list->key_times[1] = 1;
	if(g_irrc_mod_use_hw_timer)
            irrc_mod_generate_key_list_by_hw_timer(key_list);
        else
            irrc_mod_generate_key_list_by_grab_cpu(key_list);
	kfree(key_list);
	return 0;
}
#endif


static int irrc_mod_pwm_init(unsigned int pwm_freq, unsigned int duty)
{
    g_pwm_chip_t = rtk_pwm_chip_get_by_name(IRRC_MODE_PWM_PIN_NAME);
    if(!g_pwm_chip_t)
	return -1;

    rtk_change_to_gpio_mode(g_pwm_chip_t,GPIO_PIN_LOW);
    /*set clock source */
    g_pwm_chip_t->rtk_freq_changed = true;
    g_pwm_chip_t->rtk_freq_100times = pwm_freq * 100;
    g_pwm_chip_t->rtk_freq = pwm_freq;
    g_pwm_chip_t->rtk_enable = 1;
    g_pwm_chip_t->rtk_duty = duty;


    rtk_pwm_clock_freq_set(g_pwm_chip_t);
    rtk_pwm_clock_source(g_pwm_chip_t);

    get_v_delay(g_pwm_chip_t);

    /*set timing ctrl , freq */
    rtk_pwm_freq_w(g_pwm_chip_t);

    /*set duty */
    if(g_pwm_chip_t->rtk_duty == 0 && PWM_MISC_TYPE(g_pwm_chip_t)){
        if(g_pwm_chip_t->rtk_polarity == 1)
            rtk_pwm_force_mode_w(g_pwm_chip_t,PWM_FORCE_HIGH);
        else
            rtk_pwm_force_mode_w(g_pwm_chip_t,PWM_FORCE_LOW);
    } else{
        rtk_pwm_force_mode_w(g_pwm_chip_t,PWM_NON_FORCE);
        rtk_pwm_duty_w(g_pwm_chip_t,g_pwm_chip_t->rtk_duty);
    }
    rtk_pwm_totalcnt_w(g_pwm_chip_t,g_pwm_chip_t->rtk_totalcnt);

    /*set CTRL */
    rtk_pwm_ctrl_polarity_w(g_pwm_chip_t,g_pwm_chip_t->rtk_polarity);
    rtk_pwm_output_w(g_pwm_chip_t,g_pwm_chip_t->rtk_enable);
    rtk_pwm_cycle_max_w(g_pwm_chip_t,0);
    rtk_pwm_db_wb(g_pwm_chip_t);

    rtk_pwm_db_enable(g_pwm_chip_t, 0);

     /*vsync delay*/
    rtk_pwm_vs_delay_w(g_pwm_chip_t,0);	
	 rtk_pwm_db_wb(g_pwm_chip_t);
    return 0;
}

int __init irrc_mod_module_init(void)

{
    int ret = 0;
    if (misc_register(&rtk_irrc_mod_miscdev)) {
        rtd_printk(KERN_ERR, TAG_NAME,  "[IRRC_MOD]register irrc mod misc dev file failed\n");
        ret = -ENODEV;
        goto FAIL_REGISTER_MISC_DEV;
    }
#ifdef CONFIG_KDRV_IRTX_USE_TIMER    
    if(rtk_ir_timer_init(irrc_mod_key_htimer_func) != 0) {
	rtd_printk(KERN_ERR, TAG_NAME,  "[IRRC_MOD]rtk_ir_timer_init fail\n");
	ret = -EBUSY;
	goto FAIL_INIT_IR_TIMER;
    }
#endif
    if(irrc_mod_pwm_init(g_cur_modulation_freq, g_cur_modulation_duty) != 0) {
	rtd_printk(KERN_ERR, TAG_NAME,  "[IRRC_MOD]fail to get pwm pin\n");
	ret = -ENODEV;
	goto FAIL_GET_PWM_PIN;
    }					

     irrc_mod_generate_signal(1);

    ret = sysfs_create_group(&rtk_irrc_mod_miscdev.this_device->kobj, &irrc_mod_attr_group);
    if (ret != 0) {
        ret = -ENOMEM;
        goto FAIL_ADD_ATTRS_FILE;
    }

#ifdef __IRRC_MOD_TEST__
    kthread_run(thread_function, NULL, "mythread%d", 1);
#endif
    
    return 0;
FAIL_ADD_ATTRS_FILE:
FAIL_GET_PWM_PIN:
#ifdef CONFIG_KDRV_IRTX_USE_TIMER        
    rtk_ir_timer_exit();
FAIL_INIT_IR_TIMER:
#endif    
    misc_deregister(&rtk_irrc_mod_miscdev);
FAIL_REGISTER_MISC_DEV:
    return ret;
}

void __exit irrc_mod_module_exit(void)
{
    sysfs_remove_group(&rtk_irrc_mod_miscdev.this_device->kobj, &irrc_mod_attr_group);
    misc_deregister(&rtk_irrc_mod_miscdev);
#ifdef CONFIG_KDRV_IRTX_USE_TIMER      
    rtk_ir_timer_exit();
#endif
}
late_initcall(irrc_mod_module_init);
module_exit(irrc_mod_module_exit);

