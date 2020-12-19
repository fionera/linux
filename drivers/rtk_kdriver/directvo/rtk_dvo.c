#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/slab.h>         /* kmalloc */
#include <linux/fs.h>           /* everything... */
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/uaccess.h>      /* copy_from_user */
#include <asm/cacheflush.h>
#include <linux/string.h>       /* memset */
#include <linux/proc_fs.h>
#include <linux/dma-mapping.h> /* dma_alloc_coherent */
#include <linux/pageremap.h>
#include <linux/kthread.h>
#include <linux/err.h>         /* IS_ERR() PTR_ERR() */
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/auth.h>
#include <linux/freezer.h>
#include <linux/wait.h>

#include <mach/rtk_platform.h>
#include <mach/system.h>
#include <rtk_kdriver/RPCDriver.h>  /* register_kernel_rpc, send_rpc_command */
#include <VideoRPCBaseDS_data.h>
#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <VideoRPC_System.h>
	#include <VideoRPC_System_data.h>
#else
	#include <rpc/VideoRPC_System.h>
	#include <rpc/VideoRPC_System_data.h>
#endif

#include "rtk_dvo.h"

static struct class *rtk_dvo_class = NULL;
static struct cdev rtk_dvo_cdev;
static struct platform_device *rtk_dvo_platform_devs = NULL;

static struct platform_driver rtk_dvo_device_driver = {
    .driver = {
        .name   	= RTK_DVO_NAME,
        .bus    	= &platform_bus_type,
    },
};

int rtk_dvo_major = RTK_DVO_MAJOR;
int rtk_dvo_minor = 0;

/* direct vo dump frame info */
#define _ENABLE 1
static int data;
struct file *file_DumpDVO = 0 ;
static unsigned long long f_offset_dump = 0;
DVO_MALLOC_STRUCT g_pDumpDVO_Send;
DVO_MALLOC_STRUCT g_pDumpDVO;
DVO_DUMP_STRUCT   DumpDVO_Config;
static struct task_struct *rtk_dvo_dump_tsk;
extern struct file *file_open(const char *path, int flags, int rights);
extern int    file_write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size);
extern void   file_close(struct file *file);
static void   rtk_dvo_dump_init(void);
static int    rtk_dvo_dump_thread(void *arg);
static int    rtk_dvo_dump_enable(const char *pattern, int length);
static void   rtk_dvo_dump_disable(void);
/***************** debug log utility *****************/
#define DVO_PRINT(level, format, ...) dvo_pr_log(level, __LINE__, format, ##__VA_ARGS__)
#define DVO_PRINT_NA(level, format, ...) dvo_pr_log(level, -1, format, ##__VA_ARGS__)
static DVO_LOG_LEVEL  rtk_dvo_log_level = DVO_LOG_NOTICE;

void dvo_pr_log(DVO_LOG_LEVEL level, int nLine, const char *pszFormat, ...)
{
    if (level <= rtk_dvo_log_level) {
        va_list args;
        size_t uStringSize = 0;
        char *pStringBuffer = NULL;
        int nPosition = snprintf(NULL, 0, "%s:%d ", RTK_DVO_NAME, nLine);
        if(nLine == -1) {
            va_start(args, pszFormat);
            uStringSize += vsnprintf(NULL, 0, pszFormat, args);
            va_end(args);
            uStringSize += 1;

            pStringBuffer = (char *)kmalloc(uStringSize, GFP_KERNEL);

            if (pStringBuffer != NULL) {
                va_start(args, pszFormat);
                vsnprintf(pStringBuffer, uStringSize, pszFormat, args);
                va_end(args);

                pr_warning("%s", pStringBuffer);
            }
            kfree(pStringBuffer);

        } else {
            va_start(args, pszFormat);
            uStringSize += vsnprintf(NULL, 0, pszFormat, args);
            va_end(args);
            uStringSize += nPosition + 1;

            pStringBuffer = (char *)kmalloc(uStringSize, GFP_KERNEL);

            if (pStringBuffer != NULL) {

                nPosition = snprintf(pStringBuffer, uStringSize, "%s:%d ", RTK_DVO_NAME, nLine);
                uStringSize -= nPosition;

                va_start(args, pszFormat);
                vsnprintf(pStringBuffer + nPosition, uStringSize, pszFormat, args);
                va_end(args);

                pr_warning("%s", pStringBuffer);
            }
            kfree(pStringBuffer);
        }
    }
    return;
}

void set_dvo_log_level(DVO_LOG_LEVEL level)
{
    DVO_PRINT(DVO_LOG_NOTICE, "set log level from %d to %d\n", rtk_dvo_log_level, level);
    rtk_dvo_log_level = level;
    return;
}

/*
 * debug function which dumps all status
 */
void rtk_dvo_dump(void)
{
    /* rtk_dvo dump */
    DVO_PRINT(DVO_LOG_NOTICE, "[rtk_dvo] dump\n");
}

/***************** rpc driver export func end *****************/
long rtk_dvo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    long ret = 0;
    DVO_PRINT(DVO_LOG_WARN, "[rtk_dvo:%d] receive ioctl(cmd:0x%08x, arg:0x%08x)\n", __LINE__, cmd, (unsigned int)arg);
    switch (cmd) {
        case DVO_IOC_INIT:
            {
                DVO_PRINT(DVO_LOG_DEBUG, "DVO_IOC_INIT\n");
                break;
            }

        case DVO_IOC_DEINIT:
            {
                DVO_PRINT(DVO_LOG_DEBUG, "DVO_IOC_DEINIT\n");
                break;
            }

        default:
            {
                DVO_PRINT(DVO_LOG_WARN, "[rtk_dvo] unknown ioctl(0x%08x)\n", cmd);
                break;
            }
    };

    return ret;
}

int rtk_dvo_open(struct inode *inode, struct file *filp)
{
    return 0 ;
}

int rtk_dvo_release(struct inode *inode, struct file *filp)
{
    return 0 ;
}

static void rtk_dvo_dump_init(void)
{
#ifndef CONFIG_ANDROID
	unsigned char default_file_name3[26] = "/tmp/video_dumpvocrc.bin" ;
#else
	unsigned char default_file_name3[32] = "/data/data/video_dumpvocrc.bin" ;
#endif

	DumpDVO_Config.enable = 0 ;
	DumpDVO_Config.mem_size = DUMP_ES_SIZE ;

#ifndef CONFIG_ANDROID
	memset((void *)DumpDVO_Config.file_name, '\0', FILE_NAME_SIZE );
	memcpy((void *)DumpDVO_Config.file_name, (void *)default_file_name3, 21);
#else
	memset((void *)DumpDVO_Config.file_name, '\0', 27 + 1 );
	memcpy((void *)DumpDVO_Config.file_name, (void *)default_file_name3, 27);
#endif

	g_pDumpDVO.Memory = g_pDumpDVO.PhyAddr = g_pDumpDVO.VirtAddr = 0 ;
	g_pDumpDVO_Send.Memory = g_pDumpDVO_Send.PhyAddr = g_pDumpDVO_Send.VirtAddr = 0 ;
}

int rtk_dvo_dump_enable(const char *pattern, int length)
{
	unsigned long return_value ;
	DVO_DUMP_BUFFER_HEADER *header;
	unsigned long vir_addr ;

	if (length > FILE_NAME_SIZE - 1) {
		pr_warn("[rtk_dvo:%d] file name is too long(<%d).\n", __LINE__, FILE_NAME_SIZE - 1);
		return 0;
	}
	else if (length > 0) {
		memset((void *)DumpDVO_Config.file_name, '\0', length+1);
		memcpy((void *)DumpDVO_Config.file_name, (void *)pattern, length);
	}

	pr_notice("[rtk_dvo:%d] dumpes_file_name(%s)\n", __LINE__, DumpDVO_Config.file_name);

	if (DumpDVO_Config.enable) {
		pr_notice("[rtk_dvo:%d] dump already enable!\n", __LINE__);
		return 0 ;
	}

	file_DumpDVO = file_open((char *)(DumpDVO_Config.file_name), O_TRUNC | O_RDWR | O_CREAT, 0x755) ;
	if (file_DumpDVO == 0) {
		pr_err("[rtk_dvo:%d] open log file fail\n", __LINE__);
		return -ENOMEM;
	}

	/* allocate debug memory */
	vir_addr = (unsigned long)dvr_malloc_uncached_specific(sizeof(DVO_RPC_DEBUG_MEMORY)+256, GFP_DCU1, (void **)(&g_pDumpDVO_Send.Memory));
	if (!vir_addr) {
		pr_err("[rtk_dvo:%d] alloc debug memory fail\n", __LINE__);
		return -ENOMEM;
	}
	g_pDumpDVO_Send.PhyAddr = (unsigned long)dvr_to_phys((void*)vir_addr);
	g_pDumpDVO_Send.VirtAddr = vir_addr ;
	pr_notice("[rtk_dvo:%d] Alloc DVOFRAME v(%lx) p(%lx) vn(%lx)\n", __LINE__, vir_addr, g_pDumpDVO_Send.PhyAddr, g_pDumpDVO_Send.Memory);

	if ((file_DumpDVO != 0) && (DumpDVO_Config.mem_size > 0)) {
		/* alocate dump memory */
		vir_addr = (unsigned long)dvr_malloc_uncached_specific(DumpDVO_Config.mem_size, GFP_DCU1, (void **)(&g_pDumpDVO.Memory));
		if (!vir_addr) {
			vir_addr = g_pDumpDVO_Send.VirtAddr ;
			dvr_free((void*)vir_addr);
			pr_err("[rtk_dvo:%d] alloc debug memory fail\n", __LINE__);
			return -ENOMEM;
		}
		g_pDumpDVO.PhyAddr = (unsigned long)dvr_to_phys((void*)vir_addr);
		g_pDumpDVO.VirtAddr = vir_addr ;

		pr_notice("[rtk_dvo:%d] Alloc DUMPVOME v(%lx) p(%lx) vn(%lx)\n", __LINE__, vir_addr, g_pDumpDVO.PhyAddr, g_pDumpDVO.Memory);

		/* setup dump es ring buffer header */
		header = (DVO_DUMP_BUFFER_HEADER*) g_pDumpDVO.Memory;
		header->magic = htonl(0xdeadcafe) ;
		header->size  = htonl(DumpDVO_Config.mem_size) ;
		header->rd    = htonl(g_pDumpDVO.PhyAddr + sizeof(DVO_DUMP_BUFFER_HEADER)) ;
		header->wr    = htonl(g_pDumpDVO.PhyAddr + sizeof(DVO_DUMP_BUFFER_HEADER)) ;

		*(unsigned long *)g_pDumpDVO_Send.Memory = htonl(g_pDumpDVO.PhyAddr) ;

		pr_notice("[rtk_dvo:%d] Header:%lx  m=%x size=%x (%d) r(%x) w(%x)\n", __LINE__,
                  (unsigned long)header,
                  (unsigned int)header->magic, (unsigned int)header->size, DumpDVO_Config.mem_size, (unsigned int)header->rd, (unsigned int)header->wr);
	}
	else {
		pr_notice("[rtk_dvo:%d] no allocate debug dump ring buffer!\n", __LINE__);
		return 0 ;
	}

	rtk_dvo_dump_tsk = kthread_create(rtk_dvo_dump_thread, &data, "rtk_dvo_dump_thread");
	if (IS_ERR(rtk_dvo_dump_tsk)) {
		pr_err("[rtk_dvo:%d] rtk dump thread create fail!\n", __LINE__);
		rtk_dvo_dump_tsk = NULL;
		return -1 ;
	}
	wake_up_process(rtk_dvo_dump_tsk);

	DumpDVO_Config.enable = 1;
#if 1
	if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_DumpDVOFrameInfo, (unsigned long)g_pDumpDVO_Send.PhyAddr, _ENABLE, &return_value)) {
            pr_err("[rtk_dvo:%d] VIDEO_RPC_VOUT_ToAgent_DumpDVOFrameInfo fail %ld\n", __LINE__, return_value);
            rtk_dvo_dump_disable();
            return -1;
	}
#endif
        pr_err("[rtk_dvo:%d] rtk_dvo dump enable end. (%d)\n", __LINE__, DumpDVO_Config.enable);
	return 0;
}

void rtk_dvo_dump_disable(void)
{
	unsigned long return_value ;
	int ret = 0;
	unsigned long vir_addr ;
	DVO_DUMP_BUFFER_HEADER *header;

	if (!DumpDVO_Config.enable) {
		pr_notice("[rtk_dvo] frame info dump not enable!\n");
		return ;
	}

	DumpDVO_Config.enable = 0 ;
	/* setup debug dump ring buffer header */
	header = (DVO_DUMP_BUFFER_HEADER*) g_pDumpDVO.Memory;
	header->magic = htonl(0xdeadcafe) ;
	header->size = htonl(16) ;
	header->rd = htonl(g_pDumpDVO.PhyAddr + sizeof(DVO_DUMP_BUFFER_HEADER)) ;
	header->wr = htonl(g_pDumpDVO.PhyAddr + sizeof(DVO_DUMP_BUFFER_HEADER)) ;
	*(unsigned long *)g_pDumpDVO_Send.Memory = htonl(g_pDumpDVO.PhyAddr) ;
#if 1
	if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_DumpDVOFrameInfo, 0, 0, &return_value))
		pr_err("[rtk_dvo:%d] VIDEO_RPC_VOUT_ToAgent_DumpDVOFrameInfo fail %ld\n", __LINE__, return_value);

	pr_debug("[rtk_dvo:%d] kthread_stop rtk_dvo_dump_tsk!! \n", __LINE__);
	ret = kthread_stop(rtk_dvo_dump_tsk);
	if (!ret)
		pr_debug("[rtk_dvo:%d] dump dvo frameinfo thread stopped\n", __LINE__);

	f_offset_dump = 0 ;
	if (g_pDumpDVO.PhyAddr) {
		vir_addr = g_pDumpDVO.VirtAddr;
		dvr_free((void*)vir_addr);
		g_pDumpDVO.Memory = g_pDumpDVO.PhyAddr = g_pDumpDVO.VirtAddr = 0 ;
	}

	if (g_pDumpDVO_Send.PhyAddr) {
		vir_addr = g_pDumpDVO_Send.VirtAddr;
		dvr_free((void*)vir_addr);
		g_pDumpDVO_Send.Memory = g_pDumpDVO_Send.PhyAddr = g_pDumpDVO_Send.VirtAddr = 0 ;
	}

	if (file_DumpDVO)
		file_close(file_DumpDVO) ;
	file_DumpDVO = 0 ;
#endif
}

// Dump DVO frame info to file
static int rtk_dvo_dump_thread(void *arg)
{
	unsigned long magic, size, wr, rd;
	unsigned char *wrPtr, *rdPtr, *basePtr, *limitPtr;

	pr_err("[rtk_dvo:%d] dvo_dump_thread start\n", __LINE__);
	for (;;) {
		if (kthread_should_stop()) {
                    pr_err("[rtk_dvo:%d] dvo_dump_thread  should  STOP!!\n", __LINE__);
                    break;
                }
		if (file_DumpDVO != 0 && DumpDVO_Config.enable) {
			DVO_DUMP_BUFFER_HEADER *header;
			header = (DVO_DUMP_BUFFER_HEADER*) g_pDumpDVO.Memory;
			magic  = ntohl(header->magic);
			size   = ntohl(header->size);
			rd     = ntohl(header->rd);
			wr     = ntohl(header->wr);
			wrPtr  = (unsigned char *)(g_pDumpDVO.Memory + wr - g_pDumpDVO.PhyAddr); /* make virtual address */
			rdPtr  = (unsigned char *)(g_pDumpDVO.Memory + rd - g_pDumpDVO.PhyAddr); /* make virtual address */
			basePtr = (unsigned char *)(g_pDumpDVO.Memory + sizeof(DVO_DUMP_BUFFER_HEADER));
			size -= sizeof(DVO_DUMP_BUFFER_HEADER);
			limitPtr = basePtr+ size;

                        //pr_notice("[rtk_dvo:%d] Header %x, m=%x size=%x r(%x) w(%x)\n", __LINE__, (uintptr_t)header, (uintptr_t)magic, (uintptr_t)size, (uintptr_t)rd, (uintptr_t)wr);
                #if 0 // TEST
        static unsigned long last_wr=0;
                    if (last_wr != wr) {
                        pr_notice("[rtk_dvo] Header %x, m=%x size=%x r(%x) w(%x)\n", (unsigned int)header, (unsigned int)magic, (unsigned int)size, (unsigned int)rd, (unsigned int)wr);
                        pr_notice("[rtk_dvo] Ptr w(%x) r(%x) b(%x) (%x), offset=%x\n", (unsigned int)wrPtr, (unsigned int)rdPtr, (unsigned int)basePtr, (unsigned int)limitPtr, (unsigned int)f_offset_dump);
                        last_wr = wr;
                    }
                #endif

			if (wrPtr < rdPtr) {
				wrPtr = wrPtr + size;
			}

			if (wrPtr > rdPtr) {
				if (wrPtr > limitPtr) {
					file_write(file_DumpDVO, f_offset_dump, rdPtr, limitPtr -rdPtr);
					f_offset_dump += limitPtr -rdPtr;
					file_write(file_DumpDVO, f_offset_dump, basePtr, wrPtr - limitPtr);
					f_offset_dump += wrPtr - limitPtr;
				}
				else {
					file_write(file_DumpDVO, f_offset_dump, rdPtr, wrPtr - rdPtr);
					f_offset_dump += wrPtr - rdPtr;
				}
				header->rd = htonl(wr);
                #if 0 // TEST
                         pr_notice("[rtk_dvo] f_offset_dump=%x\n", (unsigned int)f_offset_dump);
                #endif
			}
		} else {
//                        pr_notice("[rtk_dvo] sleep: %d / %d\n", file_DumpDVO, DumpDVO_Config.enable);
                }

		msleep(80); /* sleep 8 ms */
	}

	pr_err("[rtk_dvo:%d] dvo_dump_thread break\n", __LINE__);
	return 0;
}

void rtk_dvo_help(void)
{
    DVO_PRINT(DVO_LOG_NOTICE, " echo h > /dev/rtkdvo                      ## print debug help\n");
    DVO_PRINT(DVO_LOG_NOTICE, " echo lev x > /dev/rtkdvo                  ## set log level\n");
    DVO_PRINT(DVO_LOG_NOTICE, " echo dvo@ enable_dump > /dev/rtkdvo       ## enable  frame info dump\n");
    DVO_PRINT(DVO_LOG_NOTICE, " echo dvo@ disable_dump > /dev/rtkdvo      ## disable frame info dump\n");
    DVO_PRINT(DVO_LOG_NOTICE, " echo dvo@ xxx yyy > /dev/rtkdvo           ## set TA log level (ex: sstore 256)\n");
}

void rtkdvo_send_string(const char *pattern, int length)
{
#if 0
	unsigned long ret = S_OK;
	unsigned long vir_addr;
	unsigned long phy_addr;
	char *command = NULL;

	vir_addr = (unsigned long)dvr_malloc_uncached_specific(length+1, GFP_DCU1, (void **)&command);
	if (!vir_addr) {
		DVO_PRINT(DVO_LOG_ERROR, "[%s %d]alloc string memory fail\n",__FUNCTION__,__LINE__);
		return;
	}
	phy_addr = (unsigned long)dvr_to_phys((void *)vir_addr);
	memset((void *)command, '\0', length+1);
	memcpy((void *)command, (void *)pattern, length);

	DVO_PRINT(DVO_LOG_DEBUG, "[rtk_dvo] debug string(%s), length(%d)\n", command, length);

	if (send_rpc_command(RPC_KCPU, RPC_SCPU_ID_0x502_teestub_debug_command, phy_addr, length, &ret))
		DVO_PRINT(DVO_LOG_ERROR, "[rtk_dvo] debug string(%s) fail %d\n", pattern, (int)ret);

	dvr_free((void *)vir_addr);
#endif
	return;
}

ssize_t rtk_dvo_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    long ret = count;
    char cmd_buf[100] = {0};

    DVO_PRINT(DVO_LOG_DEBUG, "count=%d, buf=0x%08lx\n", count, (long)(uintptr_t)buf);

    if (count >= 100)
        return -EFAULT;

    if (copy_from_user(cmd_buf, buf, count)) {
        ret = -EFAULT;
    }

    if ((cmd_buf[0] == 't') && (cmd_buf[1] == 'e') && (cmd_buf[2] == 's') && (cmd_buf[3] == 't') &&
        (cmd_buf[4] == 'm') && (cmd_buf[5] == 'o') && (cmd_buf[6] == 'd') && (cmd_buf[7] == 'e') && (cmd_buf[8] == '='))
    {
        // shell command as: echo testmode=1 testresult=/tmp/var/log/xxxxx > /dev/vodev
        if ((cmd_buf[9] == '1') && (cmd_buf[11] == 't') && (cmd_buf[20] == 't') && (cmd_buf[21] == '=')){
            pr_notice("\n=== [rtk_dvo:%d] enable dump frame info =============\n", __LINE__);
            rtk_dvo_dump_enable(&cmd_buf[22], (count-22)-1);
            pr_notice("\n=======================================\n");
        } // shell command as: echo testmode=1 /tmp/var/log/xxxxx > /dev/vodev
        else if ((cmd_buf[9] == '1') && (cmd_buf[11] == '/')) {
            pr_notice("\n=== [rtk_dvo:%d] enable dump DVO =============\n", __LINE__);
            rtk_dvo_dump_enable(&cmd_buf[11], (count-11)-1);
            pr_notice("\n=======================================\n");
        } // shell command as: echo testmode=0 > /dev/vodev
        else if (cmd_buf[9] == '0') {
            pr_notice("\n=== [rtk_dvo:%d] disable dump DVO =============\n", __LINE__);
            rtk_dvo_dump_disable();
            pr_notice("\n=======================================\n");
        }
        else {
            pr_notice("%s\n", cmd_buf);
        }
    }


    if ((cmd_buf[0] == 'd') && (count == 2)) {
        DVO_PRINT(DVO_LOG_NOTICE, "== rtk_dvo =========================\n");
        rtk_dvo_dump();
        DVO_PRINT(DVO_LOG_NOTICE, "=========================================\n");

    } else if ((cmd_buf[0] == 'h') && (count == 2)) {
        DVO_PRINT(DVO_LOG_NOTICE, "== rtk_dvo help usage: ==\n");
        rtk_dvo_help();
        DVO_PRINT(DVO_LOG_NOTICE, "==============================\n");
    } else if ((cmd_buf[0] == 'l') && (cmd_buf[1] == 'e') && (cmd_buf[2] == 'v')
            && (count == 6)) {
        DVO_PRINT(DVO_LOG_WARN, "== rtk_dvo log level: ==\n");
        if((cmd_buf[4] & 0x0F) <= 9)
            set_dvo_log_level(cmd_buf[4] & 0x0F);
        DVO_PRINT(DVO_LOG_NOTICE, "==============================\n");
    } else if (count > 5 &&
        (cmd_buf[0] == 't') && (cmd_buf[1] == 'e') && (cmd_buf[2] == 'e') &&
        (cmd_buf[3] == '@') && (cmd_buf[4] == ' ')) {
        DVO_PRINT(DVO_LOG_NOTICE, "== rtkdvo: send debug string: %s=============\n", cmd_buf + 9);
        rtkdvo_send_string(&cmd_buf[5], count - 5 - 1 /* remove \n */);
        DVO_PRINT(DVO_LOG_NOTICE, "=======================================\n");
    } else {
        DVO_PRINT(DVO_LOG_NOTICE, "%s\n", cmd_buf);
    }

    return ret;
}

struct file_operations rtk_dvo_fops = {
    .owner  		= THIS_MODULE,
    .unlocked_ioctl 	= rtk_dvo_ioctl,
    .write  		= rtk_dvo_write,
    .open   		= rtk_dvo_open,
    .release		= rtk_dvo_release,
};

static char *rtk_dvo_devnode(struct device *dev, umode_t *mode)
{
    *mode = 0666;
    return NULL;
}

static int rtk_dvo_init(void)
{
    int result;
    dev_t dev = 0;
    unsigned char DvoReady;
    DvoReady = 1;

    DVO_PRINT(DVO_LOG_NOTICE, "******  rtk_dvo init module  ******\n");
    if (rtk_dvo_major) {
        dev = MKDEV(rtk_dvo_major, rtk_dvo_minor);
        result = register_chrdev_region(dev, 1, RTK_DVO_NAME);
    } else {
        result = alloc_chrdev_region(&dev, rtk_dvo_minor, 1, RTK_DVO_NAME);
        rtk_dvo_major = MAJOR(dev);
    }

    if (result < 0) {
        DVO_PRINT(DVO_LOG_ERROR, "[rtk_dvo] can't get chrdev region...\n");
        return result;
    }

    DVO_PRINT(DVO_LOG_DEBUG, "[rtk_dvo] init module major number = %d\n", rtk_dvo_major);

    rtk_dvo_class = class_create(THIS_MODULE, RTK_DVO_NAME);
    if (IS_ERR(rtk_dvo_class)) {
        DVO_PRINT(DVO_LOG_ERROR, "[rtk_dvo] can't create class...\n");
        result = PTR_ERR(rtk_dvo_class);
        goto fail_class_create;
    }

    rtk_dvo_class->devnode = rtk_dvo_devnode;

    rtk_dvo_platform_devs = platform_device_register_simple(RTK_DVO_NAME, -1, NULL, 0);
    if (platform_driver_register(&rtk_dvo_device_driver) != 0) {
        DVO_PRINT(DVO_LOG_ERROR, "[rtk_dvo] can't register platform driver...\n");
        result = -EINVAL;
        goto fail_platform_driver_register;
    }

    cdev_init(&rtk_dvo_cdev, &rtk_dvo_fops);
    rtk_dvo_cdev.owner = THIS_MODULE;
    rtk_dvo_cdev.ops = &rtk_dvo_fops;
    result = cdev_add(&rtk_dvo_cdev, dev, 1);
    if (result < 0) {
        DVO_PRINT(DVO_LOG_ERROR, "[rtk_dvo] can't add character device...\n");
        goto fail_cdev_init;
    }
    device_create(rtk_dvo_class, NULL, dev, NULL, RTK_DVO_NAME);

#if 0
    /* register RPC command mapping handler */
    if (register_kernel_rpc(RPC_VCPU_ID_0x109_DVO_READY, (FUNC_PTR)rpcVoReady) == 1)
        pr_crit("[rtk_dvo:%d] Register RPC DvoReady failed!\n", __LINE__);

    if (send_rpc_command(RPC_VIDEO, VIDEO_RPC_VOUT_ToAgent_DvoDriverReady, DvoReady, 0, &ret))
        pr_crit("[rtk_dvo:%d] DvoReady RPC fail!!\n", __LINE__);
#endif

    pr_debug("[rtk_dvo:%d] rtk_dvo_dump_init... \n", __LINE__);
    // dump DVO frame info
    rtk_dvo_dump_init();

    //set debug level
    set_dvo_log_level(DVO_LOG_NOTICE);

    return 0;

fail_cdev_init:
    platform_driver_unregister(&rtk_dvo_device_driver);
fail_platform_driver_register:
    platform_device_unregister(rtk_dvo_platform_devs);
    rtk_dvo_platform_devs = NULL;
    class_destroy(rtk_dvo_class);
fail_class_create:
    rtk_dvo_class = NULL;
    unregister_chrdev_region(dev, 1);
    return result;
}

static void rtk_dvo_exit(void)
{
    dev_t dev ;

    dev = MKDEV(rtk_dvo_major, rtk_dvo_minor);

    /* if ((rtk_dvo_platform_devs == NULL) || (class_destroy == NULL)) ?? */
    if ((rtk_dvo_platform_devs == NULL) || (rtk_dvo_class == NULL))
        BUG();

    device_destroy(rtk_dvo_class, dev);
    cdev_del(&rtk_dvo_cdev);

    platform_driver_unregister(&rtk_dvo_device_driver);
    platform_device_unregister(rtk_dvo_platform_devs);
    rtk_dvo_platform_devs = NULL;

    class_destroy(rtk_dvo_class);
    rtk_dvo_class = NULL;

    unregister_chrdev_region(dev, 1);
}

late_initcall(rtk_dvo_init);
module_exit(rtk_dvo_exit);
