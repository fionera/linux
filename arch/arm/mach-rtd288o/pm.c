/* Realtek Power Management Routines
 *
 * Copyright (c) 2013 Realtek Corp.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License.
 *
 * History:
 *
 * 2013-03-11:  XXX Power Management for Magellan.
 */
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/slab.h>
#ifdef CONFIG_CMA_RTK_ALLOCATOR
#include <linux/pageremap.h>
#endif
#ifdef CONFIG_QUICK_HIBERNATION
#include <linux/sysfs.h>
#include <linux/reboot.h>
#endif
#include <rbus/stb_reg.h>
#include <asm/system_misc.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_RTK_KDRV_AVCPU
#include <rtk_kdriver/rtk_avcpu_export.h>
#endif
#include <mach/io.h>
#include <mach/common.h>
#ifdef CONFIG_RTK_KDRV_EMCU
#include <rtk_kdriver/rtk_emcu_export.h>
#endif
#ifdef CONFIG_ENABLE_WOV_SUPPORT
#include <rtk_kdriver/rtk_crt.h>
#include <rtk_kdriver/rtk_wov.h>
//#include <../../rtk_kdriver/emcu/rtk_kdv_emcu.h>
//extern int powerMgr_get_wakeup_source(unsigned int* row, unsigned int* status);
#endif

#define KERNEL_RSV_MEM_MAX_ENTRY_COUNT  4
#define KERNEL_RSV_MEM_SIZE             32*1024*1024

#define KCPU_STD_MAGICTAG               0x6b637075      //'kcpu'
#define KCPU_STD_MAGICTAG_ADDR          0x16000000
#define KCPU_STD_RESUME_ADDR            0x16000004

#if 0
/* Memory information of kernel reserved for bootcode use. */
typedef struct {
        unsigned int dwMagicNumber;     // magic number must be 0x2379beef
        unsigned int dwEntryCount;      // available entries (ex. 2)

        struct {
                unsigned int address;
                unsigned int size;
        } entry[KERNEL_RSV_MEM_MAX_ENTRY_COUNT];
} kernel_rsv_mem_info_t;
#endif

#ifdef CONFIG_SUSPEND
extern int suspend_devices_and_enter(suspend_state_t state);
#endif

#ifdef CONFIG_HAVE_ARM_SCU
extern void platform_smp_prepare_cpus(unsigned int max_cpus);
#endif
#ifdef CONFIG_CACHE_L2X0
extern void rtk_l2_cache_init(void);
#endif
extern int rtk_pm_load_8051(int flag);

static bool need_load_av = false;
unsigned int record_suspend_state = 0;
//static kernel_rsv_mem_info_t *rsv_mem_info = NULL;
#if (defined CONFIG_REALTEK_SECURE) && (!defined CONFIG_OPTEE)
void * secure_uncached_virtual =  NULL;
unsigned long secure_phy_address_start =  0;
unsigned long secure_phy_address_size =  0;
#endif

static int rtk_pm_valid(suspend_state_t state)
{
        return  (state == PM_SUSPEND_EMCU_ON)      ||
                (state == PM_SUSPEND_MEM);
        //(state == PM_SUSPEND_STANDBY)    ||
        //(state == PM_SUSPEND_ON);
}

#ifdef LOAD_UBOOT
#define EMMC_BLOCK_SIZE       (PAGE_SIZE / 8)
#define UBOOT_START_BLOCK     0x11c
#define UBOOT_CODE_SIZE       0x6a828
extern int mmc_fast_read(unsigned int, unsigned int, unsigned char *);
#endif

#ifdef LOAD_UBOOT
static int rtk_load_uboot(void)
{
        int blk;

        blk = UBOOT_CODE_SIZE / EMMC_BLOCK_SIZE;
        if (UBOOT_CODE_SIZE % EMMC_BLOCK_SIZE)
                blk++;
        printk("<< %s >>: Ready to load uboot %x @ %x...\n", __FUNCTION__, blk * EMMC_BLOCK_SIZE, UBOOT_START_BLOCK);
        if (mmc_fast_read(UBOOT_START_BLOCK, blk, PAGE_OFFSET + 0x20000) != blk)
                return 1;
        else
                return 0;
}
#endif

static int notrace rtk_enter_suspend(unsigned long param)
{
	unsigned int count = 0;
	rtd_outl(STB_WDOG_DATA2_reg, virt_to_phys(cpu_resume));

#if defined (CONFIG_OPTEE)
        printk("<< %s >>: Ready to load 8051 (%lu)...\n", __FUNCTION__, param);
        rtk_pm_load_8051(param);
//      asm volatile ("1: b 1b \n");  //debug of EMCU
        printk("<< %s >>: platform_cpu_die() have CPU itself died. (%lu)...\n", __FUNCTION__, param);

#ifdef CONFIG_OPTEE
        platform_cpu_die(0);  //cpu 0, will call level-2 suspend to secure world.
#endif

#else

	// to compatible to previouse impelment, spin here and wait EMCU turn off.
	asm ("1: b 1b \n");
#endif

//      soft_restart(virt_to_phys(cpu_resume));
end:
        /* we should not reach here */
        panic("KCPU STR flow not finish(%d)\n", count);
        return 0;
}

#if 0
static void alloc_uboot_buffer(void)
{
        int i = 0;

        BUG_ON(rsv_mem_info);
        rsv_mem_info = kmalloc(sizeof(kernel_rsv_mem_info_t), GFP_KERNEL);
        if (rsv_mem_info) {
                rsv_mem_info->dwMagicNumber = 0x2379beef;
                rsv_mem_info->dwEntryCount = 0;
                while (i < KERNEL_RSV_MEM_MAX_ENTRY_COUNT) {
                        void *virt = dvr_malloc(KERNEL_RSV_MEM_SIZE);
                        if (!virt)
                                break;
                        rsv_mem_info->entry[i].address = (unsigned int)dvr_to_phys(virt);
                        rsv_mem_info->entry[i].size = KERNEL_RSV_MEM_SIZE;
                        rsv_mem_info->dwEntryCount = ++i;
                }
                rtd_outl(STB_WDOG_DATA3_reg, virt_to_phys(rsv_mem_info));
        } else {
                rtd_outl(STB_WDOG_DATA3_reg, 0);
        }
}

static void free_uboot_buffer(void)
{
        int i = 0;
        if (rsv_mem_info) {
                while (i < rsv_mem_info->dwEntryCount) {
                        dvr_free(phys_to_virt(rsv_mem_info->entry[i].address));
                        i++;
                }
                kfree(rsv_mem_info);
                rsv_mem_info = 0;
                rtd_outl(STB_WDOG_DATA3_reg, 0);
        }
}
#endif


#ifdef CONFIG_ENABLE_WOV_SUPPORT

unsigned long long wov_start_block;
unsigned long long wov_size_block;
unsigned long long wovdb_start_block;
unsigned long long wovdb_size_block;

char *wov_save_buf_ptr=NULL;
static int wov_save_buf_size=0;
spinlock_t wov_lock;

int wov_load_uboot_db(void)
{
    int wovdb_index;
    unsigned int wovdb_par_size;
    const char *wovdb_name="wovdb";

    int size,blk,i,count=0;
    char *tmp_ptr,*buf_ptr;
    unsigned char *buf_ptr2;

    /* CLK Enable for WOV */
    //CRT_CLK_OnOff(WOV, CLK_ON, NULL);

    wovdb_index = lgemmc_get_partnum(wovdb_name);
    printk("[WOV] %s-%d, wovdb_name:%s, wovdb_index:%d, wovdb_par_size:%d\n", __func__, __LINE__, wovdb_name, wovdb_index, lgemmc_get_partsize(wovdb_index));
    
    wovdb_size_block = lgemmc_get_partsize(wovdb_index)/EMMC_BLOCK_SIZE;
    if((wovdb_size_block % 8) != 0) {
        wovdb_size_block += 8-(wovdb_size_block % 8);
    }
    wovdb_start_block =  lgemmc_get_partoffset(wovdb_index)>>9;

    printk("[WOV] %s-%d, wovdb_size_block:%lld, wovdb_start_block:%llx\n", __func__, __LINE__, wovdb_size_block, wovdb_start_block);

    //map to virtual address
    buf_ptr=dvr_remap_uncached_memory(WOV_DB_DDR_PHYSICAL_START_ADDR, wovdb_size_block*EMMC_BLOCK_SIZE, __builtin_return_address(0));
  //  buf_ptr=dvr_remap_uncached_memory(WOV_DB_DDR_PHYSICAL_START_ADDR, 1*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    printk("[WOV] %s:load wov db from EMMC(%llx @ %llx) to DDR(%x)\n", __FUNCTION__, wovdb_size_block, wovdb_start_block, buf_ptr);

    //temp buffer size use 1MB in case kmalloc fail
    size=32*1024;
    blk=size/EMMC_BLOCK_SIZE;
    tmp_ptr = kmalloc(size,GFP_DCU1);
    if(tmp_ptr==NULL){
    	printk("[WOV] kmalloc fail %d Byte\n",size);
    	return 1;
    }

	//determine how many times to read from emmc
	count=wovdb_size_block/blk;
	if(wovdb_size_block%blk!=0)
        count++;
	printk("[WOV] count=%d\n",count);

	//read from emmc
	for(i=0;i<count;i++){
    	memset((void *)tmp_ptr,0,size);
		//printk("ddr=%x,%x,%x,%x\n",tmp_ptr[0],tmp_ptr[1],tmp_ptr[2],tmp_ptr[3]);

		if (mmc_fast_read(wovdb_start_block+i*blk, blk, (unsigned char *)tmp_ptr) != blk){
			printk("[WOV] load fail %d\n",i);
			break;
		}
		else{
			//printk("[WOV] load success %d\n",i);
			//printk("[WOV] buf_ptr= %x,tmp_ptr=%x,size=%d\n",buf_ptr+i*size,tmp_ptr,size);
            memcpy(buf_ptr+i*size,tmp_ptr,size);
            //printk("[WOV] ddr=%x,%x,%x,%x\n",tmp_ptr[0],tmp_ptr[1],tmp_ptr[2],tmp_ptr[3]);
		}
	}
    
    dvr_unmap_memory((void *)buf_ptr,wovdb_size_block*EMMC_BLOCK_SIZE);
    kfree(tmp_ptr);


	//setup wov ddr dump register, map to virtual address
    buf_ptr2=(unsigned char*)dvr_remap_uncached_memory(0x47c00000, 8*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    memset(buf_ptr2, 0, 0x20);
    *(unsigned long*)(buf_ptr2+0x1C) = WOV_DB_DDR_PHYSICAL_START_ADDR; //wovdb start addr (WOV_POLLING_START_LEN + 4)
    *(unsigned long*)(buf_ptr2+0x20) = lgemmc_get_partsize(wovdb_index); //wovdb size (WOV_MODEL_DB_ADDR_reg+0x4)
    //*(unsigned long*)(buf_ptr2+0x20) = lgemmc_get_filesize(wovdb_index); //wovdb size (WOV_MODEL_DB_ADDR_reg+0x4)
    printk("[WOV] *(buf_ptr2+0x1C):%x, *(buf_ptr2+0x20):%x\n", *(unsigned long*)(buf_ptr2+0x1C), *(unsigned long*)(buf_ptr2+0x20));
    dvr_unmap_memory((void *)buf_ptr2,8*EMMC_BLOCK_SIZE);

    buf_ptr2=(unsigned char*)dvr_remap_uncached_memory(0x47c00000, 8*EMMC_BLOCK_SIZE, __builtin_return_address(0));
    printk("[WOV] *(buf_ptr2+0x1C):%x, *(buf_ptr2+0x20):%x\n", *(unsigned long*)(buf_ptr2+0x1C), *(unsigned long*)(buf_ptr2+0x20));
    dvr_unmap_memory((void *)buf_ptr2,8*EMMC_BLOCK_SIZE);

    printk("[WOV] %s-%d, lgemmc_get_partname:%s, lgemmc_get_partsize:%d, lgemmc_get_filesize:%d, lgemmc_get_partoffset:%llx\n", __func__, __LINE__, lgemmc_get_partname(wovdb_index), lgemmc_get_partsize(wovdb_index), lgemmc_get_filesize(wovdb_index), lgemmc_get_partoffset(wovdb_index));

    return 0;
}

int wov_save_file(void)
{
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0,ret;
    unsigned long long offset=0;
    char path[30];
	int i = 0;
	//printk("wov_save_file 1\n");
	unsigned char *buf_ptr;
	unsigned char * MODEL_TYPE;
	//map to virtual address
	buf_ptr=dvr_remap_uncached_memory(HOT_WORDS_START_reg-0x4, 4096, __builtin_return_address(0)); 
	//printk("buf_ptr:0x%x\n", *(unsigned char*)(buf_ptr));
	/*for(i = 0; i < 5; i++)
	{
		printk("i:%d, data:0x%x\n", i, *(unsigned char*)(buf_ptr+i));
	}*/

	MODEL_TYPE = (unsigned char*)(buf_ptr+0xC);
	//printk("wov_save_file MODEL_TYPE_0:%s,MODEL_TYPE_1:%s,MODEL_TYPE_2:%s\n",MODEL_TYPE[0],MODEL_TYPE[1],MODEL_TYPE[2]);
	if((MODEL_TYPE[0] == 'e' || MODEL_TYPE[0] =='E') &&
	(MODEL_TYPE[1] == 'f' || MODEL_TYPE[1] == 'F') &&
	(MODEL_TYPE[2] == 't' || MODEL_TYPE[2] == 'T'))	
		sprintf(path,"tmp/wov_EFT.raw");
	else
		sprintf(path,"tmp/wov_FT.raw");

	//open file
    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, O_RDWR | O_CREAT, 0644);
    if(IS_ERR(filp)) {
            err = PTR_ERR(filp);
            set_fs(oldfs);
            printk("wov:open fail\n");
            return 1;
    }

    //write to file
		ret = vfs_write(filp, wov_save_buf_ptr, wov_save_buf_size, &offset);
		printk("wov:write to %s(ret=%d)\n",path,ret);

    //close file
		filp_close(filp, NULL);

    set_fs(oldfs);
		return 0;
}

int wov_backup_ddr(void)
{
		char *ddr_vptr;

		spin_lock(&wov_lock);
		//copy from WOV_DUMP_DATA_DDR_START_ADDR to another DDR
		//wov_save_buf_size=rtd_inl(0xb8060660);
		wov_save_buf_size=WOV_AP_DDR_DEFAULT_RECORD_LENGTH;
		printk("%s:record data len=%d\n",__FUNCTION__,wov_save_buf_size);
		if(wov_save_buf_ptr==NULL){
				wov_save_buf_ptr=vmalloc(wov_save_buf_size);
				if(NULL==wov_save_buf_ptr)
					printk("%s:vmalloc fail(size=%d)\n",__FUNCTION__,wov_save_buf_size);
		}
		printk("%s:wov_save_buf_ptr=%x\n",__FUNCTION__,wov_save_buf_ptr);

		//copy
    ddr_vptr=dvr_remap_uncached_memory(WOV_DUMP_4CH_PREPROCESS_DATA_ADDR, wov_save_buf_size, __builtin_return_address(0));
		memcpy(wov_save_buf_ptr,ddr_vptr,wov_save_buf_size);
		dvr_unmap_memory((void *)ddr_vptr,wov_save_buf_size);

		spin_unlock(&wov_lock);

		//save to /tmp/wov.ddr
		wov_save_file();
		//wov_release_ddr();
		return 1;
}
//op 1 for get resules, 0 for clear resules
// retuen 1 wakeup from wov, 0 others 
int is_wakeup_from_wov(int op)
{
#define WKSOR_WOV 0xAABB
	int resume_from_wov = 0; //not get wakeup source

	if(op == 1) //get
	{
		//printk("1. %s:rtd_inl(0xb8060164)=%x\n",__FUNCTION__,rtd_inl(0xb8060164));
		//check wov POWER on status	
		if((rtd_inl(0xb8060164) >> 16) == WKSOR_WOV)
		{	
			resume_from_wov = 1;//wakeup from wov
			rtd_outl(0xb8060164, 0);
		}
		return resume_from_wov;
	}
	else //clear
		return resume_from_wov;
}

int wov_release_ddr(void)
{
		spin_lock(&wov_lock);
		if(wov_save_buf_ptr!=NULL){
			vfree(wov_save_buf_ptr);
			wov_save_buf_ptr=NULL;
		}
		printk("%s:release ddr\n",__FUNCTION__);
		wov_save_buf_size=0;

		spin_unlock(&wov_lock);
		return 0;
}

int wov_get_ddr_ptr(char **ptr)
{
		int ret=0;
		if( ptr==0){
			printk("%s:invalid address\n",__FUNCTION__);
			return 0;
		}
		spin_lock(&wov_lock);
		*ptr=wov_save_buf_ptr;
		ret=wov_save_buf_size;
		spin_unlock(&wov_lock);
		return ret;
}

int wov_load_uboot_ap(void)
{
    int wovap_index;
    unsigned int wov_par_size;        
    const char *wovap_name="wovbin";

    int size,blk,i,count=0;
    char *tmp_ptr,*buf_ptr;

	/* CLK Enable for WOV */
    CRT_CLK_OnOff(WOV, CLK_ON, NULL);

    wovap_index = lgemmc_get_partnum(wovap_name);


    printk("[WOV] %s-%d, wovap_name:%s, wovap_index:%d, wovap_par_size:%d\n", __func__, __LINE__, wovap_name, wovap_index, lgemmc_get_partsize(wovap_index));

    wov_size_block = lgemmc_get_partsize(wovap_index)/EMMC_BLOCK_SIZE;
    if((wov_size_block % 8) != 0) {
        wov_size_block += 8-(wov_size_block % 8);
    }
    wov_start_block =  lgemmc_get_partoffset(wovap_index)>>9;
    
    printk("[WOV] %s-%d, wov_size_block:%lld, wov_start_block:%llx\n", __func__, __LINE__, wov_size_block, wov_start_block);

    //map to virtual address
   buf_ptr=dvr_remap_uncached_memory(WOV_AP_DDR_PHYSICAL_START_ADDR, wov_size_block*EMMC_BLOCK_SIZE, __builtin_return_address(0));    
   printk("[WOV] %s:load wov ap from EMMC(%llx @ %llx) to DDR(%x)\n", __FUNCTION__, wov_size_block, wov_start_block, buf_ptr);


    //temp buffer size use 1MB in case kmalloc fail
    size=64*1024;
    blk=size/EMMC_BLOCK_SIZE;
    tmp_ptr = kmalloc(size,GFP_DCU1);
    if(tmp_ptr==NULL){
    	printk("[WOV] kmalloc fail %d Byte\n",size);
    	return 1;
    }

	//determine how many times to read from emmc
	count=wov_size_block/blk;
	if(wov_size_block%blk!=0)
        count++;
	printk("[WOV] count=%d\n",count);

	//read from emmc
	for(i=0;i<count;i++){
        memset((void *)tmp_ptr,0,size);

        //printk("ddr=%x,%x,%x,%x\n",tmp_ptr[0],tmp_ptr[1],tmp_ptr[2],tmp_ptr[3]);

		if (mmc_fast_read(wov_start_block+i*blk, blk, (unsigned char *)tmp_ptr) != blk){
			printk("[WOV] load fail %d\n",i);
			break;
		}
		else{
			//printk("[WOV] load success %d\n",i);
			//printk("[WOV] buf_ptr= %x,tmp_ptr=%x,size=%d\n",buf_ptr+i*size,tmp_ptr,size);
            memcpy(buf_ptr+i*size,tmp_ptr,size);
          	//printk("[WOV] ddr=%x,%x,%x,%x\n",tmp_ptr[0],tmp_ptr[1],tmp_ptr[2],tmp_ptr[3]);
		}
	}

    dvr_unmap_memory((void *)buf_ptr,wov_size_block*EMMC_BLOCK_SIZE);
    kfree(tmp_ptr);

    //setup wov ddr dump register
    rtd_outl(0xb8060658,WOV_DUMP_DATA_DDR_START_ADDR);
    //printk("b8060658=%x\n",rtd_inl(0xb8060658));

    printk("[WOV] %s-%d, lgemmc_get_partname:%s, lgemmc_get_partsize:%d, lgemmc_get_filesize:%d, lgemmc_get_partoffset:%llx\n", __func__, __LINE__, lgemmc_get_partname(wovap_index), lgemmc_get_partsize(wovap_index), lgemmc_get_filesize(wovap_index), lgemmc_get_partoffset(wovap_index));

    return 0;
}

#endif

/*
 *      rtk_pm_begin - Do preliminary suspend work.
 *      @state:         suspend state we're entering.
 *
 */
static int rtk_pm_begin(suspend_state_t state)
{
        printk("%s\n", __func__);

        need_load_av = false;
        switch (state) {
                case PM_SUSPEND_MEM:
#ifndef CONFIG_OPTEE
                        /*Notify KCPU STR suspend flow begin*/
                        printk(KERN_NOTICE "Notify KCPU STR suspend flow\n");
                        rtd_outl(STB_WDOG_DATA6_reg, 0x23792379);
#endif

#ifdef CONFIG_RTK_KDRV_AVCPU
                        /*Notify SCPU STR suspend flow begin*/
                        *((volatile int *)phys_to_virt(SCPU_STR_STATUS_FLAG_ADDR)) = SCPU_STR_STATUS_SUSPEND_BEGIN;
                        dmac_flush_range(phys_to_virt(SCPU_STR_STATUS_FLAG_ADDR), phys_to_virt(SCPU_STR_STATUS_FLAG_ADDR + 0x4));
                        outer_flush_range(SCPU_STR_STATUS_FLAG_ADDR, SCPU_STR_STATUS_FLAG_ADDR + 0x4);
#endif

#ifdef CONFIG_ENABLE_WOV_SUPPORT
        //Enable WOV support
        if (get_platform() == PLATFORM_KXL)
        {
            printk(KERN_NOTICE "[WOV] load wovap/wovdb to ddr\n");
            wov_load_uboot_ap();
            wov_load_uboot_db();
        }
#endif

                        //alloc_uboot_buffer();
#ifdef LOAD_UBOOT
                        if (rtk_load_uboot())
                                return -EINVAL;
                        else
                                return 0;
#else
                        return 0;
#endif
                case PM_SUSPEND_ON:
                        //alloc_uboot_buffer();
                        return 0;
                case PM_SUSPEND_STANDBY:
                case PM_SUSPEND_EMCU_ON:
#ifdef CONFIG_RTK_KDRV_EMCU
                        rtk_pm_load_8051(SUSPEND_NORMAL);
#if defined (CONFIG_OPTEE)
	    /** fixme,
	     * do not understand what situation here,
	     * but to complement a infinite loop because of removal inside rtk_pm_load_8051().
	     */
	    while(1) {
		    cpu_relax();
	    }
#endif //#if defined (CONFIG_OPTEE)
#endif
                        break;
                default:
                        return  -EINVAL;
        }
        return 0;
}

#if 1 //duplicate arm timer resume flow, the resume_cs_timestamp() have been moved to rtk_hpt_resume()
#ifdef CONFIG_ARM_ARCH_TIMER
extern void resume_cs_timestamp(void);
#endif
#endif

static int rtk_pm_enter(suspend_state_t state)
{
        printk("%s\n", __func__);

        switch (state) {
                case PM_SUSPEND_MEM:
#ifdef CONFIG_RTK_KDRV_EMCU
                        if (!cpu_suspend(SUSPEND_RAM, rtk_enter_suspend)) {
#ifdef CONFIG_HAVE_ARM_SCU
                                //TODO
                                //platform_smp_prepare_cpus(NR_CPUS);
#endif
#ifdef CONFIG_ARM_ARCH_TIMER
                                resume_cs_timestamp();
#endif

#ifdef CONFIG_CACHE_L2X0
                                rtk_l2_cache_init();
#endif
//                      need_load_av = true;
                        } else
                                printk("<< %s >>: Error in cpu_suspend...\n", __FUNCTION__);
                        //printk("<< %s >>: Ready to resume from suspend 1...\n", __FUNCTION__);
#endif // CONFIG_RTK_KDRV_EMCU
                        return 0;
                case PM_SUSPEND_ON:
#ifdef CONFIG_RTK_KDRV_EMCU
                        if (!cpu_suspend(SUSPEND_WAKEUP, rtk_enter_suspend)) {
#ifdef CONFIG_HAVE_ARM_SCU
                                //TODO
                                //      platform_smp_prepare_cpus(NR_CPUS);
#endif

#if 0 //duplicate arm timer resume flow, the resume_cs_timestamp() have been moved to rtk_hpt_resume()
#ifdef CONFIG_ARM_ARCH_TIMER
                                resume_cs_timestamp();
#endif
#endif

#ifdef CONFIG_CACHE_L2X0
                                rtk_l2_cache_init();
#endif
//                      need_load_av = true;
                        } else
                                printk("<< %s >>: Error in cpu_suspend...\n", __FUNCTION__);
                        printk("<< %s >>: Ready to resume from suspend 2...\n", __FUNCTION__);
#endif // CONFIG_RTK_KDRV_EMCU
                        return 0;
                case PM_SUSPEND_STANDBY:
                case PM_SUSPEND_EMCU_ON:

                        break;
                default:
                        return  -EINVAL;
                        break;
        }
        return 0;
}


static void rtk_pm_end(void)
{
        //free_uboot_buffer();

        unsigned long timeout = jiffies + HZ * 2;
        printk("%s\n", __func__);

#ifndef CONFIG_OPTEE
        while(time_before(jiffies, timeout)) {
                if(rtd_inl(STB_WDOG_DATA6_reg) == 0xcafecafe)
                        break;
        }

        /*Check KCPU STR resume flow finish*/
        if(rtd_inl(STB_WDOG_DATA6_reg) != 0xcafecafe)
                panic("KCPU STR resume Fail\n");
        else
                printk(KERN_NOTICE "KCPU STR resume finish\n");
#endif

#ifdef CONFIG_RTK_KDRV_AVCPU
        if (need_load_av) {
                resetav_lock(1);
                load_av_image(NULL, 0, NULL, 0, NULL, 0);
                resetav_unlock(1);
        }
#endif
}

static struct platform_suspend_ops rtk_pm_ops = {
        .valid = rtk_pm_valid,
        .begin = rtk_pm_begin,
        .enter = rtk_pm_enter,
        .end   = rtk_pm_end,
};

//#ifdef CONFIG_QUICK_HIBERNATION
extern int in_suspend;

static int rtk_hibernation_begin(void)
{
#if (defined CONFIG_REALTEK_SECURE) && (!defined CONFIG_OPTEE)
        /*Notify KCPU STD suspend flow begin*/
        printk(KERN_NOTICE "Notify KCPU STD suspend flow\n");
        rtd_outl(STB_WDOG_DATA6_reg, 0x23792379);

        secure_phy_address_size = carvedout_buf_query(CARVEDOUT_K_OS, &secure_phy_address_start);
        secure_uncached_virtual = dvr_remap_uncached_memory(secure_phy_address_start, secure_phy_address_size, __builtin_return_address(0));
        if(!secure_uncached_virtual)
        {
                printk(KERN_NOTICE "%s: mapping secure area(0x%x - 0x%x) fail\n", __func__, secure_phy_address_start, secure_phy_address_start+secure_phy_address_size);
                return -1;
        }
 #endif
        return 0;
}

static void rtk_hibernation_end(void)
{
#if (defined CONFIG_REALTEK_SECURE) && (!defined CONFIG_OPTEE)
        unsigned long timeout = jiffies + HZ * 2;
        while(time_before(jiffies, timeout)) {
                if(rtd_inl(STB_WDOG_DATA6_reg) == 0xcafecafe)
                        break;
        }

        /*Check KCPU STD resume flow finish*/
        if(rtd_inl(STB_WDOG_DATA6_reg) != 0xcafecafe)
                panic("KCPU STD resume Fail\n");
        else
                printk(KERN_NOTICE "KCPU STD resume finish\n");

        if(secure_uncached_virtual)
        {
                dvr_unmap_memory(secure_uncached_virtual, secure_phy_address_size);
                secure_uncached_virtual = NULL;
        }
        secure_phy_address_start =  0;
        secure_phy_address_size =  0;
#endif
}


static int rtk_hibernation_pre_snapshot(void)
{
        int ret = 0;
#if (defined CONFIG_REALTEK_SECURE) && (!defined CONFIG_OPTEE)
        unsigned int kcpu_resume_address = 0;
        unsigned long timeout = jiffies + HZ * 2;
        while(time_before(jiffies, timeout)) {
                if(rtd_inl(STB_WDOG_DATA6_reg) != 0x23792379)
                        break;
        }

        /*Check KCPU STD suspend flow finish*/
        kcpu_resume_address = rtd_inl(STB_WDOG_DATA6_reg);

        if(kcpu_resume_address == 0x23792379) {
                panic("KCPU make image suspend Fail\n");
                ret = 1;
        } else {
                *((volatile int *)secure_uncached_virtual) = KCPU_STD_MAGICTAG;
                *((volatile int *)(secure_uncached_virtual +4)) = kcpu_resume_address;
                printk(KERN_NOTICE "KCPU STD resume point(%x)\n", kcpu_resume_address);
        }
 #endif

#ifdef CONFIG_LG_SNAPSHOT_BOOT
	extern unsigned int snapshot_enable;
	extern bool reserve_boot_memory;
	extern bool reserve_boot_memory;
	if (snapshot_enable) {
		reserve_boot_memory = 1;
	}

#endif
        return ret;
}

extern int rtdlog_resume(void);
static void rtk_hibernation_finish(void)
{
#if (defined CONFIG_REALTEK_SECURE) && (!defined CONFIG_OPTEE)
        extern int in_suspend;
        if(in_suspend) { //making image resume

                /*Notify KCPU STD make image resume*/
                printk(KERN_NOTICE "Notify KCPU STD make image resume\n");
                rtd_outl(STB_WDOG_DATA6_reg, 0x23992399);
        }
 #endif
        /* get rtdlog level setting from bootcode */
        rtdlog_resume();
}

static int rtk_hibernation_prepare(void)
{
        return 0;
}

static int rtk_hibernation_enter(void)
{
        return 0;
}

static void rtk_hibernation_leave(void)
{

}

static int rtk_hibernation_pre_restore(void)
{
        return 0;
}

static void rtk_hibernation_restore_cleanup(void)
{

}

static void rtk_hibernation_recover(void)
{

}

static struct platform_hibernation_ops rtk_hibernation_ops = {
        .begin           = rtk_hibernation_begin,
        .end             = rtk_hibernation_end,
        .pre_snapshot    = rtk_hibernation_pre_snapshot,
        .finish          = rtk_hibernation_finish,
        .prepare         = rtk_hibernation_prepare,
        .enter           = rtk_hibernation_enter,
        .leave           = rtk_hibernation_leave,
        .pre_restore     = rtk_hibernation_pre_restore,
        .restore_cleanup = rtk_hibernation_restore_cleanup,
        .recover         = rtk_hibernation_recover,
};
//#endif

//#ifdef CONFIG_QUICK_HIBERNATION
extern struct kobject   *realtek_boards_kobj;
static struct kobject   *std_kobj;

static ssize_t  std_show
(struct kobject *kobj,
 struct kobj_attribute *attr,
 char *buf)
{
        return  sprintf(buf, "1\n");
}
static ssize_t  std_attr_store
(struct kobject *kobj,
 struct attribute *attr,
 const char *buf,
 size_t len)
{
        return  0;
}

static ssize_t  std_attr_show
(struct kobject *kobj,
 struct attribute *attr,
 char *buf)
{
        return  std_show(kobj, (struct kobj_attribute *)attr, buf);
}

static const struct sysfs_ops   std_sysfs_ops = {
        .show = std_attr_show,
        .store = std_attr_store,
};

static struct kobj_attribute    std_kobj_attrs[] = {
        __ATTR_RO(std),                         //std_show()
        __ATTR_NULL,
};

static struct kobj_type std_ktype = {
        .sysfs_ops = &std_sysfs_ops,
        //.default_attrs = ;
};

#define SYSFS_ERR_CHK(err)      \
do {                                            \
        if(err) {                               \
                printk("ERR!\n");       \
                goto    RETN;           \
        }                                               \
} while(0);

static int      std_kobj_init
(void)
{
        int     error;

        std_kobj = kobject_create();
        //kobject_init(std_kobj, &std_ktype);
        std_kobj->ktype = &std_ktype;
        error = kobject_set_name(std_kobj, "STD");
        SYSFS_ERR_CHK(error);
        error = kobject_add(std_kobj, realtek_boards_kobj, "STD");
        SYSFS_ERR_CHK(error);

        if((error = sysfs_create_file(std_kobj, (struct attribute*)&std_kobj_attrs[0])))
                kobject_put(std_kobj);
RETN:
        return  error;
}
//#endif        //CONFIG_QUICK_HIBERNATION

static int suspend_status_pm_event(struct notifier_block *this,
                                   unsigned long event, void *ptr)
{
        switch (event) {
                case PM_SUSPEND_PREPARE:
                        record_suspend_state = PM_SUSPEND_PREPARE;
                        break;
                case PM_POST_SUSPEND:
                        record_suspend_state = PM_POST_SUSPEND;
                        break;
                default:
                        break;
        }
        return NOTIFY_DONE;
}

static struct notifier_block suspend_status_notifier = {
        .notifier_call = suspend_status_pm_event,
};


static int __init rtk_pm_init(void)
{
        printk("Realtek Power Management 2015/08/05 15:00\n");

        //rtk_pm_init_8051(0);
        suspend_set_ops(&rtk_pm_ops);
//#ifdef CONFIG_QUICK_HIBERNATION
        hibernation_set_ops(&rtk_hibernation_ops);
        std_kobj_init();
//#endif
        register_pm_notifier(&suspend_status_notifier);
        return 0;
}

//#ifdef CONFIG_QUICK_HIBERNATION
__initcall(rtk_pm_init);
//#else
//late_initcall(rtk_pm_init);
//#endif

