/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2016 by Jerry Lu <jerry.lu@realtek.com.tw>
 *
 * sysfs attributes in /sys/realtek_boards
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/blkdev.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/rtkrecord.h>
#include <mach/timex.h>
#include <linux/pageremap.h>
extern int rtk_demod_get_debugbuf(unsigned int *buf_addr, unsigned int *buf_size);

#include <linux/time.h> 
#include <linux/sched.h>
#include <linux/kthread.h>

#include <linux/delay.h>
static struct task_struct *demodlog_tsk;
unsigned int logperiod = 0;
extern int rtk_demod_get_debugbuf(unsigned int *buf_addr, unsigned int *buf_size);

static void get_current_localtime(char *timestamp)
{
        struct timeval tv;
        struct tm t;

        do_gettimeofday(&tv);
        printk("time of day : %d \n",tv.tv_sec);
        time_to_tm(tv.tv_sec, 0, &t);
        timestamp += strlen(timestamp);
        sprintf(timestamp,"%ld%d%d%d%d%d",
            t.tm_year + 1900,t.tm_mon + 1,t.tm_mday,(t.tm_hour + 8) % 24, t.tm_min,t.tm_sec);
}

static void createLogFile(const char *filename, unsigned int addr, unsigned int Size)
{
    mm_segment_t old_fs;
    struct file *demod_log_file = NULL; //rtd log file fd
    loff_t pos = 0;
    char		buffer[100];
    unsigned int *current_ptr = 0;
//    unsigned int *tempbuf=(unsigned int *)dvr_malloc(1024*1024);
    unsigned int tial = 0;
    unsigned char changeline = 0;
 //   if(addr==0)
 //       goto createfail_return;

    printk("before file open\n");
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    current_ptr = (unsigned int *)addr;
    tial = addr + Size;
    demod_log_file = filp_open(filename, O_CREAT | O_RDWR | O_TRUNC, 0600);
    printk("file open\n");

//	printf("writing log data...");
    if(!IS_ERR(demod_log_file))
    {
        for(current_ptr; current_ptr < tial;current_ptr++)
        {
            sprintf(buffer, "%x",*current_ptr);
            vfs_write(demod_log_file, buffer, strlen(buffer),&pos);
            changeline++;
            if(changeline == 16)
            {
                sprintf(buffer, "\n");
                vfs_write(demod_log_file, buffer, strlen(buffer),&pos);
                changeline = 0;
            }
        }
    }
    else
    {
        printk("[%s]line %d ERROR\n",__FUNCTION__,__LINE__);
    }

//	printf("done\n");
createfail_return:
      	set_fs(old_fs);
	if(!IS_ERR(demod_log_file))
	{
		filp_close(demod_log_file,NULL);
		printk("file closed\n");
	}

	demod_log_file=NULL;
	
}

int demod_get_time_period(void)
{
        return logperiod;
}

void demod_set_time_period(int period)
{
       logperiod = period;
}

static int demod_log(void *arg)
{
        unsigned int Addr;
        mm_segment_t old_fs;
        struct file *demod_log_file = NULL; //rtd log file fd
        loff_t pos = 0;
        unsigned int Size;
        unsigned char count =0;
        char buffer[100] = "/tmp/demod_";
        char date[30];
        char wirtebuffer[100];
        unsigned int *current_ptr = 0;
        unsigned int tial = 0;
        unsigned char changeline = 0; 
        int *period = (int*)arg;
        while(1)
        {
                msleep(*period);
                sprintf(buffer,"/tmp/demod_%d.bin",count);
                rtk_demod_get_debugbuf(&Addr, &Size);
                current_ptr = (unsigned int *)Addr;
                tial = Addr + Size; 
                old_fs = get_fs();
                set_fs(KERNEL_DS);
                Size = 2*1024*1024;
                if(Size)
                {   
                        demod_log_file = filp_open(buffer, O_CREAT | O_RDWR | O_TRUNC, 0600);
                        if(!IS_ERR(demod_log_file))
                        { 
                                get_current_localtime(date);
                                vfs_write(demod_log_file, date, strlen(date),&pos);
                                for(current_ptr; current_ptr < tial;current_ptr++)
                                {
                                        sprintf(wirtebuffer, "%x",*current_ptr);
                                        vfs_write(demod_log_file, wirtebuffer, strlen(wirtebuffer),&pos);
                                        changeline++;
                                        if(changeline == 16)
                                        {
                                                sprintf(wirtebuffer, "\n");
                                                vfs_write(demod_log_file, wirtebuffer, strlen(wirtebuffer),&pos);
                                                changeline = 0;
                                        }
                                }
                        }
                        else
                        {
                             set_fs(old_fs);
                             return 1;
                        }
                        if(!IS_ERR(demod_log_file))
	                 {
		               filp_close(demod_log_file,NULL);
	                 }

                }
                count++;
                if(count >= 10)
                        count = 0;
                pos = 0;
        }
}

static int demod_log_repeatly(int period)
{
        int ret;
        demodlog_tsk = kthread_create(demod_log, &logperiod, "demodlog");
        if (IS_ERR(demodlog_tsk)) {
                ret = PTR_ERR(demodlog_tsk);
                demodlog_tsk = NULL;
                return ret;
        }
        wake_up_process(demodlog_tsk);

        return 0;
}

void demod_log_init(void)
{

}

int  demod_log_start(void)
{
        unsigned int Addr;
        unsigned int Size;
        char buffer[100] = "/tmp/demod_";
        char *tmp_str;
        if(logperiod)
        {
            demod_log_repeatly(logperiod);
            return 0;
        }
        get_current_localtime(buffer);
        tmp_str = buffer;
        tmp_str += strlen(tmp_str);
        sprintf(tmp_str,".bin");
        rtk_demod_get_debugbuf(&Addr, &Size);
        timer_get_value(SYSTEM_TIMER); 
        if(Addr)
           createLogFile("/tmp/demod.bin",Addr,Size);

        return 0;
}
