#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/kernel.h>   /* MISC_DBG_PRINT() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>           /* everything... */
#include <linux/errno.h>        /* error codes */
#include <linux/types.h>        /* size_t */
#include <linux/fcntl.h>        /* O_ACCMODE */
#include <linux/cdev.h>
#include <linux/module.h>
//#include <asm/system.h>       /* cli(), *_flags */
#include <linux/uaccess.h>        /* copy_*_user */
#include <asm/io.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <rtk_kdriver/misc.h>
//#include <rbus/crt_reg.h>
#include <rbus/dc_sys_reg.h>
#include <rbus/stb_reg.h>
#include "rtd_types.h"
#include <linux/kthread.h>
#include <linux/interrupt.h>

#include <rbus/dc1_mc_reg.h>
#ifdef DCMT_DC2
#include <rbus/dc2_sys_reg.h>
#include <rbus/dc2_mc_reg.h>
#include <rbus/ib_reg.h>
#endif // #ifdef DCMT_DC2
#include <rbus/tvsb1_reg.h>
#include <rbus/tvsb2_reg.h>
#include <rbus/tvsb4_reg.h>
#include <rbus/tvsb5_reg.h>
#include <rbus/dc_sys_64bit_wrapper_reg.h>
//#include <rbus/crt_reg.h>

#include <rbus/vodma_reg.h>

#include <mach/rtk_log.h>
#include <mach/pcbMgr.h>
#include <linux/console.h>

#ifdef CONFIG_RTK_KDRV_WATCHDOG
#include <rtk_kdriver/rtk_watchdog.h>
#endif

#include <linux/of_irq.h>
#include <pinmux_reg.h>
#include <rtk_gpio.h>

#ifdef CONFIG_ENABLE_WOV_SUPPORT
#include <rtk_kdriver/rtk_wov.h>
#endif


#define MISC_TAG "MISC"

#define MISC_DBG(fmt,args...) rtd_printk(KERN_DEBUG, MISC_TAG, fmt, ## args)
#define MISC_INFO(fmt,args...) rtd_printk(KERN_INFO, MISC_TAG, fmt, ## args)
#define MISC_ERR(fmt,args...) rtd_printk(KERN_ERR, MISC_TAG, fmt, ## args)
#define MISC_EMERG(fmt,args...) rtd_printk(KERN_EMERG, MISC_TAG, fmt, ## args)
//#define MISC_DBG_PRINT(s, args...) printk(s, ## args)
//#define MISC_DBG_PRINT(s, args...)

int g_bIsBackupJTAGPinmux = 0;
unsigned int ST_GPIO_ST_CFG_5 = 0;
unsigned int ST_GPIO_ST_CFG_6 = 0;

int misc_major      = 0;
int misc_minor      = 0;
#define REG_BUSH_OC_CONTROL 0xb8000438
#define REG_BUSH_OC_DONE 0xb8000438
#define REG_BUSH_SSC_CONTROL 0xb80003d0
#define REG_BUSH_NCODE_SSC 0xb80003d4
#define REG_BUSH_FCODE_SSC 0xb80003d4
#define REG_BUSH_GRAN_EST 0xb80003d0

#define REG_BUS_OC_CONTROL 0xb8000428
#define REG_BUS_OC_DONE 0xb8000428
#define REG_BUS_SSC_CONTROL 0xb80003c0
#define REG_BUS_NCODE_SSC 0xb80003c4
#define REG_BUS_FCODE_SSC 0xb80003c4
#define REG_BUS_GRAN_EST 0xb80003c0

#define REG_DDR_OC_CONTROL 0xb8008028
#define REG_DDR_OC_DONE 0xb8008028
#define REG_DDR_SSC_CONTROL 0xb800801c
#define REG_DDR_NCODE_SSC 0xb8008028
#define REG_DDR_FCODE_SSC 0xb8008024
#define REG_DDR_GRAN_EST 0xb8008020

#define REG_DDR2_OC_CONTROL 0xb8004028
#define REG_DDR2_OC_DONE 0xb8004028
#define REG_DDR2_SSC_CONTROL 0xb800401c
#define REG_DDR2_NCODE_SSC 0xb8004028
#define REG_DDR2_FCODE_SSC 0xb8004024
#define REG_DDR2_GRAN_EST 0xb8004020

#define REG_DISPLAY_OC_CONTROL 0xb80006a0
#define REG_DISPLAY_OC_DONE 0xb80006b4
#define REG_DISPLAY_SSC_CONTROL 0xb80006b0
#define REG_DISPLAY_NCODE_SSC 0xb80006ac
#define REG_DISPLAY_FCODE_SSC 0xb80006ac
#define REG_DISPLAY_GRAN_EST 0xb80006b0

#define DC_PHY_EFF_MEAS_CTRL_reg 0xB8008D80
#define DC_PHY_TMCTRL3_reg 0xB800880C
#define DC_PHY_READ_CMD_reg 0xB8008D84
#define DC_PHY_WRITE_CMD_reg 0xB8008D88

#define JTAG_CONTROL_REG 0xB810E004
#define U2R_CONTROL_REG 0xB8061010

#define PERIOD 30//60 //period 30 kHz
#define DOT_GRAN 4
#define RTD_LOG_CHECKING_REGISTER 0xb801a610
#define RTD_LOG_CHECK_REG 0x100

#define IB_CLIENTS_NUM 13
#define IB_CLIENTS_REG_ADDRESS_OFFSET 0x100
#define IB_TRAP_CASE_NUMBER 11
#define IB_ERR_STATUS_NUMBER 32

#define MICOM_IRDA_CTRL_reg 0xb8060150

#define MICOM_WRITE_PANEL_ONOFF 0x3A

#if defined(CONFIG_COMPAT) && defined(CONFIG_ARM64)
#include <linux/compat.h>
#define to_user_ptr(x)          compat_ptr((UINT32) x)
#else
#define to_user_ptr(x)          ((void* __user)(x)) // convert 32 bit value to user pointer
#endif

typedef enum DC_ID {
        DC_ID_1 = 0,
#ifdef DCMT_DC2
        DC_ID_2 = 1,
#endif //#ifdef DCMT_DC2
        DC_NUMBER,
} DC_ID;

typedef enum DC_SYS_ID {
        DC_SYS1 = 0,
        DC_SYS2 = 1,
        DC_SYS3 = 2,
        DC_SYS_NUMBER,
} DC_SYS_ID;

static struct class *misc_class;
static struct misc_dev *misc_drv_dev;                           /* allocated in misc_init_module */

struct semaphore micom_sem;

extern void msleep(unsigned int msecs);
extern void dump_stacks (void);
static void rtk_ddr_debug_resume(void);

#if defined(CONFIG_REALTEK_INT_MICOM)
static int fWOLEnable = 0;
#endif
//extern unsigned int console_rtdlog_level;
//extern unsigned int console_rtdlog_module;
extern rtk_rtdlog_info_t console_rtdlog;
extern rtk_rtdlog_info_t * rtdlog_info ;

typedef struct DC_ERRCMD_REGS {
        unsigned int addcmd_hi[DC_NUMBER][DC_SYS_NUMBER];
        unsigned int addcmd_lo[DC_NUMBER][DC_SYS_NUMBER];
        unsigned int dc_int[DC_NUMBER][DC_SYS_NUMBER];
} DC_ERRCMD_REGS;

DC_ERRCMD_REGS dcsys_errcmd_regs;



#ifdef CONFIG_ENABLE_WOV_SUPPORT

unsigned long long g_temp_wovdb_file_size = 0;

bool updateSensoryModel(const char* pStrModelFilePath) {

    unsigned long long wovdb_start_block;
    unsigned long long wovdb_size_block; 
    unsigned int size,blk,i,count=0;
    char *tmp_ptr,*buf_ptr;
    unsigned char *buf_ptr2;
    int wovdb_index;
    const char *wovdb_name="wovdb";

    struct kstat *stat;
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err, ret = 0;
    unsigned long long offset=0;
    char path[128];


    //printk("[WOV] %s-%d, pStrModelFilePath:%s\n", __func__, __LINE__, pStrModelFilePath);

    wovdb_index = lgemmc_get_partnum(wovdb_name);

    //printk("[WOV] %s-%d, wovdb_name:%s, wovdb_index:%d, wovdb_par_size:%d\n", __func__, __LINE__, wovdb_name, wovdb_index, lgemmc_get_partsize(wovdb_index));

    wovdb_size_block = lgemmc_get_partsize(wovdb_index)/EMMC_BLOCK_SIZE;
    if((wovdb_size_block % 8) != 0) {
        wovdb_size_block += 8-(wovdb_size_block % 8);
    }
    wovdb_start_block =  lgemmc_get_partoffset(wovdb_index)>>9;  
    //printk("[WOV] %s-%d, wovdb_size_block:%lld, wovdb_start_block:%llx\n", __func__, __LINE__, wovdb_size_block, wovdb_start_block);

   //map to virtual address
    buf_ptr=dvr_remap_uncached_memory(WOV_DB_DDR_PHYSICAL_START_ADDR, wovdb_size_block*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    //buf_ptr=dvr_remap_uncached_memory(WOV_DB_DDR_PHYSICAL_START_ADDR, 1*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    //printk("[WOV] %s:load wov db from EMMC(%llx @ %llx) to DDR(%p)\n", __FUNCTION__, wovdb_size_block, wovdb_start_block, buf_ptr);

    //temp buffer size use 1MB in case kmalloc fail
    size=64*1024;
    blk=size/EMMC_BLOCK_SIZE;
    tmp_ptr = kmalloc(size,GFP_DCU1);
    if(tmp_ptr==NULL){
        printk("[WOV] kmalloc fail %d Byte\n",size);
        return false;
    }

    //determine how many times to read from emmc
    count=wovdb_size_block/blk;
    if(wovdb_size_block%blk!=0)
        count++;
    //printk("[WOV] count=%d\n",count);

    sprintf(path,"%s",pStrModelFilePath);

    //open file
    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, O_RDONLY, 0);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        set_fs(oldfs);  
        printk("[WOV] wov:open fail\n");
        kfree(tmp_ptr);        
        return false;
    }

    //printk("[WOV] wovdb filePath:%s\n", path);

    //clear emmc
    for(i=0;i<count;i++){
        memset((void *)tmp_ptr,0,size);
        if (mmc_fast_write(wovdb_start_block+i*blk, blk, (unsigned char *)tmp_ptr) != blk){
            printk("[WOV] clear emmc fail %d\n",i);
            ret = false;
            goto gofail;
            break;
        }
    }

    //read file write to emmc
    i = 0;
    memset((void *)tmp_ptr,0,size);
    //printk("[WOV] v2, read size:%d\n", vfs_read(filp, tmp_ptr, size, &offset));    
    while (vfs_read(filp, tmp_ptr, size, &offset) > 0){      
        //printk("[WOV] file context=%x,%x,%x,%x\n",tmp_ptr[0],tmp_ptr[1],tmp_ptr[2],tmp_ptr[3]);
        if (mmc_fast_write(wovdb_start_block+i*blk, blk, (unsigned char *)tmp_ptr) != blk){
            printk("[WOV] write emmc fail %d\n",i);
            ret = false;          
            goto gofail;
            break;
        }
        memset((void *)tmp_ptr,0,size);
        i++;
    }

	//setup wov ddr dump register, map to virtual address
    buf_ptr2=(unsigned char*)dvr_remap_uncached_memory(0x47c00000, 8*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    *(unsigned long*)(buf_ptr2+0x1C) = WOV_DB_DDR_PHYSICAL_START_ADDR; //wovdb start addr (WOV_POLLING_START_LEN + 4)
    *(unsigned long*)(buf_ptr2+0x20) = lgemmc_get_partsize(wovdb_index); //wovdb size (WOV_MODEL_DB_ADDR_reg+0x4)
    //*(unsigned long*)(buf_ptr2+0x20) = lgemmc_get_filesize(wovdb_index); //wovdb size (WOV_MODEL_DB_ADDR_reg+0x4)
    printk("[WOV] *(buf_ptr2+0x1C):%lx, *(buf_ptr2+0x20):%lx\n", *(unsigned long*)(buf_ptr2+0x1C), *(unsigned long*)(buf_ptr2+0x20));
    dvr_unmap_memory((void *)buf_ptr2,8*EMMC_BLOCK_SIZE);

    buf_ptr2=(unsigned char*)dvr_remap_uncached_memory(0x47c00000, 8*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    printk("[WOV] *(buf_ptr2+0x1C):%lx, *(buf_ptr2+0x20):%lx\n", *(unsigned long*)(buf_ptr2+0x1C), *(unsigned long*)(buf_ptr2+0x20));
    dvr_unmap_memory((void *)buf_ptr2,8*EMMC_BLOCK_SIZE);

    printk("[WOV] %s-%d, lgemmc_get_partname:%s, lgemmc_get_partsize:%d, lgemmc_get_filesize:%d, lgemmc_get_partoffset:%llx\n", __func__, __LINE__, lgemmc_get_partname(wovdb_index), lgemmc_get_partsize(wovdb_index), lgemmc_get_filesize(wovdb_index), lgemmc_get_partoffset(wovdb_index));

        
    stat =(struct kstat *) kmalloc(sizeof(struct kstat),GFP_KERNEL);
    vfs_stat(path,stat);
    g_temp_wovdb_file_size = stat->size;
    printk("[WOV] filesize:%lld\n",stat->size);
    kfree(stat);

    ret = true; 

gofail:

    //close file
    filp_close(filp, NULL);     
    set_fs(oldfs);

    dvr_unmap_memory((void *)buf_ptr,wovdb_size_block*EMMC_BLOCK_SIZE);       
    kfree(tmp_ptr);
    
    return ret;
}

void dumpSensoryModel(){
    int wovdb_index;
    const char *wovdb_name="wovdb";
    unsigned char *tmp_ptr;    
    unsigned long long wovdb_start_block;
    unsigned long long wovdb_size_block, wovdb_file_size_block;    
    unsigned int wovdb_file_size,i,j=0;
    unsigned int size,blk,count=0;
    unsigned int dump_size, temp_block_size = 0;

    wovdb_index = lgemmc_get_partnum(wovdb_name);
    wovdb_file_size = (g_temp_wovdb_file_size != 0) ? g_temp_wovdb_file_size : lgemmc_get_filesize(wovdb_index);
    wovdb_start_block =  lgemmc_get_partoffset(wovdb_index)>>9;
    wovdb_size_block = lgemmc_get_partsize(wovdb_index)/EMMC_BLOCK_SIZE;
    if((wovdb_size_block % 8) != 0) {
        wovdb_size_block += 8-(wovdb_size_block % 8);
    }

    //temp buffer size use 64kb in case kmalloc fail
    size=64*1024; //65536 bytes
    blk=size/EMMC_BLOCK_SIZE; //128 block
    tmp_ptr = kmalloc(size,GFP_DCU1);
    if(tmp_ptr==NULL){
        printk("[WOV] kmalloc fail %d Byte\n",size);
        return;
    }

    //determine how many times to read from emmc
  /*  
    count=wovdb_size_block/blk;
    if(wovdb_size_block%blk!=0)
        count++;
    printk("[WOV] count=%d, blk:%d\n",count, blk);
  */

    wovdb_file_size_block = wovdb_file_size/EMMC_BLOCK_SIZE;
    count=wovdb_file_size_block/blk;
    if(wovdb_file_size_block%blk!=0)
        count++;
    printk("[WOV] count=%d, blk:%d\n",count, blk);
    
    //read from emmc
    printk("[WOV] dump wovdb data, filesize:%d\n", wovdb_file_size);
    printk("========================\n");
    for(i=0;i<count;i++){
        temp_block_size = (i+1)*blk*EMMC_BLOCK_SIZE;
        memset((void *)tmp_ptr,0,size);

		if (mmc_fast_read(wovdb_start_block+i*blk, blk, (unsigned char *)tmp_ptr) != blk){
			printk("[WOV] load fail %d\n",i);
			break;
		}

        dump_size = (wovdb_file_size / temp_block_size != 0) ? (blk*EMMC_BLOCK_SIZE) : (blk*EMMC_BLOCK_SIZE) - (temp_block_size - wovdb_file_size); 
        
        printk("\ni:%d\n",i);                
        for(j=0;j<dump_size;j++){
            printk(" %x ", *(tmp_ptr+j));  
        }

        if(temp_block_size > wovdb_file_size) {
            printk("\n[WOV] read wovdb file from emmc done, i:%d, wovdb_fileSize:%d\n",i, wovdb_file_size);             
            break;   
        }

    }
    printk("\n========================\n");    
    kfree(tmp_ptr);  

}


#endif


static void  rtd_part_outl(unsigned int reg_addr, unsigned int endBit, unsigned int startBit, unsigned int value)
{
        unsigned int X, A, result;
        X = (1 << (endBit - startBit + 1)) - 1;
        A = rtd_inl(reg_addr);
        result = (A & (~(X << startBit))) | (value << startBit);
        MISC_INFO("[%s]origin=0x%x,startBit=%d, endBit=%d, value=0x%x\n", __FUNCTION__, A, startBit, endBit, value);
        MISC_INFO("[%s]reg=0x%x, value=0x%x\n", __FUNCTION__, reg_addr, result);
        rtd_outl(reg_addr, result);

}

#define MODULE_STR_SIZE    (8)
static bool StrToHex(unsigned char* pu8string, unsigned int u32Size, unsigned int* pu32Value)
{
        unsigned int u32Value = 0;
        unsigned int i;
        unsigned char u8Data[MODULE_STR_SIZE];
        bool bRet = true;

        if (u32Size > MODULE_STR_SIZE) {
                u32Size = MODULE_STR_SIZE;
        }
        for (i = 0; i < u32Size; i++) {
                //Covert 'A' to 'a'
                if ((pu8string[i] >= 'A') && (pu8string[i] <= 'F')) {
                        u8Data[i] = pu8string[i] - 'A' + 'a';
                } else {
                        u8Data[i] = pu8string[i];
                }
        }

        for (i = 0; i < u32Size; i++) {
                //printf("u8Data[%d]: %d.\n", i, u8Data[i]);
                if ((u8Data[i] >= '0') && (u8Data[i] <= '9')) {
                        u32Value *= 16;
                        u32Value += (u8Data[i] - '0');
                } else if ((u8Data[i] >= 'a') && (u8Data[i] <= 'f')) {
                        u32Value *= 16;
                        u32Value += (u8Data[i] + 10 - 'a');
                } else if (u8Data[i] != '\n') {
                        bRet = false;
                }
        }
        if (bRet) {
                *pu32Value = u32Value;
        }
        // MISC_INFO("[MISC][StrToHex]%d,leng=%s,%s, ===> %d\n", bRet,u32Size ,u8Data, *pu32Value  );

        return bRet;
}


static unsigned int getSSCParameter(unsigned int inputFreq, unsigned int inRatio,
                                    unsigned int *Ncode_ssc,  unsigned int *Fcode_ssc, unsigned int *Gran_est)
{
        unsigned int M_target = 0;
        unsigned int target_F_Code = 0;
        unsigned int ssc_clock = 0;
        unsigned int step = 0;
        unsigned int ret_Ncode_ssc = 0;
        unsigned int ret_Fcode_ssc = 0;
        unsigned int ret_Gran_est = 0;
        unsigned int tempValue = 0;
        unsigned int tempValue3 = 0;

        unsigned int tempValue2 = 0;
        unsigned int temp_a = 0;
        unsigned int temp_b = 0;

        M_target = (inputFreq / 27) - 3;

        tempValue = ((100000 * inputFreq) / 27) % 100000;
        target_F_Code = (tempValue * 2048) / 100000;

        ret_Ncode_ssc = ((inputFreq * (10000 - inRatio)) / 27) / 10000 - 3;

        tempValue = ((inputFreq * (10000 - inRatio)) / 270) % 1000;
        ret_Fcode_ssc = (tempValue * 2048) / 1000;

        tempValue2 = (inputFreq * (10000 - inRatio / 2));
        tempValue = (((M_target - ret_Ncode_ssc) * 2048 + (target_F_Code - ret_Fcode_ssc)) / 2 / 2048) + (ret_Ncode_ssc + 3);
        temp_a = inputFreq * 10000;
        temp_b = inputFreq * (inRatio / 2);
        tempValue3 = (temp_a - temp_b);
        ssc_clock = tempValue3 / tempValue / 10;
        step = ssc_clock / PERIOD;

        tempValue2 = ((M_target - ret_Ncode_ssc) * 2048 + (target_F_Code - ret_Fcode_ssc));
        tempValue = (1000 * (tempValue2 * 2)) / step;
        tempValue2 = 1 << (15 - DOT_GRAN);
        ret_Gran_est = (tempValue * tempValue2) / 1000; //pow(2,15-DOT_GRAN);

        if(Ncode_ssc != NULL)
                *Ncode_ssc = ret_Ncode_ssc;
        if(Fcode_ssc != NULL)
                *Fcode_ssc = ret_Fcode_ssc;
        if(Gran_est != NULL)
                *Gran_est = ret_Gran_est;

        MISC_INFO("[%s] M_target=%d, target_F_Code=%d\n", __FUNCTION__, M_target, target_F_Code);
        MISC_INFO("[%s] ssc_clock=%d, step=%d\n", __FUNCTION__, ssc_clock, step);
        MISC_INFO("[%s] input(%d,%d),output(%d,,%d,,%d,)\n", __FUNCTION__, inputFreq, inRatio, ret_Ncode_ssc, ret_Fcode_ssc, ret_Gran_est);

        return 1;

}

static unsigned int getDisplaySSCParameter(unsigned int inputFreq, unsigned int inRatio,
                unsigned int *Ncode_ssc,  unsigned int *Fcode_ssc, unsigned int *Gran_est)
{
        unsigned int N_code = 0;
        unsigned int Dividor = 0;
        unsigned int M_target = 0;
        unsigned int target_F_Code = 0;
        unsigned int ssc_clock = 0;
        unsigned int step = 0;
        unsigned int ret_Ncode_ssc = 0;
        unsigned int ret_Fcode_ssc = 0;
        unsigned int ret_Gran_est = 0;
        unsigned int tempValue = 0;
        unsigned int tempValue2 = 0;

        M_target = (((1000 * inputFreq / 27) * (N_code + 2) - 3000) / 1000) * (1 << Dividor);

        tempValue = (((100000 * inputFreq) * (N_code + 2)) / 27) % 100000;
        target_F_Code = ((tempValue * 2048) / 100000) * (1 << Dividor);

        ret_Ncode_ssc = (((inputFreq * (10000 - inRatio)) / 27) * (N_code + 2) / 10000 - 3) * (1 << Dividor);

        tempValue = ((inputFreq * (10000 - inRatio)) / 270) * (N_code + 2) % 1000;
        ret_Fcode_ssc = ((tempValue * 2048) / 1000) * (1 << Dividor);

        tempValue2 = (inputFreq * (10000 - inRatio / 2));
        tempValue = (1000 * ((M_target - ret_Ncode_ssc) * 2048 + (target_F_Code - ret_Fcode_ssc)) / 2 / 2048) + 1000 * (ret_Ncode_ssc + 3);
        ssc_clock = (100 * tempValue2) / tempValue;

        step = ssc_clock / PERIOD;

        tempValue2 = ((M_target - ret_Ncode_ssc) * 2048 + (target_F_Code - ret_Fcode_ssc));
        tempValue = (1000 * (tempValue2 * 2)) / step;
        tempValue2 = 1 << (15 - DOT_GRAN);
        ret_Gran_est = (tempValue2 * tempValue) / 1000; //pow(2,15-DOT_GRAN);

        if(Ncode_ssc != NULL)
                *Ncode_ssc = ret_Ncode_ssc;
        if(Fcode_ssc != NULL)
                *Fcode_ssc = ret_Fcode_ssc;
        if(Gran_est != NULL)
                *Gran_est = ret_Gran_est;

        MISC_INFO("[%s] M_target=%d, target_F_Code=%d\n", __FUNCTION__, M_target, target_F_Code);
        MISC_INFO("[%s] ssc_clock=%d, step=%d\n", __FUNCTION__, ssc_clock, step);
        MISC_INFO("[%s] input(%d,%d),output(%d,,%d,,%d,)\n", __FUNCTION__, inputFreq, inRatio, ret_Ncode_ssc, ret_Fcode_ssc, ret_Gran_est);

        return 0;
}

static unsigned int getDisplayFrequency(void)//reference from bootcode:/common/cmd_dssinfo.c  pll_ddr_freq()
{
        unsigned int regValue = 0;
        unsigned int PLL_M, PLL_N, DPLL_O, F_CODE, FOUT;
        unsigned int O_Divider = 1;
        unsigned int tempValue;
        regValue = rtd_inl(0xb80006c0);

        PLL_M = (regValue & 0x0FF00000) >> 20;
        PLL_N = (regValue & 0x00007000) >> 12;

        regValue = rtd_inl(0xb80006a0);
        F_CODE = (regValue & 0x07FF0000) >> 16;

        regValue = rtd_inl(0xb80006C8);
        DPLL_O = (regValue & 0x00003000) >> 12;
        switch (DPLL_O) {
                case 0x3:
                        O_Divider = 8;
                        break;
                case 0x1:
                        O_Divider = 2;
                        break;
                case 0x2:
                        O_Divider = 4;
                        break;
                case 0x00:
                default:
                        O_Divider = 1;
                        break;
        }

        tempValue = (PLL_N + 2) * O_Divider;

        FOUT = (unsigned int)((27 * PLL_M + 27 * 3) + (27 * F_CODE / 2048)) / tempValue;

        MISC_INFO("[%s] M:%d, N:%d, F=%d ,DPLL_O=0x%x, O=%d, out=%d\n", __FUNCTION__, PLL_M, PLL_N, F_CODE, DPLL_O, O_Divider, FOUT);
        return FOUT;
}

static unsigned int getCPUFrequency(void)//reference from bootcode:/common/cmd_dssinfo.c  pll_ddr_freq()
{
        unsigned int regValue = rtd_inl(0xb8000424);
        unsigned int N_CODE, F_CODE, FOUT;
        N_CODE = (regValue & 0x0000FF00) >> 8;
        F_CODE = (regValue & 0x07FF0000) >> 16;

        FOUT = (unsigned int)(((27 * N_CODE + 27 * 3 + (27 * F_CODE) / 2048)));

        MISC_INFO("[%s] regvalue= 0x%x ==>N:%d, F=%d , out=%d\n", __FUNCTION__, regValue, N_CODE, F_CODE, FOUT);
        return FOUT;
}

static unsigned int getDDR1Frequency(void)//reference from bootcode:/common/cmd_dssinfo.c  pll_ddr_freq()
{
        unsigned int regValueForN = rtd_inl(REG_DDR_NCODE_SSC);
        unsigned int regValueForF = rtd_inl(REG_DDR_FCODE_SSC);
        unsigned int N_CODE, F_CODE, FOUT;
        N_CODE = (regValueForN & 0xFF00) >> 8;
        F_CODE = (regValueForF & 0x7FF0000) >> 16;

        FOUT = (unsigned int)(((27 * N_CODE + 27 * 3 + (27 * F_CODE) / 2048)));

        MISC_INFO("[%s] regvalueForN= 0x%x regvalueForF= 0x%x ==>N:%d, F=%d , out=%d\n", __FUNCTION__, regValueForN, regValueForF, N_CODE, F_CODE, FOUT);
        return FOUT;
}
static unsigned int getDDR2Frequency(void)//reference from bootcode:/common/cmd_dssinfo.c  pll_ddr_freq()
{
        unsigned int regValueForN = rtd_inl(REG_DDR2_NCODE_SSC);
        unsigned int regValueForF = rtd_inl(REG_DDR2_FCODE_SSC);
        unsigned int N_CODE, F_CODE, FOUT;
        N_CODE = (regValueForN & 0xFF00) >> 8;
        F_CODE = (regValueForF & 0x7FF0000) >> 16;

        FOUT = (unsigned int)(((27 * N_CODE + 27 * 3 + (27 * F_CODE) / 2048)));

        MISC_INFO("[%s] regvalueForN= 0x%x regvalueForF= 0x%x ==>N:%d, F=%d , out=%d\n", __FUNCTION__, regValueForN, regValueForF, N_CODE, F_CODE, FOUT);
        return FOUT;
}
int RHAL_SYS_SetCPUSpreadSpectrum(unsigned int nRatio)
{
        //unsigned int reg;
        unsigned int NCode_ssc = 0;
        unsigned int FCode_ssc = 0;
        unsigned int Gran_est = 0;
        unsigned int freq_MHz = 0;
        unsigned int waitBit = 0;
        //Step 1. Get frequnecy
        freq_MHz = getCPUFrequency();

        //Step 2. Get Ncode_ssc, Fcode_ssc and Gran
        getSSCParameter(freq_MHz, nRatio, &NCode_ssc, &FCode_ssc, &Gran_est);

        //For BUS
        //Step 3. Write to register
        //Step 3.1 Disable ssc
        rtd_part_outl(REG_BUS_SSC_CONTROL, 0, 0, 0x0);//En_ssc =0
        //Step 3.2 Disable oc
        rtd_part_outl(REG_BUS_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.3 Set Ncode_ssc, Fcode_ssc
        rtd_part_outl(REG_BUS_NCODE_SSC, 23, 16, NCode_ssc);//Ncode_ssc
        rtd_part_outl(REG_BUS_FCODE_SSC, 10, 0, FCode_ssc);//Fcode_ssc
        //Step 3.4 Set Gran_est
        rtd_part_outl(REG_BUS_GRAN_EST, 22, 4, Gran_est);//Gran_set
        //Step 3.5 Enable oc
        rtd_part_outl(REG_BUS_OC_CONTROL, 0, 0, 1);//oc_en =1

        waitBit = rtd_inl(REG_BUS_OC_DONE);
        while((waitBit & 0x2) == 0) {
                waitBit = rtd_inl(REG_BUS_OC_DONE); // polling for oc_done
                MISC_INFO("waiting.....BUS=0x%x\n", waitBit);
        }
        //Step 3.6 Disable oc
        rtd_part_outl(REG_BUS_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.7 Enable ssc
        rtd_part_outl(REG_BUS_SSC_CONTROL, 0, 0, 0x1);//En_ssc =1

        //For BUSH
        //Step 3. Write to register
        //Step 3.1 Disable ssc
        rtd_part_outl(REG_BUSH_SSC_CONTROL, 0, 0, 0x0);//En_ssc =0
        //Step 3.2 Disable oc
        rtd_part_outl(REG_BUSH_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.3 Set Ncode_ssc, Fcode_ssc
        rtd_part_outl(REG_BUSH_NCODE_SSC, 23, 16, NCode_ssc);//Ncode_ssc
        rtd_part_outl(REG_BUSH_FCODE_SSC, 10, 0, FCode_ssc);//Fcode_ssc
        //Step 3.4 Set Gran_est
        rtd_part_outl(REG_BUSH_GRAN_EST, 22, 4, Gran_est);//Gran_set
        //Step 3.5 Enable oc
        rtd_part_outl(REG_BUSH_OC_CONTROL, 0, 0, 1);//oc_en =1

        waitBit = rtd_inl(REG_BUSH_OC_DONE);
        while((waitBit & 0x2) == 0) {
                waitBit = rtd_inl(REG_BUSH_OC_DONE); // polling for oc_done
                MISC_INFO("waiting.....BUSH=0x%x\n", waitBit);
        }

        //Step 3.6 Disable oc
        rtd_part_outl(REG_BUSH_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.7 Enable ssc
        rtd_part_outl(REG_BUS_SSC_CONTROL, 0, 0, 0x1);//En_ssc =1

        return 0;
}

//-----------------------------------------------------
//
//
//-----------------------------------------------------
int RHAL_SYS_SetDDRSpreadSpectrum(unsigned int nRatio)
{
        unsigned int NCode_ssc = 0;
        unsigned int FCode_ssc = 0;
        unsigned int Gran_est = 0;
        unsigned int freq_MHz = 0;
        unsigned int waitBit = 0;
        /******************************DCU1********************************/
        //Step 1. Get frequnecy
        freq_MHz = getDDR1Frequency();

        //Step 2. Get Ncode_ssc, Fcode_ssc and Gran
        getSSCParameter(freq_MHz, nRatio, &NCode_ssc, &FCode_ssc, &Gran_est);
        //Step 3. Write to register
        //Step 3.1 Disable ssc
        rtd_part_outl(REG_DDR_SSC_CONTROL, 3, 3, 0x1);//En_ssc =0
        //Step 3.2 Disable oc
        rtd_part_outl(REG_DDR_OC_CONTROL, 26, 26, 0);//oc_en =0
        //Step 3.3 Set Ncode_ssc, Fcode_ssc
        rtd_part_outl(REG_DDR_NCODE_SSC, 7, 0, NCode_ssc);//Ncode_ssc
        rtd_part_outl(REG_DDR_FCODE_SSC, 10, 0, FCode_ssc);//Fcode_ssc
        //Step 3.4 Set Gran_est
        rtd_part_outl(REG_DDR_GRAN_EST, 22, 20, DOT_GRAN);//Gran_set
        rtd_part_outl(REG_DDR_GRAN_EST, 18, 0, Gran_est);//Gran_set
        //Step 3.5 Enable oc
        rtd_part_outl(REG_DDR_OC_CONTROL, 26, 26, 1);//oc_en =1

        waitBit = rtd_inl(REG_DDR_OC_DONE);
        while((waitBit & 0x4000000) == 0) {
                waitBit = rtd_inl(REG_DDR_OC_DONE); // polling for oc_done
                printk("waiting.....DDR  =0x%x\n", waitBit);
        }
        //Step 3.6 Disable oc
        rtd_part_outl(REG_DDR_OC_CONTROL, 26, 26, 0);//oc_en =0
        //Step 3.7 Enable ssc
        rtd_part_outl(REG_DDR_SSC_CONTROL, 1, 1, 0x1);//En_ssc =1
        //Step 4.End

        /******************************DCU2********************************/
        NCode_ssc = 0;
        FCode_ssc = 0;
        Gran_est = 0;
        freq_MHz = 0;

        freq_MHz = getDDR2Frequency();
        //Step 2. Get Ncode_ssc, Fcode_ssc and Gran
        getSSCParameter(freq_MHz, nRatio, &NCode_ssc, &FCode_ssc, &Gran_est);
        //Step 3. Write to register
        //Step 3.1 Disable ssc
        rtd_part_outl(REG_DDR2_SSC_CONTROL, 3, 3, 0x1);//En_ssc =0
        //Step 3.2 Disable oc
        rtd_part_outl(REG_DDR2_OC_CONTROL, 26, 26, 0);//oc_en =0
        //Step 3.3 Set Ncode_ssc, Fcode_ssc
        rtd_part_outl(REG_DDR2_NCODE_SSC, 7, 0, NCode_ssc);//Ncode_ssc
        rtd_part_outl(REG_DDR2_FCODE_SSC, 10, 0, FCode_ssc);//Fcode_ssc
        //Step 3.4 Set Gran_est
        rtd_part_outl(REG_DDR2_GRAN_EST, 22, 20, DOT_GRAN);//Gran_set
        rtd_part_outl(REG_DDR2_GRAN_EST, 18, 0, Gran_est);//Gran_set
        //Step 3.5 Enable oc
        rtd_part_outl(REG_DDR2_OC_CONTROL, 26, 26, 1);//oc_en =1

        waitBit = 0;
        waitBit = rtd_inl(REG_DDR2_OC_DONE);
        while((waitBit & 0x4000000) == 0) {
                waitBit = rtd_inl(REG_DDR2_OC_DONE); // polling for oc_done
                printk("waiting.....DDR2  =0x%x\n", waitBit);
        }
        //Step 3.6 Disable oc
        rtd_part_outl(REG_DDR2_OC_CONTROL, 26, 26, 0);//oc_en =0
        //Step 3.7 Enable ssc
        rtd_part_outl(REG_DDR2_SSC_CONTROL, 1, 1, 0x1);//En_ssc =1
        //Step 4.End

        return 0;

}

//-----------------------------------------------------
//
//
//-----------------------------------------------------
int RHAL_SYS_SetDISPSpreadSpectrum(unsigned int nRatio)
{
        //unsigned int reg;
        unsigned int NCode_ssc = 0;
        unsigned int FCode_ssc = 0;
        unsigned int Gran_est = 0;
        unsigned int freq_MHz = 0;
        unsigned int waitBit = 0;
        //Step 1. Get frequnecy
        freq_MHz = getDisplayFrequency();

        //Step 2. Get Ncode_ssc, Fcode_ssc and Gran
        getDisplaySSCParameter(freq_MHz, nRatio, &NCode_ssc, &FCode_ssc, &Gran_est);

        //Step 3. Write to register
        //Step 3.1 Disable ssc
        rtd_part_outl(REG_DISPLAY_SSC_CONTROL, 0, 0, 0x0);//En_ssc =0
        //Step 3.2 Disable oc
        rtd_part_outl(REG_DISPLAY_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.3 Set Ncode_ssc, Fcode_ssc
        rtd_part_outl(REG_DISPLAY_NCODE_SSC, 23, 16, NCode_ssc);//Ncode_ssc
        rtd_part_outl(REG_DISPLAY_FCODE_SSC, 10, 0, FCode_ssc);//Fcode_ssc
        //Step 3.4 Set Gran_est
        rtd_part_outl(REG_DISPLAY_GRAN_EST, 22, 4, Gran_est);//Gran_set
        //Step 3.5 Enable oc
        rtd_part_outl(REG_DISPLAY_OC_CONTROL, 0, 0, 1);//oc_en =1

        waitBit = rtd_inl(REG_DISPLAY_OC_DONE);
        while((waitBit & 0x10) == 0) {
                waitBit = rtd_inl(REG_DISPLAY_OC_DONE); // polling for oc_done
                MISC_INFO("waiting.....Didplay =0x%x\n", waitBit);
        }
        //Step 3.6 Disable oc
        rtd_part_outl(REG_DISPLAY_OC_CONTROL, 0, 0, 0);//oc_en =0
        //Step 3.7 Enable ssc
        rtd_part_outl(REG_DISPLAY_SSC_CONTROL, 0, 0, 0x1);//En_ssc =1

        //Step 4.End
        return 0;
}

/*------------------------------------------------------------------
 * Func : Enable_PC_go
 *
 * Desc : Set pc_go bit to start counting. Reset this to stop counting
 *
 * Parm : pc_go, Counter start counting or not
 *
 * Retn : 0 - success, others fail
 *------------------------------------------------------------------*/
int set_PC_go_status(int pc_go)
{
        if(pc_go) {
                rtd_outl(DC_PHY_EFF_MEAS_CTRL_reg, 0x003FFF01);
                rtd_outl(0xB8004D80, 0x003FFF01);
        } else {
                rtd_outl(DC_PHY_EFF_MEAS_CTRL_reg, 0x003FFF00);
                rtd_outl(0xB8004D80, 0x003FFF00);
        }

        return 0;
}

int get_bw_values_info(unsigned int* pTotalBytesA , unsigned int* pPercentA ,
                       unsigned int* pTotalBytesB , unsigned int* pPercentB)
{
        unsigned int rd_cnt, wr_cnt, refcycle, meas_period;

        if((pTotalBytesA == NULL) || (pPercentA == NULL))
                return -1;

        if((pTotalBytesB == NULL) || (pPercentB == NULL))
                return -1;

        //comput DC1
        refcycle = (rtd_inl(DC_PHY_TMCTRL3_reg) >> 12) & 0xFFF;
        meas_period = (rtd_inl(DC_PHY_EFF_MEAS_CTRL_reg) >> 8) & 0x3FFF;
        rd_cnt = rtd_inl(DC_PHY_READ_CMD_reg) & 0x1FFFFFFF;
        wr_cnt = rtd_inl(DC_PHY_WRITE_CMD_reg) & 0x1FFFFFFF;

        //(rd_cnt+wr_cnt)*32/8/4;
        *pTotalBytesA =  (rd_cnt + wr_cnt);
        //*pPercentA = (rd_cnt+wr_cnt)*4*100;
        //*pPercentA /= (16*(refcycle+1)*meas_period);
        *pPercentA = (1 * (refcycle + 1) * meas_period) / 250;
        *pPercentA = *pTotalBytesA / *pPercentA;


        //comput DC2
        refcycle = (rtd_inl(0xB800480C) >> 12) & 0xFFF;
        meas_period = (rtd_inl(0xB8004D80) >> 8) & 0x3FFF;
        rd_cnt = rtd_inl(0xB8004D84) & 0x1FFFFFFF;
        wr_cnt = rtd_inl(0xB8004D88) & 0x1FFFFFFF;

        //(rd_cnt+wr_cnt)*32/8/4;
        *pTotalBytesB =  (rd_cnt + wr_cnt);
        //*pPercentB = (rd_cnt+wr_cnt)*4*100;
        //*pPercentB /= (16*(refcycle+1)*meas_period);
        *pPercentB = (1 * (refcycle + 1) * meas_period) / 250;
        *pPercentB = *pTotalBytesB / *pPercentB;


        return 0;
}



/*
 * Open and close
 */

int misc_open(
        struct inode*           inode,
        struct file*            filp
)
{
        struct misc_dev *pdev; /* device information */

        pdev = container_of(inode->i_cdev, struct misc_dev, cdev);
        filp->private_data = pdev; /* for other methods */

        MISC_DBG("misc open\n");
        return 0;
}

int misc_release(
        struct inode*           inode,
        struct file*            filp
)
{
        filp->private_data = NULL;

        MISC_DBG("misc release\n");
        return 0;
}

#ifdef CONFIG_RTK_KDRV_R8168
#if !defined(CONFIG_REALTEK_INT_MICOM) //defined but not used
static struct task_struct *thread_wol, *thread_wolether;
static int wol_polling(void *arg);
static int wolether_polling(void *arg);
#endif
#endif

extern struct platform_device *network_devs;
extern int rtl8168_suspend(struct platform_device *dev, pm_message_t state);
extern int rtl8168_resume(struct platform_device *dev);

#if !defined(CONFIG_REALTEK_INT_MICOM)
static int wol_polling(void *arg)
{
        pm_message_t pmstate;

        rtl8168_suspend(network_devs, pmstate);
        MISC_INFO("ether driver suspend for WOL\n");
        // Reset WOL Pin to high for the falling edge trigger of the ext. micom
        rtd_outl(0xb8061108, (rtd_inl(0xb8061108) | (1 << 6)));
        // Clear WOL Flag
        rtd_outl(0xb80160b0, 0xf0031e00);  //0xf0031e00
        rtd_outl(0xb806050c, 0xbeef7777);
        while(!kthread_should_stop()) {
                msleep(300);
                rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00
                MISC_INFO("%08x\n", rtd_inl(0xb80160b0));   // check 0x1e00(default)  -->WOL 0x9e00
                MISC_INFO("%08x\n", rtd_inl(0xb8061108));
                if ( (rtd_inl(0xb80160b0) & 0xffff) == 0x9e00)
                        break;
                if ( rtd_inl(0xb806050c) == 0xbeef9999)
                        break;
        }
        MISC_INFO("WOL!! wol_polling thread break\n");
        if ( rtd_inl(0xb806050c) != 0xbeef9999) {
                MISC_INFO("Set WOL Pin to low for the falling edge trigger of the ext. micom!\n");
                MISC_INFO("%08x\n", rtd_inl(0xb8061100));
                MISC_INFO("%08x\n", rtd_inl(0xb8061108));
                // Set WOL Pin to low for the falling edge trigger of the ext. micom
                rtd_outl(0xb8061108, (rtd_inl(0xb8061108) & ~(1 << 6)));
                MISC_INFO("%08x\n", rtd_inl(0xb8061108));
        }
        // Resume
        rtl8168_resume(network_devs);
        MISC_INFO("ether driver resume for WOL\n");
        rtd_outl(0xb806050c, 0x0);
        MISC_INFO("stop wol_polling thread!\n");
        kthread_stop(thread_wol);

        return 0;
}
#endif

#if !defined(CONFIG_REALTEK_INT_MICOM)
#if 1
static int wolether_polling(void *arg)
{
        //pm_message_t pmstate;

        while(!kthread_should_stop()) {
                msleep(300);
                if ( rtd_inl(0xb8060110) == 0x9021affe )
                        break;
                if ( rtd_inl(0xb8060110) == 0xbeef8888 )
                        break;
        }
        if ( rtd_inl(0xb8060110) == 0xbeef8888 ) {
                MISC_INFO("wolether_polling thread break(magic:%08x)\n", rtd_inl(0xb8060110));
                MISC_INFO("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
#if 0
                /* Reset Geth IP */
                MISC_INFO("Reset Geth IP!!\n");
                rtd_outl(0xb8016037, rtd_inl(0xb8016037) | 0x10); // BIT4
                MISC_INFO("checking rtd_inl(0xb8016037)=%08x \n", rtd_inl(0xb8016037));
                mdelay(50);
                // enable WOL IP setting for MAC
                MISC_INFO("enable WOL IP setting for MAC!!!\n");
                rtd_outl(0xb8016050, 0x1dcfc010);
                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                mdelay(50);
                //rtd_outl(0xb80160d0, 0xb2000021);
                rtd_outl(0xb80160d0, 0x22000021); // fix auto wakeup issue
                MISC_INFO("rtd_inl(0xb80160d0)=%08x\n", rtd_inl(0xb80160d0));
                mdelay(50);
                rtd_outl(0xb8016050, 0x1dcf0010);
                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                mdelay(50);
                // 0x18016c10[0] = 0x1 (default =0) before enable emcu clock
                // gmac clock select  0: gmac/emcu2 use bus clock 1: gmac/emcu2 use GPHY125MHz clock
                rtd_outl(0xb8016c10, (rtd_inl(0xb8016c10) & 0xfffffffe) | 0x1);
                mdelay(50);
                rtd_inl(0xb8016c10); // for asynchronous access
#endif
                //rtl8168_suspend(network_devs, pmstate);
                //MISC_INFO("ether driver suspend for WOL\n");

#if defined(CONFIG_REALTEK_INT_MICOM)
                //rtd_outl(0xb8060110, 0xbeef6666);
#else
                rtd_outl(0xb8060110, 0xbeef6666);
#endif
        }
        MISC_INFO("stop wolether_polling thread!\n");
        kthread_stop(thread_wolether);

        return 0;
}
#endif
#endif
/*
 * The QSM Off implementation: tunning OSC & enable OSC
 */
void MISC_OSC_Clk_init(void)
{
    //   rtd_outl(0x0044, rtd_inl(0x0044)|_BIT11); //emb osc clock enable
    rtd_outl(0xb8060044, _BIT11 | _BIT0);

    //   rtd_outl(0x0044, rtd_inl(0x0044)&~_BIT11); //emb osc clock disable
    rtd_outl(0xb8060044, _BIT11);

    //   rtd_outl(0x0034, rtd_inl(0x0034)&~_BIT12); //hold osc reset
    rtd_outl(0xb8060034, _BIT12);

    //   rtd_outl(0x0034, rtd_inl(0x0034)|_BIT12); //release osc reset
    rtd_outl(0xb8060034, _BIT12 | _BIT0);

    //delayms(0x1); //delay at least > 150us
    udelay(200);
    //   rtd_outl(0x0044, rtd_inl(0x0044)|_BIT11); //emb osc clock enable
    rtd_outl(0xb8060044, _BIT11 | _BIT0);
}

void MISC_OSC_tracking(void)
{
    rtd_outl(0xb8060058, rtd_inl(0xb8060058)|_BIT1);// select external 27Mhz
    //rtd_setBits(0x0058, _BIT1);

    rtd_outl(0xb80600d0, rtd_inl(0xb80600d0)&~_BIT1);// auto mode
    //rtd_clearBits(0x00d0, _BIT1);

    rtd_outl(0xb80600d0, rtd_inl(0xb80600d0)|_BIT3);// osc_track_lock_en
    //rtd_setBits(0x00d0, _BIT3);

    rtd_outl(0xb80600d0, rtd_inl(0xb80600d0)|_BIT2);// osc_calibratable_en
    //rtd_setBits(0x00d0, _BIT2);

    rtd_outl(0xb80600d0, rtd_inl(0xb80600d0)|_BIT0);// Osc tracking enable
    //rtd_setBits(0x00d0, _BIT0);

    //    while(!(rtd_inl(0xb80600d4) & _BIT3));//polling for osc_track_lock_status=1
    while(!(rtd_inl(0xb80600d4) & _BIT3));//polling for osc_track_lock_status=1

    rtd_outl(0xb80600d0, rtd_inl(0xb80600d0)&~_BIT0);// Osc tracking disable
    //rtd_clearBits(0x00d0, _BIT0);

    rtd_outl(0xb8060058, rtd_inl(0xb8060058)&~_BIT1);// select ring_osc
    //rtd_clearBits(0x0058, _BIT1);
}

/*
 * The ioctl() implementation
 */

void misc_get_micom_log()
{
    // Check if Micom send log to SHM
    if(rtd_inl(0xB8060570) == 0x2379beef)
    {
        UINT8 micomLog[MICOM_LOG_LEN] = {0};
        UINT8 j = 0;
        UINT32 regdata = 0;
        UINT8 dataLen = 0;

        // Copy Log from SHM
        for( dataLen = 0 ; dataLen < MICOM_LOG_LEN ; dataLen++ ) {
            if((j % 4) == 0) {
                regdata = rtd_inl( MICOM_LOG_REG + ( ( dataLen /4 )	<< 2 ) );
            }

            micomLog[dataLen] = ( regdata >> (j*8) )& 0xFF;
            j = (j+1)%4;
        }

        // Clear SHM log area
        for(j = 0; j < (MICOM_LOG_LEN/4); j++){
            rtd_outl((MICOM_LOG_REG+(j*4)), 0);
        }

        printk("%s\n", micomLog);

        /*
        // printk("[MICOM]\n");      
        // Print micom log for rtd log
         for(j = 0; j < MICOM_LOG_LEN; j++){
            // skip NULL char
            if(micomLog[j] == 0)
                continue;

            printk("%c", micomLog[j]);
        }  */   
    }

    // Inform Micom to send the log
    rtd_outl(0xB8060570, 0xbeef2379);
}

long misc_ioctl(
        struct file             *filp,
        unsigned int            cmd,
        unsigned long           arg)
{
        struct misc_dev *pdev = filp->private_data;
        int retval = 0;
#if defined(CONFIG_REALTEK_INT_MICOM)
        unsigned int i = 0;
        //unsigned int i;                    error: unused variable
        //pm_message_t pmstate;     error: unused variable
#else
        unsigned int i;
        pm_message_t pmstate;
#endif


#if defined(CONFIG_REALTEK_INT_MICOM)
    switch(cmd){
        case MISC_IPC_READ:
        case MISC_IPC_WRITE:
        case MISC_UCOM_WRITE:
        case MISC_UCOM_READ:
        case MISC_UCOM_WHOLE_CHIP_RESET:
            break;

        default:
            goto NON_IPC_UCOM_CMD;
    }

    /* seperate micom IPC/UCOM commands and use different semaphore */
    if (down_interruptible(&micom_sem))
        return -ERESTARTSYS;

    switch(cmd) {
            case MISC_IPC_READ:{
                IPC_ARG_T IPC_data = {0};
                UINT8 RtryCnt = 0;
                UINT8 *CmdFromHal = NULL;
                UINT8 *DataFromHal = NULL;

                misc_get_micom_log();
                if(copy_from_user((void *)&IPC_data, (void __user *)arg, sizeof(IPC_ARG_T))) {
                    MISC_INFO("[MISC_IPC_READ] copy_from_user fail...\n");
                    retval = -EFAULT;
                    break;
                }


                if(IPC_data.CmdSize)
                {
                    CmdFromHal = kmalloc(IPC_data.CmdSize, GFP_ATOMIC);
                    if(copy_from_user((void *)CmdFromHal, to_user_ptr(IPC_data.pCmdBuf), IPC_data.CmdSize)) {
                        MISC_INFO("[MISC_IPC_READ-2] copy_from_user fail...\n");
                        if(CmdFromHal){
                            kfree(CmdFromHal);
                            CmdFromHal = NULL;
                        }
                        retval = -EFAULT;
		                goto break_MISC_IPC_READ;
                        break;
                    }
                }

                if(IPC_data.DataSize)
                {
                    DataFromHal = kmalloc(IPC_data.DataSize, GFP_ATOMIC);
                    if(copy_from_user((void *)DataFromHal, to_user_ptr(IPC_data.pDataBuf), IPC_data.DataSize)) {
                        MISC_INFO("[MISC_IPC_READ-3] copy_from_user fail...\n");
                        retval = -EFAULT;
                        if(DataFromHal){
                            kfree(DataFromHal);
                            DataFromHal = NULL;
                        }
                        goto break_MISC_IPC_READ;
                        /* fixme, CmdFromHal not free */
                        break;
                    }
                }

#if 0   // for IPC check
if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) {

                if(IPC_data.CmdSize)
                {
                    MISC_INFO("[MISC_IPC_READ] CmdSize: %d\n", IPC_data.CmdSize);
                    for(i = 0; i < IPC_data.CmdSize; i++)
                    {
                        MISC_INFO("[MISC_IPC_READ] Cmd:%#02x", CmdFromHal[i]);
                    }
                    MISC_INFO("\n");
                }
                else
                    MISC_INFO("[MISC_IPC_READ] No Cmd\n");
}
#endif

                RtryCnt = IPC_data.retryCnt;

                if(RtryCnt < 0)
                {
                    MISC_ERR("[MISC_IPC_READ] RtryCnt:%d is less than 0!\n", RtryCnt);
                    RtryCnt = 0;
                }

                if(CmdFromHal != NULL &&  DataFromHal != NULL) 
                {
                    for(; RtryCnt >= 0 ; RtryCnt--)
                    {
                        if(do_intMicomShareMemory(CmdFromHal, IPC_data.CmdSize, SH_MEM_OP_WRITE) == SH_MEM_OK)
                        {
                            if(do_intMicomShareMemory(DataFromHal, IPC_data.DataSize, SH_MEM_OP_READ) == SH_MEM_OK)
                            {
                                if(copy_to_user(to_user_ptr(IPC_data.pDataBuf), (void *)DataFromHal, IPC_data.DataSize)) {
                                    MISC_INFO("[MISC_IPC_READ-4] copy_from_user fail...\n");
                                    retval = -EFAULT;
                                    goto break_MISC_IPC_READ;
                                    /* fixme, is this to break for-loop ? or to break switch-case ? */
                                    break;
                                }
                                break;
                            }
                        }
                        MISC_ERR("[MISC_IPC_READ] failed, RtryCnt:%d!\n", RtryCnt);
                    }

#if 1   // for IPC check
if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) {

                if(IPC_data.DataSize)
                {
                    MISC_INFO("[MISC_IPC_READ] DataSize: %d\n", IPC_data.DataSize);
                    for(i = 0; i < IPC_data.DataSize; i++)
                    {
                          MISC_INFO("[MISC_IPC_READ] Data:%#02x\n", DataFromHal[i]);
                    }
                    //MISC_INFO("\n");
                }
                else
                    MISC_INFO("[MISC_IPC_READ] No Data\n");
                rtd_outl(MICOM_IRDA_CTRL_reg, 0x1);
}
#endif
                }

	break_MISC_IPC_READ:
                if(CmdFromHal)
                    kfree(CmdFromHal);

                if(DataFromHal)
                    kfree(DataFromHal);
            }
            break;

            case MISC_IPC_WRITE:{
                IPC_ARG_T IPC_data = {0};
                UINT8 RtryCnt = 0;
                UINT8 *CmdFromHal = NULL;
                UINT8 *DataFromHal = NULL;

                if(copy_from_user((void *)&IPC_data, (void __user *)arg, sizeof(IPC_ARG_T))) {
                    MISC_INFO("[MISC_IPC_WRITE] copy_from_user fail...\n");
                    retval = -EFAULT;
                    break;
                }

                if(IPC_data.CmdSize)
                {
                    CmdFromHal = kmalloc(IPC_data.CmdSize, GFP_ATOMIC);
                    if(copy_from_user((void *)CmdFromHal, to_user_ptr(IPC_data.pCmdBuf), IPC_data.CmdSize)) {
                        MISC_INFO("[MISC_IPC_WRITE-2] copy_from_user fail...\n");
                        retval = -EFAULT;
                        goto end_ipc_write;
                    }
                }

                if(IPC_data.DataSize)
                {
                    DataFromHal = kmalloc(IPC_data.DataSize, GFP_ATOMIC);
                    if(copy_from_user((void *)DataFromHal, to_user_ptr(IPC_data.pDataBuf), IPC_data.DataSize)) {
                        MISC_INFO("[MISC_IPC_WRITE-3] copy_from_user fail...\n");
                        retval = -EFAULT;
                        goto end_ipc_write;
                    }
                }

#if 0   // for IPC check
if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) {

                if(IPC_data.CmdSize)
                {
                    MISC_INFO("[MISC_IPC_WRITE] CmdSize: %d\n", IPC_data.CmdSize);
                    for(i = 0; i < IPC_data.CmdSize; i++)
                    {
                        MISC_INFO("[MISC_IPC_WRITE] Cmd:%#02x \n", CmdFromHal[i]);
                    }
                    MISC_INFO("\n");
                }
                else
                    MISC_INFO("[MISC_IPC_WRITE] No Cmd\n");

                if(IPC_data.DataSize)
                {
                    MISC_INFO("[MISC_IPC_WRITE] DataSize: %d\n", IPC_data.DataSize);
                    for(i = 0; i < IPC_data.DataSize; i++)
                    {
                        MISC_INFO("[MISC_IPC_WRITE] Data:%#02x\n ", DataFromHal[i]);
                    }
                    MISC_INFO("\n");
                }
                else
                    MISC_INFO("[MISC_IPC_WRITE] No Data\n");
}
#endif

                RtryCnt = IPC_data.retryCnt;

                if(RtryCnt < 0)
                {
                    MISC_ERR("[MISC_IPC_WRITE] RtryCnt:%d is less than 0!\n", RtryCnt);
                    RtryCnt = 0;
                }

                if(CmdFromHal != NULL &&  DataFromHal != NULL) 
                {
                    for(; RtryCnt >= 0 ; RtryCnt--)
                    {
                        if(do_intMicomShareMemory(DataFromHal, IPC_data.DataSize, SH_MEM_OP_WRITE) == SH_MEM_OK)
                            break;

                        MISC_ERR("[MISC_IPC_WRITE] failed, RtryCnt:%d!\n", RtryCnt);
                    }

#if 0   // for IPC check
if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) {

                if(IPC_data.DataSize)
                {
                    MISC_INFO("[MISC_IPC_WRITE] DataSize: %d\n", IPC_data.DataSize);
                    for(i = 0; i < IPC_data.DataSize; i++)
                    {
                        MISC_INFO("[MISC_IPC_WRITE] Data:%#02x \n", DataFromHal[i]);
                    }
                    MISC_INFO("\n");
                }
                else
                    MISC_INFO("[MISC_IPC_WRITE] No Data\n");
                rtd_outl(MICOM_IRDA_CTRL_reg, 0x1);
}
#endif
                }

end_ipc_write:

                if(CmdFromHal)
                    kfree(CmdFromHal);

                if(DataFromHal)
                    kfree(DataFromHal);
            }
            break;

            case MISC_UCOM_WRITE:{
                UCOM_ARG_T UCOM_data = {0};
                UINT8 *DataFromHal = NULL;

                if(copy_from_user((void *)&UCOM_data, (void __user *)arg, sizeof(UCOM_ARG_T))) {
                    MISC_ERR("[MISC_IPC_WRITE] copy_from_user fail...\n");
                    retval = -EFAULT;
                    break;
                }

                if(UCOM_data.DataSize)
                {
                    DataFromHal = kmalloc(UCOM_data.DataSize, GFP_ATOMIC);
                    if(copy_from_user((void *)DataFromHal, to_user_ptr(UCOM_data.pDataBuf), UCOM_data.DataSize)) {
                        MISC_ERR("[MISC_IPC_WRITE-3] copy_from_user fail...\n");
                        retval = -EFAULT;
                        goto end_ucom_write;
                    }
                }

                if( DataFromHal != NULL) 
                {
                    if(DataFromHal[0] == MICOM_WRITE_PANEL_ONOFF)
                    {
                        struct sched_param param;
                        param.sched_priority = 99;
                        if (sched_setscheduler(current, SCHED_FIFO, &param) == -1)
                            printk("Raise the priority of panel power sequence process fail\n");
                        else{
                            printk("Panel power sequence process policy:%d prio:%d\n", current->policy, current->prio);
                        }
                    }

                    if(do_intMicomShareMemory(DataFromHal, UCOM_data.DataSize, SH_MEM_OP_WRITE) != SH_MEM_OK)
                        MISC_ERR("[MISC_IPC_WRITE] failed!\n");

#if 0   // for IPC check
    if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) {
        if(UCOM_data.DataSize)
        {
            MISC_INFO("[MISC_UCOM_WRITE] DataSize: %d\n", UCOM_data.DataSize);
            for(i = 0; i < UCOM_data.DataSize; i++)
            {
                MISC_INFO("[MISC_UCOM_WRITE] Data:%#02x \n", DataFromHal[i]);
            }
            MISC_INFO("\n");
        }
        else
            MISC_INFO("[MISC_UCOM_WRITE] No Data\n");
        rtd_outl(MICOM_IRDA_CTRL_reg, 0x1);
    }
#endif
                    }

end_ucom_write:
                if(DataFromHal)
                    kfree(DataFromHal);
            }
            break;

            case MISC_UCOM_READ:{
                UCOM_ARG_T UCOM_data = {0};
                UINT8 *DataFromHal = NULL;

                misc_get_micom_log();
                if(copy_from_user((void *)&UCOM_data, (void __user *)arg, sizeof(UCOM_ARG_T))) {
                    MISC_ERR("[MISC_IPC_READ] copy_from_user fail...\n");
                    retval = -EFAULT;
                    break;
                }

                if(UCOM_data.DataSize)
                {
                    DataFromHal = kmalloc(UCOM_data.DataSize, GFP_ATOMIC);
                }

                // Special handle for STR resume Panel_CTL timing
                if( DataFromHal != NULL) 
                {
                    if(UCOM_data.Cmd == 0xD1)
                    {
                        DataFromHal[0] = (rtd_inl(0xb8061108) & _BIT5) ? 1 : 0;
                        if(copy_to_user(to_user_ptr(UCOM_data.pDataBuf), (void *)DataFromHal, UCOM_data.DataSize)) {
                            MISC_ERR("[MISC_IPC_READ-5] copy_from_user fail...\n");
                            retval = -EFAULT;
                        }
                        goto end_ucom_read;
                    }

                    if(do_intMicomShareMemory(&(UCOM_data.Cmd), 1, SH_MEM_OP_WRITE) == SH_MEM_OK)
                    {
                        if(do_intMicomShareMemory(DataFromHal, UCOM_data.DataSize, SH_MEM_OP_READ) == SH_MEM_OK)
                        {
                            if(copy_to_user(to_user_ptr(UCOM_data.pDataBuf), (void *)DataFromHal, UCOM_data.DataSize)) {
                                MISC_ERR("[MISC_IPC_READ-4] copy_from_user fail...\n");
                                retval = -EFAULT;
                                //break;
                            }
                            //break;
                        }
                    }

#if 1   // for IPC check
    if(rtd_inl(MICOM_IRDA_CTRL_reg) == 0x2) { 
        if(UCOM_data.DataSize)
        {
            MISC_INFO("[MISC_UCOM_READ] DataSize: %d\n", UCOM_data.DataSize);
            for(i = 0; i < UCOM_data.DataSize; i++)
            {
                  MISC_INFO("[MISC_UCOM_READ] Data:%#02x\n", DataFromHal[i]);
            }
            //MISC_INFO("\n");
        }
        else
            MISC_INFO("[MISC_UCOM_READ] No Data\n");
        rtd_outl(MICOM_IRDA_CTRL_reg, 0x1);
    }
#endif
                    }
end_ucom_read:
                if(DataFromHal)
                    kfree(DataFromHal);
            }
            break;
            case MISC_UCOM_WHOLE_CHIP_RESET:{
            	// [AMP_MUTE]
            	// GPO_Set: High (Output)
            	// IO_Set: Output (GPO)
            	rtk_gpio_output(rtk_gpio_id(ISO_GPIO, 54), 1);
            	
            	// PWM0 - MIS_PWM output disable
            	rtd_outl(0xb8061284, rtd_inl(0xb8061284) & ~_BIT29);
            	
            	// Set Output
            	// [INV_CTL]
            	// GPO_Set: Low (Output)
            	// IO_Set: Output (GPO)
            	rtk_gpio_output(rtk_gpio_id(ISO_GPIO, 5), 0);
            	
            	// [POWER_ON/OFF2_3] 
            	// GPO_Set: Low (Output)
            	// IO_Set: Output (GPO)
            	rtk_gpio_output(rtk_gpio_id(ISO_GPIO, 44), 0);
            }
            break;  
        default:{
            up(&micom_sem);
            goto NON_IPC_UCOM_CMD;
        }
    }

    up(&micom_sem);
    return retval;
    /* seperate micom IPC/UCOM commands and use different semaphore */
#endif

NON_IPC_UCOM_CMD:
        if (down_interruptible(&pdev->sem))
                return -ERESTARTSYS;

        switch(cmd) {
                case MISC_GET_STR_ENABLE: {
                        int qsmEnable;            
                        if(rtd_inl(0xB8060100) == 0x2379beef)
                            qsmEnable = 1;
                        else
                            qsmEnable = 0;

                       if(copy_to_user((void __user *)arg, (void *)&qsmEnable, sizeof(int))) {
                                retval = -EFAULT;
                                break;
                        }
                    }
                break;            
                case MISC_WAKEUP_ECPU: {
                       MISC_INFO( "Wake-up ECPU...\n");
#if defined(CONFIG_REALTEK_INT_MICOM)
#if 0
                       MISC_INFO("289x rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                       if(rtd_inl(0xb8060110) == 0x9021aebe) {  // sync system timer between A/V CPU
                           rtd_outl(0xb8060110, 0);
                       //    rtd_outl(STB_ST_SRST1_reg, BIT(9));
		       }
                       //rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));       // clk enable
                       //udelay(500);
                       //rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));
#endif
#else
                       MISC_INFO("289x rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                       if(rtd_inl(0xb8060110) == 0x9021aebe) {  // sync system timer between A/V CPU
                           rtd_outl(0xb8060110, 0);
                           rtd_outl(STB_ST_SRST1_reg, BIT(9));
														}
                       rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));       // clk enable
                       udelay(500);
                       rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));

#endif
                }
                break;

                case MISC_SET_WOL_ENABLE: {
#if defined(CONFIG_REALTEK_INT_MICOM)
			fWOLEnable = arg;
                        MISC_INFO( "WOL enable flag = %x\n", fWOLEnable);
#if 0   // not used in intmicom
                        int nEnable = arg;
			//pm_message_t pmstate;

#ifdef CONFIG_RTK_KDRV_R8168

                        if(nEnable) {
                                // stop wol_polling thread and resume ether driver in case of Warm standby mode is on
                                /*if ( rtd_inl(0xb806050c) == 0xbeef7777 ) {
                                MISC_INFO( "Enter WOL flow in Warm standby mode...\n");
                                rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00
                                        // set WOL flag disabled for emcu
                                        MISC_INFO("set WOL flag disabled for emcu\n");
                                        rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                        // stop wol_polling thread and resume ether driver in case of Warm standby mode is on
                                        rtd_outl(0xb806050c, 0xbeef9999);
                                        while ( rtd_inl(0xb806050c) == 0xbeef9999 ) {
                                                MISC_INFO( "stop wol_polling thread waiting! %x\n", rtd_inl(0xb806050c));
                                                msleep(300);
                                        }
						         }
						         else {
                                         if(rtd_inl(0xb8060110) ==0x9021affa) {
		                                  MISC_INFO( "Enter twice WOL flow...\n");
										  break;
								       }
								  MISC_INFO( "Enable WOL...\n");
								  rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00	                                    MISC_INFO( "Enter WOL flow...\n");
	                                MISC_INFO( "Enter WOL flow...\n");
                                        MISC_INFO( "%s: WOL:%08x\n", __FUNCTION__, rtd_inl(0xb80160b0));   // check 0x1e00(default)  -->WOL 0x9e00
                                        MISC_INFO( "warm standby wol_polling thread does not exist!\n");
                                }*/
#if 1
#if 0
                                /* Reset Geth IP */
                                MISC_INFO("Reset Geth IP!!\n");
                                rtd_outl(0xb8016037, 0x10); // BIT4
                                MISC_INFO("checking rtd_inl(0xb8016037)=%08x \n", rtd_inl(0xb8016037));
                                mdelay(50);
                                // enable WOL IP setting for PHY
#if 0
                                MISC_INFO("enable WOL IP setting for PHY!!!\n");
                                rtd_outl(0xb8016060, 0x841f0bc4);       // Goto to Page bc4
                                MISC_INFO("rtd_inl(0xb8016060)=%08x\n", rtd_inl(0xb8016060));
                                rtd_outl(0xb8016060, 0x84150203);       // set internal LDO turn on
                                MISC_INFO("rtd_inl(0xb8016060)=%08x\n", rtd_inl(0xb8016060));
                                udelay(500);
#endif
                                // enable WOL IP setting for MAC
                                MISC_INFO("enable WOL IP setting for MAC!!!\n");
                                rtd_outl(0xb8016050, 0x1dcfc010);
                                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                                mdelay(50);
                                //rtd_outl(0xb80160d0, 0xb2000021);
                                rtd_outl(0xb80160d0, 0x22000021); // fix auto wakeup issue
                                MISC_INFO("rtd_inl(0xb80160d0)=%08x\n", rtd_inl(0xb80160d0));
                                mdelay(50);
                                rtd_outl(0xb8016050, 0x1dcf0010);
                                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                                mdelay(50);
#endif
                                MISC_INFO("289x  rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));

                                if((rtd_inl(0xb8060110) == 0x9021aebe) || (rtd_inl(0xb8060110) == 0x9021affc)) {
                                        //rtd_outl(STB_ST_SRST1_reg, BIT(9));
                                }
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                /*for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);*/
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                //rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // 0x18016c10[0] = 0x1 (default =0) before enable emcu clock
                                // gmac clock select  0: gmac/emcu2 use bus clock 1: gmac/emcu2 use GPHY125MHz clock
                                rtd_outl(0xb8016c10, (rtd_inl(0xb8016c10) & 0xfffffffe) | 0x1);
                                mdelay(50);
                                rtd_inl(0xb8016c10); // for asynchronous access
                                // run a thread for WOL polling
                                thread_wolether = kthread_run(wolether_polling, NULL, "wolether_polling");
                                if(IS_ERR(thread_wolether)) {
                                        MISC_INFO("create wolether_polling thread failed\n");
                                        retval = -EFAULT;
                                        break;
																				 }
				MISC_INFO("create wolether_polling thread successfully\n");

				MISC_INFO("ether driver suspend for WOL before\n");
				rtl8168_suspend(network_devs, pmstate);
				MISC_INFO("ether driver suspend for WOL done\n");
#else
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // run a thread for WOL polling
                                //thread_wolether = kthread_run(wolether_polling, NULL, "wolether_polling");
                                //if(IS_ERR(thread_wolether)) {
                                //        MISC_INFO("create wolether_polling thread failed\n");
                                //        retval = -EFAULT;
                                //        break;
 																				//}
                                //MISC_INFO("create wolether_polling thread successfully\n");
#endif
                                // enable emcu clock
                             MISC_INFO( "Wake-up ECPU...\n");
                             MISC_INFO("289x  rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                             rtd_outl(0xb8060110, 0x9021affa);
                             //rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));  // clk enable for emcu
                             //udelay(500);
                             //rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));        // release reset for emcu
                        } else {
                                MISC_INFO( "Disable WOL...\n");
                                // clear STR flag for emcu
                                rtd_outl(0xb8060110, 0x00000000);
				//rtd_outl(STB_ST_SRST1_reg, _BIT9);//reset 8051
                                // clear share memory for emcu
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                /*for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);*/
                                // set WOL flag disabled for emcu
                                MISC_INFO("set WOL flag disabled for emcu\n");
                                //rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                // disable emcu clock
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~BIT(9));    // clk disable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) & ~BIT(9));  // hold reset for emcu
                        }
#endif
#endif
#else
                        int nEnable = arg;
			//pm_message_t pmstate;

#ifdef CONFIG_RTK_KDRV_R8168

                        if(nEnable) {
                                // stop wol_polling thread and resume ether driver in case of Warm standby mode is on
                                if ( rtd_inl(0xb806050c) == 0xbeef7777 ) {
                                MISC_INFO( "Enter WOL flow in Warm standby mode...\n");
                                rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00
                                        // set WOL flag disabled for emcu
                                        MISC_INFO("set WOL flag disabled for emcu\n");
                                        rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                        // stop wol_polling thread and resume ether driver in case of Warm standby mode is on
                                        rtd_outl(0xb806050c, 0xbeef9999);
                                        while ( rtd_inl(0xb806050c) == 0xbeef9999 ) {
                                                MISC_INFO( "stop wol_polling thread waiting! %x\n", rtd_inl(0xb806050c));
                                                msleep(300);
                                        }
						         }
						         else {
                                         if(rtd_inl(0xb8060110) ==0x9021affa) {
		                                  MISC_INFO( "Enter twice WOL flow...\n");
										  break;
								       }
								  MISC_INFO( "Enable WOL...\n");
								  rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00	                                    MISC_INFO( "Enter WOL flow...\n");
	                                MISC_INFO( "Enter WOL flow...\n");
                                        MISC_INFO( "%s: WOL:%08x\n", __FUNCTION__, rtd_inl(0xb80160b0));   // check 0x1e00(default)  -->WOL 0x9e00
                                        MISC_INFO( "warm standby wol_polling thread does not exist!\n");
                                }
#if 1
#if 0
                                /* Reset Geth IP */
                                MISC_INFO("Reset Geth IP!!\n");
                                rtd_outl(0xb8016037, 0x10); // BIT4
                                MISC_INFO("checking rtd_inl(0xb8016037)=%08x \n", rtd_inl(0xb8016037));
                                mdelay(50);
                                // enable WOL IP setting for PHY
#if 0
                                MISC_INFO("enable WOL IP setting for PHY!!!\n");
                                rtd_outl(0xb8016060, 0x841f0bc4);       // Goto to Page bc4
                                MISC_INFO("rtd_inl(0xb8016060)=%08x\n", rtd_inl(0xb8016060));
                                rtd_outl(0xb8016060, 0x84150203);       // set internal LDO turn on
                                MISC_INFO("rtd_inl(0xb8016060)=%08x\n", rtd_inl(0xb8016060));
                                udelay(500);
#endif
                                // enable WOL IP setting for MAC
                                MISC_INFO("enable WOL IP setting for MAC!!!\n");
                                rtd_outl(0xb8016050, 0x1dcfc010);
                                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                                mdelay(50);
                                //rtd_outl(0xb80160d0, 0xb2000021);
                                rtd_outl(0xb80160d0, 0x22000021); // fix auto wakeup issue
                                MISC_INFO("rtd_inl(0xb80160d0)=%08x\n", rtd_inl(0xb80160d0));
                                mdelay(50);
                                rtd_outl(0xb8016050, 0x1dcf0010);
                                MISC_INFO("rtd_inl(0xb8016050)=%08x\n", rtd_inl(0xb8016050));
                                mdelay(50);
#endif
                                MISC_INFO("289x  rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));

                                if((rtd_inl(0xb8060110) == 0x9021aebe) || (rtd_inl(0xb8060110) == 0x9021affc)) {
                                        rtd_outl(STB_ST_SRST1_reg, BIT(9));
                                }
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // 0x18016c10[0] = 0x1 (default =0) before enable emcu clock
                                // gmac clock select  0: gmac/emcu2 use bus clock 1: gmac/emcu2 use GPHY125MHz clock
                                rtd_outl(0xb8016c10, (rtd_inl(0xb8016c10) & 0xfffffffe) | 0x1);
                                mdelay(50);
                                rtd_inl(0xb8016c10); // for asynchronous access
                                // run a thread for WOL polling
                                thread_wolether = kthread_run(wolether_polling, NULL, "wolether_polling");
                                if(IS_ERR(thread_wolether)) {
                                        MISC_INFO("create wolether_polling thread failed\n");
                                        retval = -EFAULT;
                                        break;
																				 }
				MISC_INFO("create wolether_polling thread successfully\n");

				MISC_INFO("ether driver suspend for WOL before\n");
				rtl8168_suspend(network_devs, pmstate);
				MISC_INFO("ether driver suspend for WOL done\n");
#else
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // run a thread for WOL polling
                                //thread_wolether = kthread_run(wolether_polling, NULL, "wolether_polling");
                                //if(IS_ERR(thread_wolether)) {
                                //        MISC_INFO("create wolether_polling thread failed\n");
                                //        retval = -EFAULT;
                                //        break;
 																				//}
                                //MISC_INFO("create wolether_polling thread successfully\n");
#endif
                                // enable emcu clock
                             MISC_INFO( "Wake-up ECPU...\n");
                             MISC_INFO("289x  rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                             rtd_outl(0xb8060110, 0x9021affa);
                             rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));  // clk enable for emcu
                             udelay(500);
                             rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));        // release reset for emcu
                        } else {
                                MISC_INFO( "Disable WOL...\n");
                                // clear STR flag for emcu
                                rtd_outl(0xb8060110, 0x00000000);
																				rtd_outl(STB_ST_SRST1_reg, _BIT9);//reset 8051
                                // clear share memory for emcu
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag disabled for emcu
                                MISC_INFO("set WOL flag disabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                // disable emcu clock
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~BIT(9));    // clk disable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) & ~BIT(9));  // hold reset for emcu
                        }
#endif
#endif
                }
                break;
                case MISC_GET_WOL_ENABLE: {
#ifndef CONFIG_REALTEK_INT_MICOM
                        int nEnable;

                        if((rtd_inl(0xb8060500) & 0x01000000) != 0)
                                nEnable = true;
                        else
                                nEnable = false;


                        if(copy_to_user((void __user *)arg, (void *)&nEnable, sizeof(int))) {
                                retval = -EFAULT;
                                break;
                        }
#endif
                }
                break;
#ifdef CONFIG_RTK_KDRV_RTICE
                case MISC_SET_DEBUG_LOCK: {

                        int nLock = arg;
                        extern  UINT8 g_ByPassRTICECmd;
			if( !g_bIsBackupJTAGPinmux )
			{
				//backup original setting
				//default disable HDMI2JTAG
				ST_GPIO_ST_CFG_5 = rtd_inl( PINMUX_ST_GPIO_ST_CFG_5_reg );
				ST_GPIO_ST_CFG_6 = rtd_inl( PINMUX_ST_GPIO_ST_CFG_6_reg );
				g_bIsBackupJTAGPinmux = 1;
			}
                        if(nLock) {
                                //pin switch to uart2
                                //according to KTASKWBS-2863, always enable log.
                                //rtd_maskl(0xb806020C, 0xFF0FFFFF, 0x00600000);//06: uart2_rXdi_src0,<I> 0x1800_020C[23:20]
                                //  rtd_maskl(0xb806020C, 0x0FFFFFFF, 0x60000000);//06: uart2_tXdo,<O> 0x1800_020C[31:28]

				//U2R
				rtd_maskl(U2R_CONTROL_REG, 0xFFFFFFFB, 0x00000000);//u2r setting
				pr_debug("u2r dis: %x %x\n",U2R_CONTROL_REG,rtd_inl(U2R_CONTROL_REG));

                                //disable JTAG debug with default settings
				rtd_outl( PINMUX_ST_GPIO_ST_CFG_5_reg, ST_GPIO_ST_CFG_5);
				rtd_outl( PINMUX_ST_GPIO_ST_CFG_6_reg, ST_GPIO_ST_CFG_6);


                                //disable connect RTICE
                                g_ByPassRTICECmd = 1;
                        } else {
                                //enable JTAG debug
                                //pin switch to uart1
                                //according to KTASKWBS-2863, always enable log.
                                //rtd_maskl(0xb806020C, 0xFF0FFFFF, 0x00900000);//09: uart0_rXdi_src0,<I> 0x1806_020C[23:20]
                                //  rtd_maskl(0xb806020C, 0x0FFFFFFF, 0x90000000);//09: uart0_tXdo,<O> 0x1806_020C[31:28]

				//U2R
				rtd_maskl(U2R_CONTROL_REG, 0xFFFFFFFB, 0x00000004);//u2r setting
				pr_debug("u2r en: %x %x\n",U2R_CONTROL_REG,rtd_inl(U2R_CONTROL_REG));

//enable JTAG debug
				rtd_part_outl(PINMUX_ST_GPIO_ST_CFG_5_reg, 31, 28, 0x3);//EJTAG_TCLK_CLK
				rtd_part_outl(PINMUX_ST_GPIO_ST_CFG_5_reg, 23, 20, 0x3);//EJTAG_TMS        
				rtd_part_outl(PINMUX_ST_GPIO_ST_CFG_5_reg, 7, 4, 0x3);  //EJTAG_TDO        
				rtd_part_outl(PINMUX_ST_GPIO_ST_CFG_6_reg, 23, 20, 0x3);//EJTAG_TRST        
				rtd_part_outl(PINMUX_ST_GPIO_ST_CFG_6_reg, 31, 28, 0x3);//EJTAG_TDI
                                //enbale connect RTICE
                                g_ByPassRTICECmd = 0;

                        }
                }
                break;
#endif
                case MISC_SET_DRAM_BW_ENABLE: {

                        int nEnable = arg;

                        if(set_PC_go_status(nEnable) != 0) {
                                retval = -EFAULT;
                                break;
                        }

                }
                break;
                case MISC_GET_DRAM_BW_INFO: {
                        DRAM_BW_INFO_T stBWInfo;

                        if(get_bw_values_info(&(stBWInfo.totalBytes_chA), &(stBWInfo.percent_chA),
                                              &(stBWInfo.totalBytes_chB), &(stBWInfo.percent_chB)) != 0) {
                                retval = -EFAULT;
                                break;
                        }

                        if(copy_to_user((void __user *)arg, (void *)&stBWInfo, sizeof(DRAM_BW_INFO_T))) {
                                retval = -EFAULT;
                                break;
                        }
                }
                break;
                case MISC_SET_SPREAD_SPECTRUM: {
                        SPREAD_SPECTRUM_PARA para;
                        int nRet;
                        if(copy_from_user((void *)&para, (const void __user *)arg, sizeof(SPREAD_SPECTRUM_PARA))) {
                                retval = -EFAULT;
                                break;
                        }

                        switch (para.module) {
                                case SPREAD_SPECTRUM_MODULE_CPU:
                                        nRet = RHAL_SYS_SetCPUSpreadSpectrum(para.ratio * 25);
                                        break;
                                case SPREAD_SPECTRUM_MODULE_MAIN_0:
                                        nRet = RHAL_SYS_SetDDRSpreadSpectrum(para.ratio * 25);
                                        break;
                                case SPREAD_SPECTRUM_MODULE_DISPLAY:
                                        nRet = RHAL_SYS_SetDISPSpreadSpectrum(para.ratio * 25);
                                        break;
                                default:
                                        nRet = -1;
                                        break;
                        }

                        if(nRet != 0) {
                                retval = -EFAULT;
                                break;
                        }
                }
                break;

                case MISC_SET_RTDLOG: {
                        rtk_rtdlog_info_t rtd_log = {0};
                        if(rtdlog_info == NULL){
                            return  -EFAULT;
                        }
                        if(copy_from_user((void *)&rtd_log, (const void __user *)arg, sizeof(rtk_rtdlog_info_t))) {
                                MISC_INFO("[RTDLOG_LEVEL] copy_from_user fail...\n");
                                retval = -EFAULT;
                                break;
                        }
                        rtdlog_info->rtdlog_level = rtd_log.rtdlog_level;
                        rtdlog_info->rtdlog_module = rtd_log.rtdlog_module;
                        rtdlog_info->rtdlog_option = rtd_log.rtdlog_option;
                }
                break;
                case MISC_GET_RTDLOG: {
                        if(rtdlog_info == NULL){
                            return  -EFAULT;
                        }
                        if((unsigned int)rtd_inl(RTD_LOG_CHECKING_REGISTER) == 0xdeadbeef) {
                                /*using unused register 0xb801a610 to check whether to using assert
                                when rtd log fatal
                                */
                                rtdlog_info->rtdlog_option = (rtdlog_info->rtdlog_option | RTD_LOG_CHECK_REG);
                                //MISC_INFO("test the Reg : 0x%x, the option value:%x \n",rtd_inl(RTD_LOG_CHECKING_REGISTER), console_rtdlog.rtdlog_option );
                        }

                        if(copy_to_user((void __user *)arg, (void *)rtdlog_info, sizeof(rtk_rtdlog_info_t))) {
                                MISC_INFO("[RTDLOG_LEVEL] copy_to_user fail...\n");
                                retval = -EFAULT;
                                break;
                        }
                }
                break;

                /*case MISC_SET_RTDLOG_MODULE:
                {
                    unsigned int log_module = 0;
                    if(copy_from_user((void *)&log_module, (const void __user *)arg, sizeof(unsigned int)))
                    {
                        MISC_INFO("[RTDLOG_MODULE] copy_from_user fail...\n");
                        retval = -EFAULT;
                        break;
                    }
                    console_rtdlog_module = log_module;
                }
                break;

                case MISC_GET_RTDLOG_MODULE:
                {
                    if(copy_to_user((void __user *)arg, (void *)&console_rtdlog_module, sizeof(unsigned int)))
                    {
                        MISC_INFO("[RTDLOG_MODULE] copy_to_user fail...\n");
                        retval = -EFAULT;
                        break;
                    }
                }
                break;*/
#ifdef CONFIG_RTK_KDRV_R8168
                case MISC_SET_WARM_WOL_ENABLE: {
#if defined(CONFIG_REALTEK_INT_MICOM)
                        fWOLEnable = arg;
                        MISC_INFO( "Warm Standby WOL enable flag = %x\n", fWOLEnable);
#if 0
                        int nEnable = arg;

                        if(nEnable) {
                                MISC_INFO( "Enable Warm Standby WOL...\n");
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                /*for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);*/
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                //rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // run a thread for WOL polling
                                thread_wol = kthread_run(wol_polling, NULL, "wol_polling");
                                if(IS_ERR(thread_wol)) {
                                        MISC_INFO("create wol_polling thread failed\n");
                                        retval = -EFAULT;
                                        break;
                                }
                                MISC_INFO("create wol_polling thread successfully\n");
                                // 0x18016c10[0] = 0x1 (default =0) before enable emcu clock
                                // gmac clock select  0: gmac/emcu2 use bus clock 1: gmac/emcu2 use GPHY125MHz clock
                                //rtd_outl(0xb8016c10, (rtd_inl(0xb8016c10) & 0xfffffffe) | 0x1);
                                // enable emcu clock
                                //MISC_INFO( "Wake-up ECPU...\n");
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) | BIT(9)); // clk enable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) | BIT(9));       // release reset for emcu
                        } else {
                                MISC_INFO( "Disable WOL...\n");
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                /*for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);*/
                                // set WOL flag disabled for emcu
                                MISC_INFO("set WOL flag disabled for emcu\n");
                                //rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                // stop a thread for WOL polling
                                rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00
                                if ( (thread_wol != NULL) && ((rtd_inl(0xb80160b0) & 0xffff) != 0x9e00) ) {
                                        // Resume
                                        rtl8168_resume(network_devs);
                                        MISC_INFO( "ether driver resume for WOL\n");
                                        kthread_stop(thread_wol);
                                        MISC_INFO( "stop wol_polling thread!\n");
                                } else {
                                        MISC_INFO( "%s: WOL:%08x\n", __FUNCTION__, rtd_inl(0xb80160b0));   // check 0x1e00(default)  -->WOL 0x9e00
                                        MISC_INFO( "wol_polling thread does not exist!\n");
                                }
                                // disable emcu clock
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~BIT(9));    // clk disable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) & ~BIT(9));  // hold reset for emcu
                        }
#endif
#else
                        int nEnable = arg;

                        if(nEnable) {
                                MISC_INFO( "Enable Warm Standby WOL...\n");
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
#define RTD_SHARE_MEM_LEN       32
#define RTD_SHARE_MEM_BASE      0xb8060500
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag enabled for emcu
                                MISC_INFO("set WOL flag enabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff) | 0x01000000);
                                // run a thread for WOL polling
                                thread_wol = kthread_run(wol_polling, NULL, "wol_polling");
                                if(IS_ERR(thread_wol)) {
                                        MISC_INFO("create wol_polling thread failed\n");
                                        retval = -EFAULT;
                                        break;
                                }
                                MISC_INFO("create wol_polling thread successfully\n");
                                // 0x18016c10[0] = 0x1 (default =0) before enable emcu clock
                                // gmac clock select  0: gmac/emcu2 use bus clock 1: gmac/emcu2 use GPHY125MHz clock
                                //rtd_outl(0xb8016c10, (rtd_inl(0xb8016c10) & 0xfffffffe) | 0x1);
                                // enable emcu clock
                                //MISC_INFO( "Wake-up ECPU...\n");
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) | BIT(9)); // clk enable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) | BIT(9));       // release reset for emcu
                        } else {
                                MISC_INFO( "Disable WOL...\n");
                                // clear STR flag for emcu
                                //rtd_outl(0xb8060110, 0x00000000);
                                // clear share memory for emcu
                                MISC_INFO("clear share memory in %s...\n", __FUNCTION__);
                                for(i = 0; i < RTD_SHARE_MEM_LEN; i++)
                                        rtd_outl(RTD_SHARE_MEM_BASE + (4 * i), 0);
                                // set WOL flag disabled for emcu
                                MISC_INFO("set WOL flag disabled for emcu\n");
                                rtd_outl(0xb8060500, (rtd_inl(0xb8060500) & 0xfeffffff));
                                // stop a thread for WOL polling
                                rtd_outl(0xb80160b0, 0x70030000);  //0xf0031e00
                                if ( (thread_wol != NULL) && ((rtd_inl(0xb80160b0) & 0xffff) != 0x9e00) ) {
                                        // Resume
                                        rtl8168_resume(network_devs);
                                        MISC_INFO( "ether driver resume for WOL\n");
                                        kthread_stop(thread_wol);
                                        MISC_INFO( "stop wol_polling thread!\n");
                                } else {
                                        MISC_INFO( "%s: WOL:%08x\n", __FUNCTION__, rtd_inl(0xb80160b0));   // check 0x1e00(default)  -->WOL 0x9e00
                                        MISC_INFO( "wol_polling thread does not exist!\n");
                                }
                                // disable emcu clock
                                //rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~BIT(9));    // clk disable for emcu
                                //udelay(500);
                                //rtd_outl(STB_ST_SRST1_reg, rtd_inl(STB_ST_SRST1_reg) & ~BIT(9));  // hold reset for emcu
                        }
#endif
                }
                break;
#endif
                case MISC_GET_WARM_WOL_ENABLE: {
#ifndef CONFIG_REALTEK_INT_MICOM
                        int nEnable;

                        if((rtd_inl(0xb8060500) & 0x01000000) != 0)
                                nEnable = true;
                        else
                                nEnable = false;

                        if(copy_to_user((void __user *)arg, (void *)&nEnable, sizeof(int))) {
                                retval = -EFAULT;
                                break;
                        }
#endif
                }
                break;
                case MISC_QSM_OFF: {
                       unsigned long long cfg;
                       MISC_INFO( "QSM OFF Wake-Up Internal Micom...\n");
#if defined(CONFIG_REALTEK_INT_MICOM)
#if 0 // not used in Intmicom
#if 0
                       /* disable LVR function */
                       rtd_outl(STB_SC_LV_RST_reg, (rtd_inl(STB_SC_LV_RST_reg) & 0xffffdfff));
                       MISC_INFO("289x rtd_inl(0xb8060004)=%08x\n", rtd_inl(STB_SC_LV_RST_reg));
                       MISC_OSC_Clk_init();
                       MISC_INFO("289x OSC Clk init ok\n");
                       /* tunning OSC & enable OSC */
                       MISC_OSC_tracking();
#endif
                       if( rtd_inl(0xb8060110) != 0x9021affa) {
                           //rtd_outl(STB_ST_SRST1_reg, BIT(9));
                           rtd_outl(0xb8060110, 0x9021affc);
                           //rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));  // clk enable for emcu
                           //udelay(500);
                           //rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));        // release reset for emcu
                           udelay(500);
                           MISC_INFO("289x OSC Clk tracking ok, rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                        }
#endif
#else
#if 0
                       /* disable LVR function */
                       rtd_outl(STB_SC_LV_RST_reg, (rtd_inl(STB_SC_LV_RST_reg) & 0xffffdfff));
                       MISC_INFO("289x rtd_inl(0xb8060004)=%08x\n", rtd_inl(STB_SC_LV_RST_reg));
                       MISC_OSC_Clk_init();
                       MISC_INFO("289x OSC Clk init ok\n");
                       /* tunning OSC & enable OSC */
                       MISC_OSC_tracking();
#endif
                       if( rtd_inl(0xb8060110) != 0x9021affa) {
                           rtd_outl(STB_ST_SRST1_reg, BIT(9));
                           rtd_outl(0xb8060110, 0x9021affc);
                           rtd_outl(STB_ST_CLKEN1_reg, BIT(9) | BIT(0));  // clk enable for emcu
                           udelay(500);
                           rtd_outl(STB_ST_SRST1_reg, BIT(9) | BIT(0));        // release reset for emcu
                           udelay(500);
                           MISC_INFO("289x OSC Clk tracking ok, rtd_inl(0xb8060110)=%08x\n", rtd_inl(0xb8060110));
                        }
#endif
	                if (pcb_mgr_get_enum_info_byname("SC0_CFG", &cfg) == 0){
	                    rtk_gpio_output(rtk_gpio_id(((cfg >> 17) & 0x1) ? ISO_GPIO : MIS_GPIO, (cfg & 0xFF)), ((cfg >> 18) & 0x1) ? 0 : 1);
	                }
                }
                break;
#ifdef CONFIG_RTK_KDRV_WATCHDOG
                case MISC_SET_WATCHDOG_ENABLE: {
	                int On = arg;
			   		watchdog_enable(On);
                }
                break;
#endif
               case MISC_GET_MODULE_TEST_PIN:
			   {
					MODULE_TEST_PIN_T mt_pin;
					pcb_mgr_get_enum_info_byname("PIN_SIG", (unsigned long long *)&mt_pin.pin_signal);
					pcb_mgr_get_enum_info_byname("PIN_SCL", (unsigned long long *)&mt_pin.pin_scl);
					pcb_mgr_get_enum_info_byname("PIN_SDA", (unsigned long long *)&mt_pin.pin_sda);
					pcb_mgr_get_enum_info_byname("PIN_STATUS0", (unsigned long long *)&mt_pin.pin_status0);
					pcb_mgr_get_enum_info_byname("PIN_STATUS1", (unsigned long long *)&mt_pin.pin_status1);

					if(copy_to_user((void __user *)arg, (void *)&mt_pin, sizeof(MODULE_TEST_PIN_T))) {
							retval = -EFAULT;
							break;
					}
                }
                break;
#ifdef CONFIG_ENABLE_WOV_SUPPORT            
            case MISC_VOICE_UPDATE_SENSORY_MODEL:{
                const char pStrModelFilePath[256] = {0};
                if (get_platform() == PLATFORM_KXL)
                {
                    printk("[WOV] Into MISC_VOICE_UPDATE_SENSORY_MODEL\n");
                    if(copy_from_user((void *)pStrModelFilePath, (const char __user *)arg, sizeof(pStrModelFilePath))) {
                        MISC_ERR("[WOV] [MISC_VOICE_UPDATE_SENSORY_MODEL] copy_from_user fail...\n");
                        retval = -EFAULT;
                        break;
                    }
                    printk("[WOV] pStrModelFilePath:%s\n", pStrModelFilePath);
                    updateSensoryModel(pStrModelFilePath);
                }
            }
            break;
#endif               
                default:  /* redundant, as cmd was checked against MAXNR */
                        MISC_ERR("misc ioctl not supported\n");
                        retval = -EFAULT;
                        goto out;
        }
out:
        up(&pdev->sem);
        return retval;
}

/*
Add for rtd_logger to change log module and level
*/

ssize_t misc_read(struct file * filp, char __user * buf, size_t count,
                  loff_t * f_pos)
{
        if(rtdlog_info == NULL){
            MISC_ERR("[misc_read]rtdlog_info is NULL\n");
            return  -EFAULT;
        }
        //using KERN_EMERG to get information at any time
        MISC_EMERG(" ==========Read information==========\n" );
        MISC_EMERG(" LOGGER Module=0x%x\n", rtdlog_info->rtdlog_module );
        MISC_EMERG(" LOGGER level=%d\n", rtdlog_info->rtdlog_level);
        MISC_EMERG(" LOGGER option=0x%x\n", rtdlog_info->rtdlog_option);
        MISC_EMERG(" ========Read information End=========\n" );
        return 0;
}

ssize_t misc_write(struct file * filp, const char __user * buf, size_t count,
                   loff_t * f_pos)
{
        ssize_t retval = -ENOMEM;
        char cmd[20];
        char value[MODULE_STR_SIZE] = {'\n'};
#ifdef CONFIG_ENABLE_WOV_SUPPORT        
        char path[256] = {'\n'};
#endif
        unsigned int convertValue = 0;
        char str_length = 0;
        int i = 0;
        
        if(rtdlog_info == NULL){
            MISC_ERR("[misc_write]rtdlog_info is NULL\n");
            retval = -EFAULT;
            goto out;
        }

        if (count <= 256) {

                uint8_t data[256] = {0};
                if (copy_from_user(data, buf, count)) {
                        retval = -EFAULT;
                        goto out;
                }
                MISC_ERR("[misc_write]Get string =%s\n", data);

                //sscanf (data,"%s %u",cmd,&convertValue);
                sscanf (data, "%s %s", cmd, value);
                //count  value str size
                for (i = 0; i < MODULE_STR_SIZE; i++) {
                        if ((value[i] >= 'A') && (value[i] <= 'F')) {
                                str_length++;
                        } else if((value[i] >= 'a') && (value[i] <= 'f')) {
                                str_length++;
                        } else if((value[i] >= '0') && (value[i] <= '9')) {
                                str_length++;
                        }
                }

#ifdef CONFIG_ENABLE_WOV_SUPPORT
                if (get_platform() == PLATFORM_KXL)
                {
                    sscanf (data, "%s %s", cmd, path);
                    if(strcmp(cmd, "micom_wov") == 0) {
                        MISC_ERR("[micom_wov] into micom wov test\n");
                        updateSensoryModel(path);
                    } else if(strcmp(cmd, "micom_wovdb_dump") == 0) {
                        dumpSensoryModel();
                    }
                }
#endif
                if(StrToHex(value, str_length, &convertValue) == true) {
                        // MISC_ERR("[misc_write]value  %s convert=0x%x\n",value,convertValue);
                        if(strcmp(cmd, "logmodule") == 0) {
                                // MISC_ERR("[misc_write]convert=%s, 0x%x\n",cmd,convertValue);
                                // console_rtdlog_module = convertValue;
                                rtdlog_info->rtdlog_module = convertValue;
                        } else if(strcmp(cmd, "loglevel") == 0) {
                                //MISC_ERR("[misc_write]convert=%s, 0x%x\n",cmd,convertValue);
                                //console_rtdlog_level = convertValue;
                                rtdlog_info->rtdlog_level = convertValue;
                        } else if(strcmp(cmd, "logoption") == 0) {
                                //MISC_ERR("[misc_write]convert=%s, 0x%x\n",cmd,convertValue);
                                //console_rtdlog_level = convertValue;
                                rtdlog_info->rtdlog_option = convertValue;
                        } else if(strcmp(cmd, "micom_wst") == 0) {
                                if(convertValue < 1 || (convertValue != 0xff && convertValue > 0xa)) {
                                    MISC_ERR("[micom_wst] Please set 1~a value for wait scpu times.\n");
                                }
                                else {
                                    rtd_maskl(0xb8060150, 0x00FFFFFF, convertValue << 24);
                                    if(convertValue == 0xff)
                                        MISC_ERR("[micom_wst] set 0x%x micom will into no dead mode!!\n",  ((rtd_inl(0xb8060150) >> 24) & 0xff));
                                    else                             
                                        MISC_ERR("[micom_wst] micom_wst=0x%x, 0xb8060150:0x%x, wait scpu times:0x%x\n", convertValue, rtd_inl(0xb8060150), ((rtd_inl(0xb8060150) >> 24) & 0xff));                                        
                                }
                        } else {
                            MISC_ERR("[misc_write]NO USING COMMAND\n");
                        }
                }
        }

        retval = count;

out:
        return retval;
}

char* DCMT_module_str (unsigned char id);
void dump_addcmd_status (unsigned int addcmd0, unsigned int addcmd1)
{
        unsigned int module_id = (addcmd0 >> 15) & 0xFF;
        if (addcmd1 & BIT(0)) {
                MISC_ERR("Block Mode access: %s\n", (addcmd1 & BIT(1)) ? "Block write" : "Block read");
                MISC_ERR("module_ID is 0x%x(%s)\n", module_id,DCMT_module_str(module_id));


                MISC_ERR("Sequence starting address is : 0x%x (unit: byte).  (0x%x Unit : 8-Byte )\n", ( (addcmd1 & 0x7FFFFFFF) >> 2) << 3, (addcmd1 & 0x7FFFFFFF)>>2);
                MISC_ERR("Sequence burst length ( Unit : 8-Byte ) is : %d\n", (addcmd1 >> 31) + ((addcmd0 & 0x7F)<<1));

#ifdef DCMT_DC2
                if (((addcmd0 >> 18) & 0xF) < 13) {
                        MISC_ERR("IB_ERROR_TRAP_STATUS[0x%x] %x, IB_ERROR_TRAP_STATUS2[0x%x] %x\n",
                                  (IB_IB_ERROR_TRAP_STATUS_reg + ((addcmd0 >> 18) & 0xF) * 0x100), rtd_inl(IB_IB_ERROR_TRAP_STATUS_reg + ((addcmd0 >> 18) & 0xF) * 0x100),
                                  (IB_IB_ERROR_TRAP_STATUS2_reg + ((addcmd0 >> 18) & 0xF) * 0x100), rtd_inl(IB_IB_ERROR_TRAP_STATUS2_reg + ((addcmd0 >> 18) & 0xF) * 0x100));
                }
#endif
        } else {
                MISC_ERR("Sequence Mode access: %s\n", (addcmd1 & BIT(1)) ? "sequence write" : "sequence read");
                MISC_ERR("module_ID is 0x%x(%s)\n", module_id,DCMT_module_str(module_id));
                MISC_ERR("Sequence starting address is : 0x%x (unit: byte).  (0x%x Unit : 8-Byte )\n", ( (addcmd1 & 0x7FFFFFFF) >> 2) << 3, (addcmd1 & 0x7FFFFFFF)>>2);
                MISC_ERR("Sequence burst length ( Unit : 8-Byte ) is : %d\n", (addcmd1 >> 31) + ((addcmd0 & 0x7F)<<1));

#ifdef DCMT_DC2
                if (((addcmd0 >> 18) & 0xF) < 13) {
                        MISC_ERR("IB_ERROR_TRAP_STATUS[0x%x] %x, IB_ERROR_TRAP_STATUS2[0x%x] %x\n",
                                  (IB_IB_ERROR_TRAP_STATUS_reg + ((addcmd0 >> 18) & 0xF) * 0x100), rtd_inl(IB_IB_ERROR_TRAP_STATUS_reg + ((addcmd0 >> 18) & 0xF) * 0x100),
                                  (IB_IB_ERROR_TRAP_STATUS2_reg + ((addcmd0 >> 18) & 0xF) * 0x100), rtd_inl(IB_IB_ERROR_TRAP_STATUS2_reg + ((addcmd0 >> 18) & 0xF) * 0x100));
                }
#endif
        }
}

static void dump_dc_errcmd(int dc_id, int sys_id, char * msg)
{
        int i;
        unsigned int add_hi_reg = (dcsys_errcmd_regs.addcmd_hi[dc_id][sys_id]);
        unsigned int add_lo_reg = (dcsys_errcmd_regs.addcmd_lo[dc_id][sys_id]);
        unsigned int dc_int_reg = (dcsys_errcmd_regs.dc_int[dc_id][sys_id]);
        unsigned int addcmd[2];
        unsigned int trap_case;
        unsigned int mask = BIT(24) | BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15);
        char* errcmd_str[8] = {
                "seq_sa_odd",
                "seq_bl_zero",
                "seq_bl_odd",
                "blk_cmd",
                "err_disram_switch_int",
                "null",
                "null",
                "null"
        };

        if (rtd_inl(add_hi_reg) & BIT(31)) {
                trap_case = (rtd_inl(add_hi_reg) & 0x7F800000) >> 23;
                addcmd[0] = (rtd_inl(add_hi_reg) & 0x007FFFFF);
                addcmd[1] = rtd_inl(add_lo_reg);

                MISC_ERR("%s\n", msg);

                MISC_ERR("dump regs\n add_hi_reg 0x%08x = 0x%08x\n add_lo_reg 0x%08x = 0x%08x\n dc_int_reg 0x%08x = 0x%08x\n",
                        add_hi_reg, rtd_inl(add_hi_reg),
                        add_lo_reg, rtd_inl(add_lo_reg),
                        dc_int_reg, rtd_inl(dc_int_reg));
                MISC_ERR("DC error command detected: trap case %08x, addcmd %08x-%08x\n", trap_case, addcmd[0], addcmd[1]);
                for(i = 0; i < 8; ++i) {
                        if(trap_case & (1 << i)) {
                                MISC_ERR("trap case : %d (%s)\n", i, errcmd_str[i]);
                        }
                }
                dump_addcmd_status(addcmd[0], addcmd[1]);
                rtd_outl(dc_int_reg, mask | BIT(0));  //write 0 to clear the pending
        }
}

static void dump_dc_errcmd_reg(int dc_id, int sys_id, char * msg)
{
        unsigned int add_hi_reg = (dcsys_errcmd_regs.addcmd_hi[dc_id][sys_id]);
        unsigned int add_lo_reg = (dcsys_errcmd_regs.addcmd_lo[dc_id][sys_id]);
        unsigned int dc_int_reg = (dcsys_errcmd_regs.dc_int[dc_id][sys_id]);

        MISC_ERR("%s\n", msg);
        MISC_ERR("add_hi_reg(0x%08x),val:0x%08x\n",add_hi_reg,rtd_inl(add_hi_reg));
        MISC_ERR("add_lo_reg(0x%08x),val:0x%08x\n",add_lo_reg,rtd_inl(add_lo_reg));
        MISC_ERR("dc_int_reg(0x%08x),val:0x%08x\n",dc_int_reg,rtd_inl(dc_int_reg));
}

static void dump_dc_errcmd_all(void)
{
        dump_dc_errcmd_reg(DC_ID_1, DC_SYS1, "DC1 SYS1 ERRCMD REG");
        dump_dc_errcmd_reg(DC_ID_1, DC_SYS2, "DC1 SYS2 ERRCMD REG");
        dump_dc_errcmd_reg(DC_ID_1, DC_SYS3, "DC1 SYS3 ERRCMD REG");
#ifdef DCMT_DC2
        dump_dc_errcmd_reg(DC_ID_2, DC_SYS1, "DC2 SYS1 ERRCMD REG");
        dump_dc_errcmd_reg(DC_ID_2, DC_SYS2, "DC2 SYS2 ERRCMD REG");
        dump_dc_errcmd_reg(DC_ID_2, DC_SYS3, "DC2 SYS3 ERRCMD REG");
#endif // #ifdef DCMT_DC2
        dump_dc_errcmd(DC_ID_1, DC_SYS1, "DC1 SYS1 ERRCMD");
        dump_dc_errcmd(DC_ID_1, DC_SYS2, "DC1 SYS2 ERRCMD");
        dump_dc_errcmd(DC_ID_1, DC_SYS3, "DC1 SYS3 ERRCMD");
#ifdef DCMT_DC2
        dump_dc_errcmd(DC_ID_2, DC_SYS1, "DC2 SYS1 ERRCMD");
        dump_dc_errcmd(DC_ID_2, DC_SYS2, "DC2 SYS2 ERRCMD");
        dump_dc_errcmd(DC_ID_2, DC_SYS3, "DC2 SYS3 ERRCMD");
#endif // #ifdef DCMT_DC2
}

static void dump_dcsys_debug_status(void)
{
        unsigned int val;
        val = rtd_inl(DC_SYS_DC_debug_status_reg);
        MISC_ERR("DC1 debug status is %08x: \n", val);
        if (val & BIT(25)) {
                MISC_ERR("DC1 BIT 25:  exsram write data fifo underflow\n");
        }
        if (val & BIT(24)) {
                MISC_ERR("DC1 BIT 24:  exsram write data fifo overflow\n");
        }
#if 0 //removed status
        if (val & BIT(23)) {
                MISC_ERR("DC1 BIT 23:  DC_SYS3 write cmd fifo underflow\n");
        }
        if (val & BIT(22)) {
                MISC_ERR("DC1 BIT 22:  DC_SYS2 write cmd fifo underflow\n");
        }
        if (val & BIT(21)) {
                MISC_ERR("DC1 BIT 21:  DC_SYS write cmd fifo underflow\n");
        }
        if (val & BIT(20)) {
                MISC_ERR("DC1 BIT 20:  DC_SYS3 read cmd fifo underflow\n");
        }
        if (val & BIT(19)) {
                MISC_ERR("DC1 BIT 19:  DC_SYS2 read cmd fifo underflow\n");
        }
        if (val & BIT(18)) {
                MISC_ERR("DC1 BIT 18:  DC_SYS read cmd fifo underflow\n");
        }
        if (val & BIT(17)) {
                MISC_ERR("DC1 BIT 17:  DC_SYS3 write cmd fifo overflow\n");
        }
        if (val & BIT(16)) {
                MISC_ERR("DC1 BIT 16:  DC_SYS2 write cmd fifo overflow\n");
        }
        if (val & BIT(15)) {
                MISC_ERR("DC1 BIT 15:  DC_SYS write cmd fifo overflow\n");
        }
        if (val & BIT(14)) {
                MISC_ERR("DC1 BIT 14:  DC_SYS3 read cmd fifo overflow\n");
        }
        if (val & BIT(13)) {
                MISC_ERR("DC1 BIT 13:  DC_SYS2 read cmd fifo overflow\n");
        }
        if (val & BIT(12)) {
                MISC_ERR("DC1 BIT 12:  DC_SYS read cmd fifo overflow\n");
        }
        if (val & BIT(11)) {
                MISC_ERR("DC1 BIT 11:  DC_SYS3 write data fifo underflow\n");
        }
        if (val & BIT(10)) {
                MISC_ERR("DC1 BIT 10:  DC_SYS2 write data fifo underflow\n");
        }
        if (val & BIT(9)) {
                MISC_ERR("DC1 BIT 9:  DC_SYS write data fifo underflow\n");
        }
        if (val & BIT(8)) {
                MISC_ERR("DC1 BIT 8:  DC_SYS3 read data fifo underflow\n");
        }
        if (val & BIT(7)) {
                MISC_ERR("DC1 BIT 7:  DC_SYS2 read data fifo underflow\n");
        }
        if (val & BIT(6)) {
                MISC_ERR("DC1 BIT 6:  DC_SYS read data fifo underflow\n");
        }
        if (val & BIT(5)) {
                MISC_ERR("DC1 BIT 5:  DC_SYS3 write data fifo overflow\n");
        }
        if (val & BIT(4)) {
                MISC_ERR("DC1 BIT 4:  DC_SYS2 write data fifo overflow\n");
        }
        if (val & BIT(3)) {
                MISC_ERR("DC1 BIT 3:  DC_SYS write data fifo overflow\n");
        }
        if (val & BIT(2)) {
                MISC_ERR("DC1 BIT 2:  DC_SYS3 read data fifo overflow\n");
        }
        if (val & BIT(1)) {
                MISC_ERR("DC1 BIT 1:  DC_SYS2 read data fifo overflow\n");
        }
        if (val & BIT(0)) {
                MISC_ERR("DC1 BIT 0:  DC_SYS read data fifo overflow\n");
        }
#endif

#ifdef DCMT_DC2
        val = rtd_inl(DC2_SYS_DC_debug_status_reg);
        MISC_ERR("\n\n\n\n");
        MISC_ERR("DC2 debug status is %08x: \n", val);
        if (val & BIT(25)) {
                MISC_ERR("DC2 BIT 25:  exsram write data fifo underflow\n");
        }
        if (val & BIT(24)) {
                MISC_ERR("DC2 BIT 24:  exsram write data fifo overflow\n");
        }
#if 0 //removed status
        if (val & BIT(23)) {
                MISC_ERR("DC2 BIT 23:  DC_SYS3 write cmd fifo underflow\n");
        }
        if (val & BIT(22)) {
                MISC_ERR("DC2 BIT 22:  DC_SYS2 write cmd fifo underflow\n");
        }
        if (val & BIT(21)) {
                MISC_ERR("DC2 BIT 21:  DC_SYS write cmd fifo underflow\n");
        }
        if (val & BIT(20)) {
                MISC_ERR("DC2 BIT 20:  DC_SYS3 read cmd fifo underflow\n");
        }
        if (val & BIT(19)) {
                MISC_ERR("DC2 BIT 19:  DC_SYS2 read cmd fifo underflow\n");
        }
        if (val & BIT(18)) {
                MISC_ERR("DC2 BIT 18:  DC_SYS read cmd fifo underflow\n");
        }
        if (val & BIT(17)) {
                MISC_ERR("DC2 BIT 17:  DC_SYS3 write cmd fifo overflow\n");
        }
        if (val & BIT(16)) {
                MISC_ERR("DC2 BIT 16:  DC_SYS2 write cmd fifo overflow\n");
        }
        if (val & BIT(15)) {
                MISC_ERR("DC2 BIT 15:  DC_SYS write cmd fifo overflow\n");
        }
        if (val & BIT(14)) {
                MISC_ERR("DC2 BIT 14:  DC_SYS3 read cmd fifo overflow\n");
        }
        if (val & BIT(13)) {
                MISC_ERR("DC2 BIT 13:  DC_SYS2 read cmd fifo overflow\n");
        }
        if (val & BIT(12)) {
                MISC_ERR("DC2 BIT 12:  DC_SYS read cmd fifo overflow\n");
        }
        if (val & BIT(11)) {
                MISC_ERR("DC2 BIT 11:  DC_SYS3 write data fifo underflow\n");
        }
        if (val & BIT(10)) {
                MISC_ERR("DC2 BIT 10:  DC_SYS2 write data fifo underflow\n");
        }
        if (val & BIT(9)) {
                MISC_ERR("DC2 BIT 9:  DC_SYS write data fifo underflow\n");
        }
        if (val & BIT(8)) {
                MISC_ERR("DC2 BIT 8:  DC_SYS3 read data fifo underflow\n");
        }
        if (val & BIT(7)) {
                MISC_ERR("DC2 BIT 7:  DC_SYS2 read data fifo underflow\n");
        }
        if (val & BIT(6)) {
                MISC_ERR("DC2 BIT 6:  DC_SYS read data fifo underflow\n");
        }
        if (val & BIT(5)) {
                MISC_ERR("DC2 BIT 5:  DC_SYS3 write data fifo overflow\n");
        }
        if (val & BIT(4)) {
                MISC_ERR("DC2 BIT 4:  DC_SYS2 write data fifo overflow\n");
        }
        if (val & BIT(3)) {
                MISC_ERR("DC2 BIT 3:  DC_SYS write data fifo overflow\n");
        }
        if (val & BIT(2)) {
                MISC_ERR("DC2 BIT 2:  DC_SYS3 read data fifo overflow\n");
        }
        if (val & BIT(1)) {
                MISC_ERR("DC2 BIT 1:  DC_SYS2 read data fifo overflow\n");
        }
        if (val & BIT(0)) {
                MISC_ERR("DC2 BIT 0:  DC_SYS read data fifo overflow\n");
        }
#endif
#endif // #ifdef DCMT_DC2
}
#ifdef DCMT_DC2
static void dump_ib_errcmd_reg(void)
{
        unsigned int ib_err_status_reg = 0;
        unsigned int ib_err_status = 0;
        unsigned int ib_trap_status_reg = 0;
        unsigned int ib_trap_status = 0;
        unsigned int ib_trap_status2_reg = 0;
        unsigned int ib_trap_status2 = 0;
        int i;

        char * ib_client_str[IB_CLIENTS_NUM] = {
                "c0 (TVSB2)",
                "c1 (TVSB1)",
                "c2 (SB1)",
                "c3 (SB2)",
                "c4 (SB3)",
                "c5 (VE)",
                "c6 (Reserved)",
                "c7 (GDE)",
                "c8 (TVSB5)",
                "c9 (VE2)",
                "c10 (SE2)",
                "c11 (TVSB4)",
                "c12 (MEMC)"
        };

        for(i = 0; i < IB_CLIENTS_NUM; i++)
        {
                ib_err_status_reg = IB_IB_ERROR_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                ib_err_status = rtd_inl(ib_err_status_reg);
                ib_trap_status_reg = IB_IB_ERROR_TRAP_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                ib_trap_status = rtd_inl(ib_trap_status_reg);
                ib_trap_status2_reg = IB_IB_ERROR_TRAP_STATUS2_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                ib_trap_status2 = rtd_inl(ib_trap_status2_reg);

                MISC_ERR("IB CLIENT %d:%s\n",i,ib_client_str[i]);
                MISC_ERR("ib_err_status(0x%08x),val:0x%08x\n",ib_err_status_reg,ib_err_status);
                MISC_ERR("ib_trap_status(0x%08x),val:0x%08x\n",ib_trap_status_reg,ib_trap_status);
                MISC_ERR("ib_trap_status2(0x%08x),val:0x%08x\n",ib_trap_status2_reg,ib_trap_status2);
        }
}

static void dump_ib_errcmd(void)
{
        char * ib_client_str[IB_CLIENTS_NUM] = {
                "c0 (TVSB2)",
                "c1 (TVSB1)",
                "c2 (SB1)",
                "c3 (SB2)",
                "c4 (SB3)",
                "c5 (VE)",
                "c6 (Reserved)",
                "c7 (GPU)",
                "c8 (TVSB5)",
                "c9 (VE2)",
                "c10 (SE2)",
                "c11 (TVSB4)",
                "c12 (MEMC)"
        };

        char * ib_trapcase_str[IB_TRAP_CASE_NUMBER] = {
                "null ",
                "null",
                "null",
                "null",
                "err_seq_bl_odd_int ",
                "err_seq_bl_zero_int",
                "err_seq_sa_odd_int",
                "null",
                "null",
                "null",
                "null"
        };

        char * ib_err_status_str[IB_ERR_STATUS_NUMBER] = {
                "Seq_blen_zero",
                "Seq_blen_odd",
                "Seq_saddr_odd",
                "Rinfo_overflow",
                "Rinfo_underflow",
                "Winfo_overflow",
                "Winfo_underflow",
                "Wdone_overflow",
                "Wdone_underflow",
                "Cmd_dc1_overflow",
                "Cmd_dc1_underflow",
                "Cmd_dc2_overflow",
                "Cmd_dc2_underflow",
                "Rdata_dc1_overflow",
                "Rdata_dc1_underflow",
                "Rdout_dc1_underflow",
                "Rdata_dc2_overflow",
                "Rdout_dc2_underflow",
                "Rdout_dc2_underflow",
                "wdata_dc1_overflow",
                "wdata_dc1_underflow",
                "wdout_dc1_underflow",
                "wdata_dc2_overflow",
                "wdata_dc2_underflow",
                "wdout_dc2_underflow",
                "region_error(addcmd cross region)",
                "null",
                "null",
                "dc1_end_in_disram",
                "dc1_st_in_disram",
                "is_blk_cmd",
                "null"
        };

        int i = 0, j = 0;
        for(i = 0; i < IB_CLIENTS_NUM; i++) {
                unsigned int ib_err_status = rtd_inl(IB_IB_ERROR_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i);
                unsigned int ib_trap_address = IB_IB_ERROR_TRAP_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                unsigned int ib_trap_status = rtd_inl(ib_trap_address);

                MISC_ERR("--------------------------------------------------\n");
                MISC_ERR("Client[%s] IB command status: IB_ERROR_STATUS(0x%x)\n", ib_client_str[i], ib_err_status);

                /*detect IB error trap happen or not*/
                if(((ib_trap_status & IB_IB_ERROR_TRAP_STATUS_trap_case_mask) >> IB_IB_ERROR_TRAP_STATUS_trap_case_shift) != 0x0) {
                        MISC_ERR("IB_ERROR_TRAP: IDX(0x%x),Trap_Case(0x%x),CMD_HI(0x%x), CMD_LO(0x%x)\n",
                                  i, ib_trap_status, ib_trap_status & IB_IB_ERROR_TRAP_STATUS_addcmd_hi_mask, rtd_inl(ib_trap_address + 0x4));

                        for(j = 0; j < IB_TRAP_CASE_NUMBER; ++j) {
                                if(ib_trap_status & (1 << (20 + j))) {
                                        MISC_ERR("IB CLIENT : %s -> TRAP CASE : %s\n", ib_client_str[i], ib_trapcase_str[j]);
                                }
                        }

                        MISC_ERR("IB CLIENT : %s -> IB_ERROR_STATUS(0x%x)\n", ib_client_str[i], ib_err_status);
                        for(j = 0; j < IB_ERR_STATUS_NUMBER; ++j) {
                                if(ib_err_status & (1 << j)) {
                                        MISC_ERR("IB CLIENT : %s -> ERR STATUS : %s\n", ib_client_str[i], ib_err_status_str[j]);
                                }
                        }

                        //write 1 to clear error trap status
                        rtd_outl(ib_trap_address, IB_IB_ERROR_TRAP_STATUS_trap_status_clr_mask | BIT(0));

                        //write 0 to reset to default for next time latch
                        rtd_outl(ib_trap_address, IB_IB_ERROR_TRAP_STATUS_trap_status_clr_mask & (~BIT(0)));
                }
        }
}
#endif // #ifdef DCMT_DC2
static __maybe_unused void dump_vo_regs(void)
{
        /*dump VO realted register for checking*/
        rtd_setbits(VODMA_VODMA_V1CHROMA_FMT_reg, BIT(12));
        MISC_ERR("Dump VODMA Setting(1) 0xB8005030(%x), 0xb80050DC(%x),0xb8005000(%x)\n",
                  rtd_inl(VODMA_VODMA_V1CHROMA_FMT_reg),
                  rtd_inl(VODMA_VODMA_DMA_OPTION_reg), rtd_inl(VODMA_VODMA_V1_DCFG_reg));

        rtd_clearbits(VODMA_VODMA_V1CHROMA_FMT_reg, BIT(12));
        MISC_ERR("Dump VODMA Setting(0) 0xB8005030(%x), 0xb80050DC(%x),0xb8005000(%x)\n",
                  rtd_inl(VODMA_VODMA_V1CHROMA_FMT_reg),
                  rtd_inl(VODMA_VODMA_DMA_OPTION_reg), rtd_inl(VODMA_VODMA_V1_DCFG_reg));

        MISC_ERR("Dump 0xB8005100(%x), 0xB8005104(%x),0xB800500C(%x), 0xB800511C(%x)\n",
                  rtd_inl(0xB8005100), rtd_inl(0xB8005104), rtd_inl(0xB800500C), rtd_inl(0xB800511C));

        MISC_ERR("Dump 0xB8005120(%x), 0xB8005134(%x), 0xB8005144(%x), 0xB8005018(%x)\n",
                  rtd_inl(0xB8005120), rtd_inl(0xB8005134), rtd_inl(0xB8005144), rtd_inl(0xB8005018));

        MISC_ERR("Dump 0xB800501c(%x), 0xB8005020(%x), 0xB8005024(%x), 0xB80050E4(%x)\n",
                  rtd_inl(0xB800501c), rtd_inl(0xB8005020), rtd_inl(0xB8005024), rtd_inl(0xB80050E4));
}

static void dump_mc_error_status(unsigned int reg, char * msg)
{
        unsigned int val;
        MISC_ERR("[%s]reg val:0x%08x\n", msg, reg);
        if(reg & BIT(30)) {
                MISC_ERR("[%s] ASYNC FIFO for wdata from DC_SYS overflows\n", msg);
        }
        if(reg & BIT(29)) {
                MISC_ERR("[%s] ASYNC FIFO for command from DC_SYS overflows\n", msg);
        }
        if(reg & BIT(27)) {
                MISC_ERR("[%s] Tag with wdata ack from MC mismatch with the expected qfifo cmd tag from DC_SYS\n", msg);
        }
        if(reg & BIT(26)) {
                MISC_ERR("[%s] Tag with rdata valid from MC mismatch with the expected qfifo cmd tag from DC_SYS\n", msg);
        }
        if(reg & BIT(25)) {
                MISC_ERR("[%s] Burst length of write command from DC_SYS mismatch with the one from MC\n", msg);
        }
        if(reg & BIT(24)) {
                MISC_ERR("[%s] Burst length of read command from DC_SYS mismatch with the one from MC\n", msg);
        }

        val = reg & 0x3F0;
        val = val >> 4;
        MISC_ERR("[%s] The maximum number of read command ever reached in MC_FIFO is 0x%08x\n", msg, val);

        val = reg & 0x7;
        MISC_ERR("[%s] The bank number of the maximum number of write command ever reached in MC_FIFO is 0x%08x\n", msg, val);
}

static void dump_mc_error_status_all(void)
{
        unsigned int mc1_fifo_dbg_cmd0_val = rtd_inl(DC1_MC_MCFIFO_DBG_CMD0_reg);
        unsigned int mc1_fifo_dbg_cmd1_val = rtd_inl(DC1_MC_MCFIFO_DBG_CMD1_reg);
        unsigned int mc1_fifo_dbg_cmd2_val = rtd_inl(DC1_MC_MCFIFO_DBG_CMD2_reg);
        unsigned int mc1_fifo_dbg_cmd3_val = rtd_inl(DC1_MC_MCFIFO_DBG_CMD3_reg);
#ifdef DCMT_DC2
        unsigned int mc2_fifo_dbg_cmd0_val = rtd_inl(DC2_MC_MCFIFO_DBG_CMD0_reg);
        unsigned int mc2_fifo_dbg_cmd1_val = rtd_inl(DC2_MC_MCFIFO_DBG_CMD1_reg);
        unsigned int mc2_fifo_dbg_cmd2_val = rtd_inl(DC2_MC_MCFIFO_DBG_CMD2_reg);
        unsigned int mc2_fifo_dbg_cmd3_val = rtd_inl(DC2_MC_MCFIFO_DBG_CMD3_reg);
#endif // #ifdef DCMT_DC2
        dump_mc_error_status(mc1_fifo_dbg_cmd0_val, "MC1 SCPU");
        dump_mc_error_status(mc1_fifo_dbg_cmd1_val, "MC1 DCSYS1");
        dump_mc_error_status(mc1_fifo_dbg_cmd2_val, "MC1 DCSYS2");
        dump_mc_error_status(mc1_fifo_dbg_cmd3_val, "MC1 DCSYS3");
#ifdef DCMT_DC2
        dump_mc_error_status(mc2_fifo_dbg_cmd0_val, "MC2 SCPU");
        dump_mc_error_status(mc2_fifo_dbg_cmd1_val, "MC2 DCSYS1");
        dump_mc_error_status(mc2_fifo_dbg_cmd2_val, "MC2 DCSYS2");
        dump_mc_error_status(mc2_fifo_dbg_cmd3_val, "MC2 DCSYS3");
#endif // #ifdef DCMT_DC2
}

void dump_tvsb1_error_status(unsigned int val, char * msg)
{
        unsigned int tmp = 0;
        char * Error_Client_ID[8] =
        {
                "null",
                "Video Decoder",
                "Audio1",
                "Audio2",
                "VBI(W)",
                "TVE(R)-VD",
                "TVE(R)-VBI",
                "audio hdmi(R)"
        };

        if(val & BIT(9))
        {
                MISC_ERR("%s - Error in read fifo\n", msg);
        }

        if(val & BIT(8))
        {
                MISC_ERR("%s - Error in write fifo\n", msg);
        }

        tmp = val & 0xf;
        if(tmp)
        {
                if(tmp < 8)
                {
                        MISC_ERR("%s - Error req in %s\n", msg, Error_Client_ID[tmp]);
                }
        }
}

void dump_tvsb2_error_status(unsigned int val, char * msg)
{
        unsigned int tmp;
        char * Error_zero_length_id_str[16] = {
                "none error",
                "De-Interlace(W)",
                "De-Interlace(R)",
                "Main Capture(W)",
                "Main Display(R)",
                "Sub Capture (W)",
                "Sub Display (R)",
                "VO1_Y (R)",
                "VO1_C (C)",
                "VO2_Y (R)",
                "VO2_C (R)",
                "I3DDMA (W)",
                "DE_XC(W)",
                "DE_XC(R)",
                "SNR(R)",
                "UNKNOWN"
        };

        char * Error_req_id[16] = {
                "none error",
                "De-Interlace(W)",
                "De-Interlace(R)",
                "Main Capture(W)",
                "Main Display(R)",
                "Sub Capture (W)",
                "Sub Display (R)",
                "VO1_Y (R)",
                "VO1_C (R)",
                "VO2_Y (R)",
                "VO2_C (R)",
                "I3DDMA (W)",
                "DE_XC(W)",
                "DE_XC(R)",
                "SNR(R)",
                "UNKNOWN"
        };

        MISC_ERR("[%s]reg val:0x%08x\n", msg, val);

        if(val & 0xF000) {
                tmp = (val & 0xF000) >> 12;
                MISC_ERR("%s - Error_zero_length : %s\n", msg, Error_zero_length_id_str[tmp]);
        }

        if(val & BIT(9)) {
                MISC_ERR("%s - Error_read_full\n", msg);
        }

        if(val & BIT(8)) {
                MISC_ERR("%s - Error_write_full\n", msg);
        }

        if(val & 0xF) {
                tmp = val & 0xF;
                MISC_ERR("%s - Error_req : %s\n", msg, Error_req_id[tmp]);
        }
}

void dump_tvsb4_error_status(unsigned int val, char * msg)
{
        unsigned int tmp;
        char * Error_id_str[10] = {
                "null",
                "OD(W)",
                "OD(R)",
                "DC2H(W)",
                "SUBTITLE(R)",
                "OSD1(R)",
                "OSD2(R)",
                "OSD3(R)",
                "Demura(R)",
                "dmato3dlut(R)"
        };

        MISC_ERR("[%s]reg val:0x%08x\n", msg, val);

        if(val & 0xF000) {
                tmp = (val & 0xF000) >> 12;
                MISC_ERR("%s - Error_zero_length : %s\n", msg, Error_id_str[tmp]);
        }

        if(val & BIT(9)) {
                MISC_ERR("%s - Error_read_full\n", msg);
        }

        if(val & BIT(8)) {
                MISC_ERR("%s - Error_write_full\n", msg);
        }

        if(val & 0xF) {
                tmp = val & 0xF;
                MISC_ERR("%s - Error_req : %s\n", msg, Error_id_str[tmp]);
        }
}

void dump_tvsb5_error_status(unsigned int val, char * msg)
{
        unsigned int tmp;
        char * Error_id_str[4] = {
                "null",
                "demod(W/R)",
                "demoddbg(W)",
                "USB3(W/R)",
        };

        MISC_ERR("[%s]reg val:0x%08x\n", msg, val);

        if(val & 0xF000) {
                tmp = (val & 0xF000) >> 12;
                MISC_ERR("%s - Error_zero_length : %s\n", msg, Error_id_str[tmp]);
        }

        if(val & BIT(10)) {
                MISC_ERR("%s - Error_wdone_fifo\n", msg);
        }

        if(val & BIT(9)) {
                MISC_ERR("%s - Error_read_full\n", msg);
        }

        if(val & BIT(8)) {
                MISC_ERR("%s - Error_write_full\n", msg);
        }

        if(val & 0xF) {
                tmp = val & 0xF;
                MISC_ERR("%s - Error_req : %s\n", msg, Error_id_str[tmp]);
        }
}


void dump_128bit_bridge_error_status_all(void)
{
        unsigned int sb1_dcu1_error_status = rtd_inl(TVSB1_TV_SB1_DCU1_error_status_reg);
        unsigned int sb2_dcu1_error_status = rtd_inl(TVSB2_TV_SB2_DCU1_error_status_reg);
        //unsigned int sb3_dcu1_error_status = rtd_inl(0xB801C5FC);
        unsigned int sb4_dcu1_error_status = rtd_inl(TVSB4_TV_SB4_DCU1_error_status_reg);
        unsigned int sb5_dcu1_error_status = rtd_inl(TVSB5_TV_SB5_DCU1_error_status_reg);

	dump_tvsb1_error_status(sb1_dcu1_error_status, "128bit_sb1_error");
        dump_tvsb2_error_status(sb2_dcu1_error_status, "128bit_sb2_error");
        //dump_128bit_bridge_error_status(sb3_dcu1_error_status, "128bit_sb3_error");
        dump_tvsb4_error_status(sb4_dcu1_error_status, "128bit_sb4_error");
        dump_tvsb5_error_status(sb5_dcu1_error_status, "128bit_sb5_error");
}

void dump_64bit_bridge_error_status(unsigned int val, char * msg)
{
        unsigned int tmp;

        MISC_ERR("[%s]reg val:0x%08x\n", msg, val);

        if(val & BIT(30)) {
                MISC_ERR("%s : err_bl_zero\n", msg);
        }

        if(val & BIT(29)) {
                MISC_ERR("%s : err_blk_wxh_odd\n", msg);
        }

        tmp = val & 0x0F000000;
        tmp = tmp >> 24;
        MISC_ERR("%s : max_wcmd_in_fifo is %d\n", msg, tmp);

        tmp = val & 0x00F00000;
        tmp = tmp >> 20;
        MISC_ERR("%s : max_rcmd_in_fifo is %d\n", msg, tmp);

        tmp = val & 0x000FC000;
        tmp = tmp >> 14;
        MISC_ERR("%s : max_wait_wdone is %d\n", msg, tmp);

        tmp = val & 0x00003F80;
        tmp = tmp >> 7;
        MISC_ERR("%s : max_wdata_in_fifo is %d\n", msg, tmp);

        tmp = val & 0x0000007F;
        MISC_ERR("%s : max_rdata_in_fifo is %d\n", msg, tmp);
}

void dump_64bit_bridge_error_status_all(void)
{
        unsigned int dc_64_err_status_aio_status = rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_aio_reg);
        unsigned int dc_64_err_status_sb1_status = rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb1_reg);
        unsigned int dc_64_err_status_sb3_status = rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb3_reg);

        dump_64bit_bridge_error_status(dc_64_err_status_aio_status, "dc_64_err_status_aio");
        dump_64bit_bridge_error_status(dc_64_err_status_sb1_status, "dc_64_err_status_sb1");
        dump_64bit_bridge_error_status(dc_64_err_status_sb3_status, "dc_64_err_status_sb3");
}

int is_dcsys_errcmd(void)
{
        int i, j;
        for(i = 0; i < DC_NUMBER; ++i) {
                for(j = 0; j < DC_SYS_NUMBER; ++j) {
                        if(rtd_inl(dcsys_errcmd_regs.addcmd_hi[i][j]) & BIT(31)) {
                                return 1;
                        }
                }
        }

        return 0;
}

#ifdef DCMT_DC2
int is_ib_err_status(void)
{
        unsigned int ib_err_status_reg = 0;
        unsigned int ib_err_status = 0;
        //unsigned int ib_trap_status_reg = 0;
        //unsigned int ib_trap_status = 0;
        int i;


        for(i = 0; i < IB_CLIENTS_NUM; i++)
        {
                ib_err_status_reg = IB_IB_ERROR_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                ib_err_status = rtd_inl(ib_err_status_reg);
                ib_err_status &= ~(1<<31);//ignore bit 31(clear status bit)
                if(ib_err_status)
                {
                        return 1;
                }
        }

        return 0;
}

int is_ib_err_trap(void)
{
        unsigned int ib_trap_status_reg = 0;
        unsigned int ib_trap_status = 0;
        int i;


        for(i = 0; i < IB_CLIENTS_NUM; i++)
        {
                ib_trap_status_reg = IB_IB_ERROR_TRAP_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                ib_trap_status = rtd_inl(ib_trap_status_reg);
                if(ib_trap_status & 0x7ff00000) //check bit 30..20 for ib error status bit
                {
                        return 1;
                }
        }

        return 0;
}
#endif // #ifdef DCMT_DC2

int is_dcsys_debug_status(void)
{
        unsigned int tmp = 0;

        tmp |= rtd_inl(DC_SYS_DC_debug_status_reg);
#ifdef DCMT_DC2
        tmp |= rtd_inl(DC2_SYS_DC_debug_status_reg);
#endif // #ifdef DCMT_DC2

        if(tmp)
        {
                return 1;
        }

        return 0;
}

int is_mc_err_status(void)
{
        unsigned int tmp = 0;

        tmp |= rtd_inl(DC1_MC_MCFIFO_DBG_CMD0_reg);
        tmp |= rtd_inl(DC1_MC_MCFIFO_DBG_CMD1_reg);
        tmp |= rtd_inl(DC1_MC_MCFIFO_DBG_CMD2_reg);
        tmp |= rtd_inl(DC1_MC_MCFIFO_DBG_CMD3_reg);
#ifdef DCMT_DC2
        tmp |= rtd_inl(DC2_MC_MCFIFO_DBG_CMD0_reg);
        tmp |= rtd_inl(DC2_MC_MCFIFO_DBG_CMD1_reg);
        tmp |= rtd_inl(DC2_MC_MCFIFO_DBG_CMD2_reg);
        tmp |= rtd_inl(DC2_MC_MCFIFO_DBG_CMD3_reg);
#endif // #ifdef DCMT_DC2

        if(tmp)
        {
                return 1;
        }

        return 0;
}

int is_128bit_bridge_error_status(void)
{
        unsigned int tmp = 0;

        tmp |= rtd_inl(TVSB1_TV_SB1_DCU1_error_status_reg);
        tmp |= rtd_inl(TVSB2_TV_SB2_DCU1_error_status_reg);
        tmp |= rtd_inl(TVSB4_TV_SB4_DCU1_error_status_reg);
        tmp |= rtd_inl(TVSB5_TV_SB5_DCU1_error_status_reg);

        if(tmp)
        {
                return 1;
        }

        return 0;
}

int is_64bit_bridge_error_status(void)
{
        unsigned int tmp = 0;

        tmp |= rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_aio_reg);
        tmp |= rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb1_reg);
        tmp |= rtd_inl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb3_reg);

        if(tmp)
        {
                return 1;
        }

        return 0;
}

int is_misc_intr(void)
{
        int ret = 0;
        if(is_dcsys_errcmd())
        {
                MISC_ERR("detected dcsys errcmd!\n");
                ret =  1;
        }

#if 0 //there is no intr for debug status
        if(is_dcsys_debug_status())
        {
                MISC_ERR("detected dcsys error status!\n");
                ret =  1;
        }
#endif

#ifdef DCMT_DC2
        if(is_ib_err_status())
        {
                MISC_ERR("detected ib error status!\n");
                ret =  1;
        }
#endif // #ifdef DCMT_DC2
        /*
        if(is_ib_err_trap())
        {
                MISC_ERR("detected ib error trap!\n");
                ret =  1;
        }

        if(is_mc_err_status())
        {
                MISC_ERR("detected mc error status!\n");
                ret =  1;
        }
        if(is_128bit_bridge_error_status())
        {
                MISC_ERR("detected 128-bridge error status!\n");
                ret =  1;
        }
        if(is_64bit_bridge_error_status())
        {
                MISC_ERR("detected 64-bridge error status!\n");
                ret =  1;
        }
        */

        return ret;
}


static void clear_dc_errcmd_reg(int dc_id, int sys_id)
{
        rtd_outl(dcsys_errcmd_regs.addcmd_hi[dc_id][sys_id], 0);
        rtd_outl(dcsys_errcmd_regs.addcmd_lo[dc_id][sys_id], 0);
        rtd_outl(dcsys_errcmd_regs.dc_int[dc_id][sys_id], BIT(23)|BIT(22)|BIT(21)|BIT(20));
}

static void clear_dc_errcmd_reg_all(void)
{
        clear_dc_errcmd_reg(DC_ID_1, DC_SYS1);
        clear_dc_errcmd_reg(DC_ID_1, DC_SYS2);
        clear_dc_errcmd_reg(DC_ID_1, DC_SYS3);
#ifdef DCMT_DC2
        clear_dc_errcmd_reg(DC_ID_2, DC_SYS1);
        clear_dc_errcmd_reg(DC_ID_2, DC_SYS2);
        clear_dc_errcmd_reg(DC_ID_2, DC_SYS3);
#endif // #ifdef DCMT_DC2
}

static void clear_dcsys_debug_status(void)
{
        rtd_outl(DC_SYS_DC_debug_status_reg, 0);
#ifdef DCMT_DC2
        rtd_outl(DC2_SYS_DC_debug_status_reg, 0);
#endif // #ifdef DCMT_DC2
}
#ifdef DCMT_DC2
static void clear_ib_errcmd(void)
{
        unsigned int ib_err_status_reg = 0;
        unsigned int ib_trap_status_reg = 0;
        unsigned int ib_trap_status2_reg = 0;
        int i;

        for(i = 0; i < IB_CLIENTS_NUM; i++)
        {
                ib_err_status_reg = IB_IB_ERROR_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                rtd_outl(ib_err_status_reg, 0);
                ib_trap_status_reg = IB_IB_ERROR_TRAP_STATUS_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                rtd_outl(ib_trap_status_reg, 0);
                ib_trap_status2_reg = IB_IB_ERROR_TRAP_STATUS2_reg + IB_CLIENTS_REG_ADDRESS_OFFSET * i;
                rtd_outl(ib_trap_status2_reg, 0);
        }
}
#endif // #ifdef DCMT_DC2
static void clear_mc_err_status(void)
{
        rtd_outl(DC1_MC_MCFIFO_DBG_CMD0_reg, BIT(31));
        rtd_outl(DC1_MC_MCFIFO_DBG_CMD1_reg, BIT(31));
        rtd_outl(DC1_MC_MCFIFO_DBG_CMD2_reg, BIT(31));
        rtd_outl(DC1_MC_MCFIFO_DBG_CMD3_reg, BIT(31));
#ifdef DCMT_DC2
        rtd_outl(DC2_MC_MCFIFO_DBG_CMD0_reg, BIT(31));
        rtd_outl(DC2_MC_MCFIFO_DBG_CMD1_reg, BIT(31));
        rtd_outl(DC2_MC_MCFIFO_DBG_CMD2_reg, BIT(31));
        rtd_outl(DC2_MC_MCFIFO_DBG_CMD3_reg, BIT(31));
#endif // #ifdef DCMT_DC2
}

static void clear_128bit_bridge_error_status(void)
{
        rtd_outl(TVSB1_TV_SB1_DCU1_error_status_reg, BIT(31));
        rtd_outl(TVSB2_TV_SB2_DCU1_error_status_reg, BIT(31));
        rtd_outl(TVSB4_TV_SB4_DCU1_error_status_reg, BIT(31));
        rtd_outl(TVSB5_TV_SB5_DCU1_error_status_reg, BIT(31));
}

static void clear_64bit_bridge_error_status(void)
{
        rtd_outl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_aio_reg, BIT(31));
        rtd_outl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb1_reg, BIT(31));
        rtd_outl(DC_SYS_64BIT_WRAPPER_DC_64_err_status_sb3_reg, BIT(31));
}

void dump_misc_error_stauts(void)
{
        is_misc_intr();

        dump_dc_errcmd_all();

        dump_dcsys_debug_status();
        console_flush_on_panic();
#ifdef DCMT_DC2
        dump_ib_errcmd_reg();
        dump_ib_errcmd();
#endif // #ifdef DCMT_DC2
        console_flush_on_panic();

        dump_mc_error_status_all();

        dump_128bit_bridge_error_status_all();

        dump_64bit_bridge_error_status_all();
        console_flush_on_panic();
}
EXPORT_SYMBOL(dump_misc_error_stauts);

irqreturn_t misc_isr (int irq, void *dev_id)
{
        unsigned int old_loglevel;

        if(!is_misc_intr())
                return IRQ_NONE;

        old_loglevel = console_loglevel;
        if(console_loglevel < 3)
        {
                console_loglevel = 5;
        }

        //dump_vo_regs();
        MISC_ERR("MISC ISR Dump Stacks!!!!\n");

        dump_stacks();
        console_flush_on_panic();
        is_misc_intr();

        dump_dc_errcmd_all();

        dump_dcsys_debug_status();
        console_flush_on_panic();
#ifdef DCMT_DC2
        dump_ib_errcmd_reg();
        dump_ib_errcmd();
#endif // #ifdef DCMT_DC2
        console_flush_on_panic();

        dump_mc_error_status_all();

        dump_128bit_bridge_error_status_all();

        dump_64bit_bridge_error_status_all();
        console_flush_on_panic();

#if 1 //panic mode
        panic("%s detected error!",__FUNCTION__);
#else //non panic mode
        dump_stacks();
#endif
        /*clear DC_SYS intr pendding : because DC_SYS interrupt sending when any error status set to '1'
          so, if we clear every error status bits, there will be no interrupt any more.
        */
        clear_dc_errcmd_reg_all();
        clear_dcsys_debug_status();
#ifdef DCMT_DC2
        clear_ib_errcmd();
#endif // #ifdef DCMT_DC2
        clear_mc_err_status();
        clear_128bit_bridge_error_status();
        clear_64bit_bridge_error_status();

        console_loglevel = old_loglevel;

        return IRQ_HANDLED;
}

//                          Add for new device PM driver.
#ifdef CONFIG_PM
static int misc_pm_suspend(
        struct platform_device  *dev,
        pm_message_t state
)
{
        return 0;
}

static int misc_pm_resume(
        struct platform_device  *dev
)
{
        MISC_INFO("misc resume\n");

        /* enable error commands detection of DCSYS1 and DCSYS2 */
        rtd_outl(DC_SYS_DC_int_enable_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_int_enable_SYS2_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_int_enable_SYS3_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_EC_CTRL_reg, BIT(3) | BIT(2) | BIT(1) | BIT(0));
#ifdef DCMT_DC2
        rtd_outl(DC2_SYS_DC_int_enable_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_int_enable_SYS2_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_int_enable_SYS3_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_EC_CTRL_reg, BIT(3) | BIT(2) | BIT(1) | BIT(0));
#endif // #ifdef DCMT_DC2
        rtd_outl(SYS_REG_INT_CTRL_SCPU_reg, BIT(11) | BIT(0));   //enable global interrupt

		rtk_ddr_debug_resume();

        return 0;
}

static struct platform_device *misc_platform_devs;

static struct platform_driver misc_device_driver = {
        .suspend    = misc_pm_suspend,
        .resume     = misc_pm_resume,
        .driver = {
                .name       = "sys-misc",
                .bus        = &platform_bus_type,
        } ,
} ;
#endif // CONFIG_PM

struct file_operations misc_fops = {
        .owner              = THIS_MODULE,
        .unlocked_ioctl     = misc_ioctl,
#if defined(CONFIG_COMPAT) && defined(CONFIG_ARM64)
        .compat_ioctl = misc_ioctl,
#endif
        .open               = misc_open,
        .release            = misc_release,
        .read           = misc_read,
        .write = misc_write,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void misc_cleanup_module(void)
{
        dev_t dev = MKDEV(misc_major, misc_minor);

        MISC_INFO( "misc clean module\n");

        /* Get rid of our char dev entries */
        if (misc_drv_dev) {
                device_destroy(misc_class, dev);
                cdev_del(&misc_drv_dev->cdev);
                kfree(misc_drv_dev);
#ifdef CONFIG_PM
                platform_device_unregister(misc_platform_devs);
#endif
        }

        unregister_chrdev_region(dev, 1);
}

static void init_dc_errcmd_regs(void)
{
        dcsys_errcmd_regs.addcmd_hi[0][0] = DC_SYS_DC_EC_ADDCMD_HI_reg;
        dcsys_errcmd_regs.addcmd_hi[0][1] = DC_SYS_DC_EC_ADDCMD_HI_SYS2_reg;
        dcsys_errcmd_regs.addcmd_hi[0][2] = DC_SYS_DC_EC_ADDCMD_HI_SYS3_reg;
#ifdef DCMT_DC2
        dcsys_errcmd_regs.addcmd_hi[1][0] = DC2_SYS_DC_EC_ADDCMD_HI_reg;
        dcsys_errcmd_regs.addcmd_hi[1][1] = DC2_SYS_DC_EC_ADDCMD_HI_SYS2_reg;
        dcsys_errcmd_regs.addcmd_hi[1][2] = DC2_SYS_DC_EC_ADDCMD_HI_SYS3_reg;
#endif // #ifdef DCMT_DC2

        dcsys_errcmd_regs.addcmd_lo[0][0] = DC_SYS_DC_EC_ADDCMD_LO_reg;
        dcsys_errcmd_regs.addcmd_lo[0][1] = DC_SYS_DC_EC_ADDCMD_LO_SYS2_reg;
        dcsys_errcmd_regs.addcmd_lo[0][2] = DC_SYS_DC_EC_ADDCMD_LO_SYS3_reg;
#ifdef DCMT_DC2
        dcsys_errcmd_regs.addcmd_lo[1][0] = DC2_SYS_DC_EC_ADDCMD_LO_reg;
        dcsys_errcmd_regs.addcmd_lo[1][1] = DC2_SYS_DC_EC_ADDCMD_LO_SYS2_reg;
        dcsys_errcmd_regs.addcmd_lo[1][2] = DC2_SYS_DC_EC_ADDCMD_LO_SYS3_reg;
#endif // #ifdef DCMT_DC2

        dcsys_errcmd_regs.dc_int[0][0] = DC_SYS_DC_int_status_reg;
        dcsys_errcmd_regs.dc_int[0][1] = DC_SYS_DC_int_status_SYS2_reg;
        dcsys_errcmd_regs.dc_int[0][2] = DC_SYS_DC_int_status_SYS3_reg;
#ifdef DCMT_DC2
        dcsys_errcmd_regs.dc_int[1][0] = DC2_SYS_DC_int_status_reg;
        dcsys_errcmd_regs.dc_int[1][1] = DC2_SYS_DC_int_status_SYS2_reg;
        dcsys_errcmd_regs.dc_int[1][2] = DC2_SYS_DC_int_status_SYS3_reg;
#endif // #ifdef DCMT_DC2
}

int misc_init_module(void)
{
        int result;
        dev_t dev = 0;

        sema_init (&micom_sem, 1);

        /*
         * Get a range of minor numbers to work with, asking for a dynamic
         * major unless directed otherwise at load time.
         */

        MISC_INFO( " ***************** misc init module ********************* \n");
        if (misc_major) {
                dev = MKDEV(misc_major, misc_minor);
                result = register_chrdev_region(dev, 1, "sys-misc");
        } else {
                result = alloc_chrdev_region(&dev, misc_minor, 1, "sys-misc");
                misc_major = MAJOR(dev);
        }
        if (result < 0) {
                MISC_ERR("misc: can't get major %d\n", misc_major);
                return result;
        }

        MISC_INFO( "misc init module major number = %d\n", misc_major);

        misc_class = class_create(THIS_MODULE, "sys-misc");
        if (IS_ERR(misc_class))
                return PTR_ERR(misc_class);

        device_create(misc_class, NULL, dev, NULL, "sys-misc");

        /*
         * allocate the devices
         */
        misc_drv_dev = kmalloc(sizeof(struct misc_dev), GFP_KERNEL);
        if (!misc_drv_dev) {
                device_destroy(misc_class, dev);
                result = -ENOMEM;
                goto fail;  /* Make this more graceful */
        }
        memset(misc_drv_dev, 0, sizeof(struct misc_dev));

        //initialize device structure
        sema_init(&misc_drv_dev->sem, 1);
        cdev_init(&misc_drv_dev->cdev, &misc_fops);
        misc_drv_dev->cdev.owner = THIS_MODULE;
        misc_drv_dev->cdev.ops = &misc_fops;
        result = cdev_add(&misc_drv_dev->cdev, dev, 1);
        /* Fail gracefully if need be */
        if (result) {
                device_destroy(misc_class, dev);
                kfree(misc_drv_dev);
                MISC_ERR("Error %d adding cdev misc", result);
                goto fail;
        }

#ifdef CONFIG_PM
        misc_platform_devs = platform_device_register_simple("sys-misc", -1, NULL, 0);

        if(platform_driver_register(&misc_device_driver) != 0) {
                device_destroy(misc_class, dev);
                cdev_del(&misc_drv_dev->cdev);
                kfree(misc_drv_dev);
                misc_platform_devs = NULL;
                goto fail;  /* Make this more graceful */
        }
#endif  //CONFIG_PM

        return 0; /* succeed */

fail:
        return result;
}

module_init(misc_init_module);
module_exit(misc_cleanup_module);

#include <mach/rtk_platform.h>
#define DDR_DEBUG_RAM_DUMMY_ADDR	0xb8008EB0 // Dora gave
#define DDR_DEBUG_RAM_INVALID		0xbeef2379

unsigned int ddr_debug_ram_addr = 0;

static void rtk_ddr_debug_fill_dummy(unsigned int addr)
{
	rtd_outl(DDR_DEBUG_RAM_DUMMY_ADDR, addr);
}

static void rtk_ddr_debug_init_dummy(void)
{
	unsigned long addr = 0 ;
	unsigned long size = 0 ;

	size = carvedout_buf_query(CARVEDOUT_K_OS, (void **)&addr);

	if (size > 0) {
		// Use addr and size here
		ddr_debug_ram_addr = addr;
		rtk_ddr_debug_fill_dummy(ddr_debug_ram_addr);
	} else {
		rtk_ddr_debug_fill_dummy(DDR_DEBUG_RAM_INVALID);
		pr_err("No Security OS memory is reserved\n");
	}
}

static void rtk_ddr_debug_resume(void)
{
	rtk_ddr_debug_fill_dummy(ddr_debug_ram_addr);
}

static int __init rtk_ddr_debug_init(void)
{
	rtk_ddr_debug_init_dummy();

	return 0;
}
late_initcall(rtk_ddr_debug_init);

static int misc_probe(struct platform_device *pdev)
{
        int ret = 0;
        int virq = -1;
        struct device_node *np = pdev->dev.of_node;

        if (!np)
        {
                MISC_ERR("[%s]there is no device node\n",__func__);
                return -ENODEV;
        }

        virq = irq_of_parse_and_map(np, 0);
        if(!virq){
                MISC_ERR("[%s] map misc vitual_irq failed\n",__func__);
                of_node_put(np);
                return -ENODEV;
        }

        if(request_irq(virq,
                       misc_isr,
                       IRQF_SHARED,
                       "DC_ERR",
                       misc_isr)) {
                MISC_ERR("MISC: cannot register IRQ %d\n", IRQ_DCSYS);
                return -1;
        }

        /* enable error commands detection of DCSYS1 and DCSYS2 */
        rtd_outl(DC_SYS_DC_int_enable_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_int_enable_SYS2_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_int_enable_SYS3_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC_SYS_DC_EC_CTRL_reg, BIT(3) | BIT(2) | BIT(1) | BIT(0));
#ifdef DCMT_DC2
        rtd_outl(DC2_SYS_DC_int_enable_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_int_enable_SYS2_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_int_enable_SYS3_reg, BIT(22) | BIT(21) | BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16) | BIT(15) | BIT(0));
        rtd_outl(DC2_SYS_DC_EC_CTRL_reg, BIT(3) | BIT(2) | BIT(1) | BIT(0));
#endif // #ifdef DCMT_DC2
        rtd_outl(SYS_REG_INT_CTRL_SCPU_reg, BIT(11) | BIT(0));    //enable global interrupt

        init_dc_errcmd_regs();

        of_node_get(np);

        return ret;
}

static int misc_probe_remove(struct platform_device *pdev)
{
        return 0;
}

static const struct of_device_id misc_of_match[] =
{
        {
                .compatible = "realtek,misc",
        },
        {},
};

static struct platform_driver misc_driver =
{
        .probe          = misc_probe,
        .driver = {
                .name = "misc",
                .of_match_table = misc_of_match,
        },
        .remove = misc_probe_remove,
};
MODULE_DEVICE_TABLE(of, misc_of_match);
module_platform_driver(misc_driver);

