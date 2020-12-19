#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
//#include <rbus/mmc_reg.h>
#include <rbus/stb_reg.h>
#include <rbus/emcu_reg.h>
#include <rbus/iso_misc_irda_reg.h>
#include "rtk_emcu.h"
#include <rtk_kdriver/rtk_emcu_export.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/suspend.h>

#define IMAGE_SIZE      (1024*16)	//size 16K
#define IMAGE_SIZE_TINY (1024)      //size  1K
#define DDR_EMCU_ADDR   0x00004000

#ifdef CONFIG_ARM64
extern void soft_restart(unsigned long);
#endif

#ifdef CONFIG_RTK_KDRV_IR
#if defined(CONFIG_REALTEK_INT_MICOM)
extern int IR_Init(int main1_protcol, int main0_protocol);
extern void ir_local_int_on_off(bool hw_mode, bool on);
extern void ir_global_int_on_off(bool hw_mode, bool on);
#else
extern int ir_suspend_protocol(void);
#endif
#endif

#if defined(CONFIG_REALTEK_INT_MICOM)
extern void ir_reg_init(void);  //../irrc.c"
extern int IR_Init(int mode);
#endif
#if !defined(CONFIG_REALTEK_INT_MICOM)
typedef enum {
	PCB_PIN_TYPE_UNUSED = 0,
	PCB_PIN_TYPE_LSADC,
	PCB_PIN_TYPE_EMCU_GPIO,
	PCB_PIN_TYPE_GPIO,
	PCB_PIN_TYPE_ISO_GPIO,
	PCB_PIN_TYPE_UNIPWM,
	PCB_PIN_TYPE_ISO_UNIPWM,
	PCB_PIN_TYPE_PWM,
	PCB_PIN_TYPE_ISO_PWM,
	PCB_PIN_TYPE_AUDIO,
	PCB_PIN_TYPE_UNDEF,
} PCB_PIN_TYPE_T;

typedef enum {
	PCB_GPIO_TYPE_INPUT,
	PCB_GPIO_TYPE_OUPUT,
	PCB_GPIO_TYPE_TRI_IO,
	PCB_GPIO_TYPE_UNDEF,
} PCB_GPIO_TYPE_T;

typedef enum {
	PCB_LSADC_TYPE_VOLTAGE,
	PCB_LSADC_TYPE_CURRENT,
	PCB_LSADC_TYPE_UNDEF,
} PCB_LSADC_TYPE_T;
#endif



#define IR_KEY_LEN      8
//#define LSADC_LEN       16
#define PARAM_MEM_LEN       128

#define loc_keypad     4*0
#define loc_cec        4*1
#define loc_pps        4*2
#define loc_wut        4*3
#define loc_irda       4*4
#define loc_pwen       4*5
#define loc_wow        4*6
#define loc_stm        4*7


#define RTD_SHARE_MEM_LEN       64
#define RTD_SHARE_MEM_BASE      0xb8060500
#define PM_EVENT_MSG            RTD_SHARE_MEM_BASE

#define PARAM_LEN_KEYPAD        10
#define PARAM_LEN_PPS           1
#define PARAM_LEN_WUT           1
#define PARAM_LEN_IRDA          16
#define PARAM_LEN_PWEN          3
#define PARAM_LEN_WOW           6   //server IP/MAC; host IP/MAC

#define PARAM_LEN_TDUR          2
#define PARAM_LEN_WK_SOR        1
#define PARAM_LEN_WK_STS        1

#define IDX_REG_INDEX           0                                       //reg0 for index
#define IDX_REG_KEYPAD          1                                       //reg1~10
#define IDX_REG_PPS             (IDX_REG_KEYPAD+PARAM_LEN_KEYPAD)       //reg11
#define IDX_REG_WUT             (IDX_REG_PPS+PARAM_LEN_PPS)             //reg12
#define IDX_REG_IRDA            (IDX_REG_WUT+PARAM_LEN_WUT)             //reg13~28
#define IDX_REG_PWEN            (IDX_REG_IRDA+PARAM_LEN_IRDA)           //reg29~31
#define IDX_REG_WOW             (IDX_REG_PWEN+PARAM_LEN_PWEN)           //reg32~35

//backward counting
#define IDX_REG_TIME_DUR        (RTD_SHARE_MEM_LEN-PARAM_LEN_TDUR)      //reg62~63

#define SET_KEYPAD_PARM_H(parm)     rtd_outl(RTD_SHARE_MEM_BASE+4*IDX_REG_KEYPAD,parm);
#define SET_KEYPAD_PARM_L(parm)     rtd_outl(RTD_SHARE_MEM_BASE+4*(IDX_REG_KEYPAD+1),parm)
#define SET_KEYPAD_PARM_E(parm)     rtd_outl(RTD_SHARE_MEM_BASE+4*(IDX_REG_KEYPAD+2),parm)
#define SET_PPS_PARM(parm)          rtd_outl(RTD_SHARE_MEM_BASE+4*IDX_REG_PPS,parm)
#define SET_WUT_PARM(parm)          rtd_outl(RTD_SHARE_MEM_BASE+4*IDX_REG_WUT,parm)
#define SET_IRDA_PARM(num,parm)     rtd_outl(RTD_SHARE_MEM_BASE+4*(IDX_REG_IRDA+num),parm)
#define SET_IRDA_PROTOCOL(parm)     SET_IRDA_PARM(0,parm)
#define SET_PWEN_PARM(num,parm)     rtd_outl(RTD_SHARE_MEM_BASE+4*(IDX_REG_PWEN+num),parm)

#define SET_EVENT_MSG(msg)          rtd_outl(PM_EVENT_MSG,msg)
#define SET_EVENT_IDX(msg,loc,idx)  msg = ((msg & ~(0xf<<loc))|(idx<<loc))

#define CHK_KEYPAD(num)     num&0xf
#define GET_KEYPAD_PARM_H() rtd_inl(RTD_SHARE_MEM_BASE+4*IDX_REG_KEYPAD)
#define GET_KEYPAD_PARM_L() rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_KEYPAD+1))
#define GET_KEYPAD_PARM_E() rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_KEYPAD+2))
#define CHK_CEC(num)        (num>>loc_cec)&0xf
#define CHK_PPS(num)        (num>>loc_pps)&0xf
#define CHK_STM(num)        (num>>loc_stm)&0xf

#define GET_PPS_PARM()      rtd_inl(RTD_SHARE_MEM_BASE+4*IDX_REG_PPS)
#define CHK_WUT(num)        (num>>loc_wut)&0xf
#define GET_WUT_PARM()      rtd_inl(RTD_SHARE_MEM_BASE+4*IDX_REG_WUT)
#define CHK_IRDA(num)       (num>>loc_irda)&0xf
#define GET_IRDA_PARM(num)  rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_IRDA+num))
#define GET_IRDA_PROTOCOL() GET_IRDA_PARM(0)
#define CHK_PWEN(num)       (num>>loc_pwen)&0xf
#define GET_PWEN_PARM(num)  rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_PWEN+num))

/////////////////
#define CHK_WOW(num)        ((num>>loc_wow)&0xf)
#define GET_SVER_MAC(ptr)          \
        do{ \
            static unsigned int tmp1;  \
            static unsigned int tmp2;  \
            tmp1 = rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_WOW+2));    \
            tmp2 = rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_WOW+3));    \
            *((unsigned char*)ptr+0) = (unsigned char)((tmp1>>24) & 0xff) ;    \
            *((unsigned char*)ptr+1) = (unsigned char)((tmp1>>16) & 0xff) ;    \
            *((unsigned char*)ptr+2) = (unsigned char)((tmp1>> 8) & 0xff) ;    \
            *((unsigned char*)ptr+3) = (unsigned char)((tmp1>> 0) & 0xff) ;    \
            *((unsigned char*)ptr+4) = (unsigned char)((tmp2>> 8) & 0xff) ;    \
            *((unsigned char*)ptr+5) = (unsigned char)((tmp2>> 0) & 0xff) ;    \
        }while(0)

#define GET_HOST_MAC(ptr)          \
        do{ \
            static unsigned int tmp1;  \
            static unsigned int tmp2;  \
            tmp1 = rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_WOW+4));    \
            tmp2 = rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_WOW+5));    \
            *((unsigned char*)ptr+0) = (unsigned char)((tmp1>>24) & 0xff) ;    \
            *((unsigned char*)ptr+1) = (unsigned char)((tmp1>>16) & 0xff) ;    \
            *((unsigned char*)ptr+2) = (unsigned char)((tmp1>> 8) & 0xff) ;    \
            *((unsigned char*)ptr+3) = (unsigned char)((tmp1>> 0) & 0xff) ;    \
            *((unsigned char*)ptr+4) = (unsigned char)((tmp2>> 8) & 0xff) ;    \
            *((unsigned char*)ptr+5) = (unsigned char)((tmp2>> 0) & 0xff) ;    \
        }while(0)
//////////////////
#define GET_EVENT_MSG()     rtd_inl(PM_EVENT_MSG)

#define GET_TIME_DUR(x,y)   x = rtd_inl(RTD_SHARE_MEM_BASE+4*IDX_REG_TIME_DUR); \
                            y = rtd_inl(RTD_SHARE_MEM_BASE+4*(IDX_REG_TIME_DUR+1))

/* general reg setting */
#define REG_MAGIC_51                0xb8060110
#define REG_8051_TICK               0xb8060114
#define REG_SCPU_TICK               0xb8060118

/* 8051 magic event */
#define MAGIC_8051                  0x2379beef  /* 8051 is runing but counting RTC only. setting by 8051 only.*/
#define WAKE_8051                   0x9021affe  /* send message to 8051,let 8051 wakeup and handle standby status.. */
#define CHECK_8051                   0           /* set magic reg of 8051 to zero then check 8051 is runing. */

#define GET_MAGIC_PARM()            rtd_inl(REG_MAGIC_51)
#define SET_MAGIC_PARM(num)         rtd_outl(REG_MAGIC_51,num)

/* clock timer event */
#define GET_SCPU_TICK()             rtd_inl(REG_SCPU_TICK)&0x3fffffff
#define SET_SCPU_TICK(num)          rtd_outl(REG_SCPU_TICK,((rtd_inl(REG_SCPU_TICK)&0xC0000000)|(num&0x3fffffff)))

#define GET_SCPU_CHK()              (rtd_inl(REG_SCPU_TICK)&0x80000000)>>31
#define SET_SCPU_CHK()              rtd_outl(REG_SCPU_TICK,(rtd_inl(REG_SCPU_TICK)|0x80000000))
#define CLR_SCPU_CHK()              rtd_outl(REG_SCPU_TICK,(rtd_inl(REG_SCPU_TICK)&0x7fffffff))

#define GET_8051_TICK()             rtd_inl(REG_8051_TICK)&0x3fffffff
#define SET_8051_TICK(num)          rtd_outl(REG_8051_TICK,((rtd_inl(REG_8051_TICK)&0xC0000000)|(num&0x3fffffff)))

#define GET_8051_CHK()              (rtd_inl(REG_8051_TICK)&0x80000000)>>31
#define SET_8051_CHK()              rtd_outl(REG_8051_TICK,(rtd_inl(REG_8051_TICK)|0x80000000))
#define CLR_8051_CHK()              rtd_outl(REG_8051_TICK,(rtd_inl(REG_8051_TICK)|0x7fffffff))

#if defined(CONFIG_REALTEK_INT_MICOM)
//#define GET_PIN_TYPE(x,y)   ((PCB_PIN_TYPE_T) (y & 0xFF))
//#define GET_PIN_INDEX(x,y)  ((unsigned char) ((y >> 8) & 0xFF))
#else
#define GET_PIN_TYPE(x,y)   ((PCB_PIN_TYPE_T) (y & 0xFF))
#define GET_PIN_INDEX(x,y)  ((unsigned char) ((y >> 8) & 0xFF))
#endif

#define STM_NOEMAL      1
#define STM_WAKEUP      2

unsigned int emcu_read_file = 0;
unsigned int w_flag = 0;
unsigned int g_no_irda=0;
UINT8 single_key = 0;

#define SINGLE_KEY (0x02)
#define RELEASE_KEY (0x0a)

void act_self_refresh(void)
{
/* emcu will DDR self refresh, so SCPU don't need to take care. */
#if 0
    // use direct access, not use rtd_xxxx API
    unsigned int temp1;
    unsigned int temp2;
    unsigned int temp3;

    temp1 = *((volatile unsigned int*)0xfe008828)|0x0008;// bit3
    temp2 = *((volatile unsigned int*)0xfe003828)|0x0008;// bit3
    temp3 = *((volatile unsigned int*)0xfe060034)|0x0200;// bit9

    //issue SR_EN command to DRAM
    *((volatile unsigned int*)0xfe008828) = temp1;
    *((volatile unsigned int*)0xfe003828) = temp2;
    *((volatile unsigned int*)0xfe060034) = temp3;
#endif
}

static void calculate_DDR_checksum(void)
{
	unsigned int *begin_ptr = phys_to_virt(0x02000000);
	unsigned int *end_ptr = phys_to_virt(0x16000000);
	const unsigned int length = 0x01000000/4;
	unsigned int total_checksume = 0;

	while(begin_ptr < end_ptr)
	{
		unsigned int i;
		unsigned int sumValue = 0;
		unsigned int *data_ptr = begin_ptr;

		for(i=0; i<length; i++)
		{
			sumValue+= *data_ptr;
			data_ptr++;
		}

		printk("DDR_checksum(%lx/%x)\n", (unsigned long int)begin_ptr, sumValue);
		total_checksume +=sumValue;
		begin_ptr+=length;

	}
	printk("DDR_checksum total (%x)\n", total_checksume);


	//check for ACPU ddr
	{
		unsigned int *data_ptr = phys_to_virt(0x01600000 );
		unsigned int *end_ptr = phys_to_virt(0x01a00000);
		unsigned int sumValue=0;
		while(data_ptr<end_ptr)
		{
			sumValue+=*data_ptr;
			data_ptr++;
		}
		printk("DDR_checksum A(%x)\n", sumValue);
	}
	//check for VCPU1 ddr
	{
		unsigned int *data_ptr = phys_to_virt(0x01a00000 );
		unsigned int *end_ptr = phys_to_virt(0x01e00000);
		unsigned int sumValue=0;
		while(data_ptr<end_ptr)
		{
			sumValue+=*data_ptr;
			data_ptr++;
		}
		printk("DDR_checksum V1(%x)\n", sumValue);
	}
	//check for VCPU2 ddr
	{
		unsigned int *data_ptr = phys_to_virt(0x01e00000 );
		unsigned int *end_ptr = phys_to_virt(0x01f00000);
		unsigned int sumValue=0;
		while(data_ptr<end_ptr)
		{
			sumValue+=*data_ptr;
			data_ptr++;
		}
		printk("DDR_checksum V2(%x)\n", sumValue);
	}
}


void wakeup_8051(int flag)
{
    printk("@@!!!!!enter pm_wakeup_emcu\n");

    rtd_outl(0xb8062308, 0xcf);
    rtd_outl(0xb8062304, 0x0);	// disable uart interrupt
	rtd_outl(0xb8062310, 0x0);

#if !defined(CONFIG_REALTEK_INT_MICOM)
    // pin share and iso uart init
    //rtd_outl(0xb8060200, 0xF8F88888);     //uart pin mux
    //rtd_maskl(0xb80602a0,   0xFFFFFEFF, 0); //input mux
    //rtd_outl(0xb806130c,    0x83);          //baud rate setting
    //rtd_outl(0xb8061300,    0xf);           //baud rate setting
    //rtd_outl(0xb8061308,    0x6);           //baud rate setting
    //rtd_outl(0xb8061308,    0x1);           //baud rate setting
    //rtd_outl(0xb8061310,    0x0);           //baud rate setting
    //rtd_outl(0xb806130c,    0x3);           //baud rate setting

    if(rtd_inl(0xb8060110) == 0x9021aebe)
        rtd_outl(STB_ST_SRST1_reg, _BIT9);           // rst off
    rtd_outl(STB_ST_CLKEN1_reg, _BIT9 | _BIT0);  // clk enable
    rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);   // rst on

    udelay(500);

    //rtd_outl(ST_SRST1_reg, rtd_inl(ST_SRST1_reg) | _BIT9);
#endif
    if(flag == SUSPEND_RAM){
        printk("%s(%u)ecpu reset release done\n",__func__,__LINE__);

				if(1){
					printk("%s(%u)call DDR self refresh\n",__func__,__LINE__);
#if defined(CONFIG_REALTEK_INT_MICOM)
					//rtd_outl(REG_MAGIC_51, WAKE_8051);
#else
					rtd_outl(REG_MAGIC_51, WAKE_8051);
#endif
					act_self_refresh();
				}
				else if(0){
						printk("%s(%u)restart by EJ\n",__func__,__LINE__);
	        soft_restart(virt_to_phys(cpu_resume));
				}
				else{
            printk("%s(%u)SKIP act_self_refresh\n",__func__,__LINE__);
#if defined(CONFIG_REALTEK_INT_MICOM)
            //rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);
#else
            rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);
#endif
				}
		}
		else{
#if defined(CONFIG_REALTEK_INT_MICOM)
        //rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);
	//				rtd_outl(REG_MAGIC_51, 0x9021affc);
#else
        rtd_outl(STB_ST_SRST1_reg, _BIT9 | _BIT0);
					rtd_outl(REG_MAGIC_51, 0x9021affc);
#endif
        printk("%s(%u)ecpu reset release done\n",__func__,__LINE__);
		}
#if defined(CONFIG_REALTEK_INT_MICOM)
//		printk("INTMICOM_COMMAND_reg = %x\n", rtd_inl(INTMICOM_COMMAND_reg));
//		printk("INTMICOM_DATA_reg = %x\n", rtd_inl(INTMICOM_DATA_reg));
//		printk("INTMICOM_CTRL_reg = %x\n", rtd_inl(INTMICOM_CTRL_reg));
		printk("[b8060104] = %x\n", rtd_inl(0xb8060104));

//    while(rtd_inl(INTMICOM_CTRL_reg) == 0x0000ffff){}

    //cal wati scpu times
    if(((rtd_inl(0xb8060150) >> 24) & 0xff) == 0)
    {
        rtd_maskl(0xb8060150, 0x00FFFFFF, 0x05000000); //default wait max 5 times
    }
    printk("set wait scpu times= 0x%x\n", ((rtd_inl(0xb8060150) >> 24) & 0xff));
    
	// To Inform 8051 to entry power off state
//	rtd_outl(INTMICOM_DATA_reg, 0x00000093);
//	rtd_outl(INTMICOM_COMMAND_reg, ( SH_MEM_WRT_ID_SCPU  <<   SH_MEM_CMMD_WRT_ID_POS )
//                                | ( SH_MEM_RD_ID_8051   <<   SH_MEM_CMMD_RD_ID_POS )
//                                | ( SH_MEM_OP_WRITE  <<   SH_MEM_CMMD_OP_POS )
//                                | (  0x02 << SH_MEM_CMMD_LENGTH_POS ));
//       shareMemoryScpuInfo8051();
//	printk("INTMICOM_COMMAND_reg = %x\n", rtd_inl(INTMICOM_COMMAND_reg));
//	printk("INTMICOM_DATA_reg = %x\n", rtd_inl(INTMICOM_DATA_reg));
//	printk("INTMICOM_CTRL_reg = %x\n", rtd_inl(INTMICOM_CTRL_reg));
#endif
#if !defined (CONFIG_OPTEE)    
	while(1);
#endif
}

void osc_tracking(void)
{
#if !defined(CONFIG_REALTEK_INT_MICOM)
    int i;
    unsigned char k;
    u32 track_val;
    u32 tmp = 0;

    tmp = (rtd_inl(0xb8060004) & ~(_BIT21|_BIT20));
    rtd_outl(0xb8060004, tmp|(_BIT0<<20));
    track_val = 0;
    k = 0;
    for(i=0;i<(0x01<<k);i++){
        rtd_outl(0xb80600E4, rtd_inl(0xb80600E4)|_BIT0);    //OSC_Tracking_en enable
        mdelay(10);
        rtd_outl(0xb80600E4, rtd_inl(0xb80600E4)&~_BIT0);   //OSC_Tracking_en disable
        tmp = (rtd_inl(0xb80600E8) & (0x7f<<12))>>12;
        track_val += tmp;
    }
    //printk("last track_val=%x\n",tmp);
    //printk("avg  track_val=%x\n",track_val>>k);

    //rtd_outl(0x00E4, rtd_inl(0x00E4)&~_BIT0);
    //rtd_outl(0x00E4, 0x240002);
    //rtd_outl(0x00E4, (((UINT32)0x24)<<16)|_BIT1);
#endif
}

bool write_phy_command(unsigned int address, unsigned int data)
{
	int m=10;
	int n=5;
	unsigned int mask = data & _BIT31;
	while(m>0)
	{
		n=5;
		rtd_outl(address,data);
		while(n>0)
		{
			if((rtd_inl(address)&_BIT31) != mask)
				return true;
			mdelay(1);
			n--;
		}
		mdelay(1);
		m--;
	}



	return false;
}

void rtkpm_get_param(void){
#if !defined(CONFIG_REALTEK_INT_MICOM)
#ifdef CONFIG_RTK_KDRV_IR
	ir_suspend_protocol();
#endif
#endif
#ifdef CONFIG_REALTEK_WOW

	printk("WOW enable\n");
    if(CHK_WOW(GET_EVENT_MSG()))
	{
		unsigned char mac_arry[6];
		#ifdef USE_SETTING_NET_PARAM
		GET_HOST_MAC(mac_arry);
		#else
		mac_arry[0]=0x11;
		mac_arry[1]=0x22;
		mac_arry[2]=0x33;
		mac_arry[3]=0x44;
		mac_arry[4]=0x55;
		mac_arry[5]=0x66;
		#endif
		printk("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                *(mac_arry+0),*(mac_arry+1),*(mac_arry+2),
                *(mac_arry+3),*(mac_arry+4),*(mac_arry+5));

		rtd_outl(0xb8020028,0x00002001); //YPPADC STB_LATCH
		mdelay(1);


		write_phy_command(0xb801685c,0x841f0012);

		write_phy_command(0xb801685c,0x84100000 |(((u32)mac_arry[1])<<8) | (u32)mac_arry[0]);

		write_phy_command(0xb801685c,0x84110000 |(((u32)mac_arry[3])<<8) | (u32)mac_arry[2]);

		write_phy_command(0xb801685c,0x84120000 |(((u32)mac_arry[5])<<8) | (u32)mac_arry[4]);

		write_phy_command(0xb801685c,0x841f0000);

#ifdef USE_GETN_10M
        write_phy_command(0xb801685c,0x84000100);    // getn 10Mhz
#else
        write_phy_command(0xb801685c,0x84002100);    // getn 100Mhz
#endif

		write_phy_command(0xb801685c,0x841f0011);
		write_phy_command(0xb801685c,0x84101000); //magic picket enable
		write_phy_command(0xb801685c,0x84119fff);// reset wol
		write_phy_command(0xb801685c,0x84111fff);// set receive packet bytes

		write_phy_command(0xb8016c00,0x00000001); //ISO_LATCH
	}
#endif


}
#if !defined(CONFIG_REALTEK_INT_MICOM)
int download_8051_imem(const u8 *buf, size_t len)
{
    int i;
    unsigned int value;
    //unsigned long long power_key;

    printk("%s(%d)Len:0x%x\n",__func__,__LINE__,(unsigned int)len);

    len = ((len + 3) / 4) * 4;

    /* check bin file if big than 16k */
    if(len > IMAGE_SIZE){
        printk("8051 bin code big than 16k!!\n");
        printk("skip download!!!\n");
        return -1;
    }

    /*
     * reference rtd294x_DesignSpec_MISC.doc
     * disable all interrupt of scpu
     */
    // mark first for rtd294x different with macarthur
    //rtd_outl(SCPU_INT_EN,0xFE);

    /*
     * reference MacArthur3-DesignSpec-CRT.doc
     * For the second time load 8051 code successfully,
     * we have to reset every time
     */
    {   //assert EMCU reset
    rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~_BIT9);           //clock off
    rtd_outl(STB_ST_SRST1_reg,  rtd_inl(STB_ST_SRST1_reg) & ~(_BIT9|_BIT10));    //reset bit9 and bit10
    rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg)| _BIT9);              //clock on
    }

    {   //de-assert EMCU reset
    rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~_BIT9);           //clock off
    rtd_outl(STB_ST_SRST1_reg,  rtd_inl(STB_ST_SRST1_reg) | (_BIT9|_BIT10));     //de-assert reset
    rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg)| _BIT9);              //clock on
    }

    rtd_outl(STB_ST_CLKEN1_reg, rtd_inl(STB_ST_CLKEN1_reg) & ~_BIT9);   // set CLKEN_EMCU_CORE = 0 ; 0xB8060044[9] = 0;
    rtd_outl(STB_ST_SRST1_reg,  rtd_inl(STB_ST_SRST1_reg) & ~_BIT9);    // set RSTN_EMCU_CORE =0 ; 0xB8060034[9] = 0;

    rtd_outl(EMCU_IMEM_control_reg, _BIT14);            //Set IMEM address to be reload
    rtd_outl(EMCU_IMEM_control_reg, (_BIT14|_BIT15));   //auto increase cur_imem_addr +4 when write imem_data
    rtd_outl(EMCU_IMEM_control_reg, _BIT14);            //Set IMEM address to be reload
    rtd_outl(EMCU_system_CPU_program_enable_reg, 0x1);

    for (i=0; i<(len/4); i++){
        value = (((unsigned int) buf[3 + i*4])<<24) |
                (((unsigned int) buf[2 + i*4])<<16) |
                (((unsigned int) buf[1 + i*4])<< 8) |
                buf[0+ i*4];
        /*
        value = (((unsigned int) buf[0 + i*4])<<24)  |
                (((unsigned int) buf[1+ i*4])<<16) |
                (((unsigned int) buf[2+ i*4])<<8) |
                buf[3+ i*4];
        */
        /*
        if(i%4 == 0)
            printk("\n");
        printk("%08x ",value);
        */

        rtd_outl(EMCU_IMEM_data_reg, value);
    }
    rtd_outl(EMCU_system_CPU_program_enable_reg, 0);
    printk("\nwrite imem done\n");

    //Readback IMEM for checking
    rtd_outl(EMCU_IMEM_control_reg, _BIT14);            //Set IMEM address to be reload
    rtd_outl(EMCU_IMEM_control_reg, (_BIT14|_BIT15));   //auto increase cur_imem_addr +4 when write imem_data
    rtd_outl(EMCU_IMEM_control_reg, _BIT14);            //Set IMEM address to be reload
    rtd_outl(EMCU_system_CPU_program_enable_reg, 0x1);

    for (i=0; i<(len/4); i++) {
        value = (((unsigned int) buf[3 + i*4])<<24) |
                (((unsigned int) buf[2 + i*4])<<16) |
                (((unsigned int) buf[1 + i*4])<< 8) |
                buf[0+ i*4];
        /*
        value = (((unsigned int) buf[0 + i*4])<<24) |
                (((unsigned int) buf[1+ i*4])<<16) |
                (((unsigned int) buf[2+ i*4])<<8) |
                buf[3+ i*4];
        */
        /*
        if(i%4 == 0)
            printk("\n");
        printk("%08x(%08x) ",value,rtd_inl(EMCU_IMEM_DATA));
        */
        if( rtd_inl(EMCU_IMEM_data_reg)  != value)
        {
            printk(" I_mem compare fail\n");
            return -1;
        }
    }
    rtd_outl(EMCU_system_CPU_program_enable_reg, 0x0);  //CPU_PGM_EN
    printk("\ncheck imem ok\n");
    return 0;
}
#endif
struct file *pm_openFile(char *path,int flag,int mode)
{
    struct file *fp;

    fp = filp_open(path, flag, 0);
    if( IS_ERR(fp))
        return NULL;
    else
        return fp;

}

int pm_readFile(struct file *fp,char *buf,int readlen)
{
    if (fp->f_op && fp->f_op->read){
        printk("%s(%u)\n",__func__,__LINE__);
        return fp->f_op->read(fp,buf,readlen, &fp->f_pos);
    }else{
        printk("%s(%u)\n",__func__,__LINE__);
        return -1;
    }
}

int pm_closeFile(struct file *fp)
{
    filp_close(fp,NULL);
    return 0;
}

int rtk_pm_load_8051(int flag)
{

    printk("\n%s(%u)2013/07/09 11:30\n",__func__,__LINE__);
    local_irq_disable();
    if(flag == SUSPEND_BOOTCODE)
    {
        printk("bootcode call suspend.\n");
    }else if((flag == SUSPEND_RAM )||
             (flag == SUSPEND_WAKEUP))
    {
#if !defined(CONFIG_REALTEK_INT_MICOM)
        unsigned int pm_event_msg;
        unsigned int stm_value;

        pm_event_msg = GET_EVENT_MSG();

        if(flag == SUSPEND_WAKEUP){
            stm_value = STM_WAKEUP;
        }else{
            stm_value = CHK_STM(pm_event_msg);
            if(!stm_value)
                stm_value = STM_NOEMAL;
        }

        SET_EVENT_IDX(pm_event_msg,loc_stm,stm_value);
        SET_EVENT_MSG(pm_event_msg);
        printk("Suspend to RAM action...(0x%08x)\n",pm_event_msg);
#else
        printk("Suspend to RAM action...\n");
#endif

    }else{
        printk("system call suspend.\n");
    }

    printk("load bin file from DDR\n");
    #ifndef CONFIG_TV005
    rtkpm_get_param();
    #endif
    // download_8051_imem((u8*)phys_to_virt(DDR_EMCU_ADDR), IMAGE_SIZE);


/* Calculate DDR checksum for make sure STR flow DDR data is not corrupt */
#if 1
	if((rtd_inl(STB_WDOG_DATA14_reg) == 0xfafefafe) && (flag == SUSPEND_RAM))
		calculate_DDR_checksum();
#endif

    wakeup_8051(flag);

    return 0;
}

static int rtk_pm_load_8051_code(char *s)
{
    printk("\n%s(%u)2013/04/30 15:00\n",__func__,__LINE__);
    #ifndef CONFIG_TV005
    rtk_pm_load_8051(0);
    #endif
    return 0;
}

static int rtk_pm_act_8051_code(char *s){
    printk("\n%s(%u)2013/04/08 11:50\n",__func__,__LINE__);
    #ifdef CONFIG_TV005
    // init IrDA
    rtd_outl(ISO_IRDA_IR_PSR_VADDR ,0x5a13133b);
    rtd_outl(ISO_IRDA_IR_PER_VADDR ,0x1162d);
    rtd_outl(ISO_IRDA_IR_SF_VADDR, 0xff00021b);
    rtd_outl(ISO_IRDA_IR_DPIR_VADDR ,0x1f4);
    rtd_outl(ISO_IRDA_IR_CR_VADDR ,0x5df);
    // load 8051
    rtk_pm_load_8051(0);
    #endif
    return 0;
}

static int rtk_pm_8051_infile(char *s){
    printk("\n%s(%u)2013/04/30 16:00\n",__func__,__LINE__);
    emcu_read_file = 1;
    return 0;
}

#if defined(CONFIG_REALTEK_INT_MICOM)
#if 1 //def CONFIG_CUSTOMER_TV006
/******  share memory section ***************/

//** please make sure check this with internal micom setting****/
RTK_GPIO_ID shareMemoryScpuGpio = rtk_gpio_id(ISO_GPIO, 18);
RTK_GPIO_ID shareMemory8051Gpio = rtk_gpio_id(ISO_GPIO, 19);

void shareMemoryScpuReleaseBus(void)
{

	rtk_gpio_set_dir(shareMemoryScpuGpio , 0 );
	rtk_gpio_output(shareMemoryScpuGpio , 1 );
}

void shareMemoryScpuInfo8051(void)
{
	rtd_outl(INTMICOM_CTRL_reg , 0x0000FFFF );

        // interrupt to EMCU
        rtd_outl(EMCU_CPU_INT_EN2_reg, (_BIT1 | _BIT0));
        rtd_outl(EMCU_CPU_INT2_reg, (_BIT1 | _BIT0));
}


int do_intMicomShareMemory(UINT8 *pBuf,UINT8 Len, bool RorW)
{
	int i;
	unsigned int timeoutCnt;
	unsigned int regdata;
	unsigned int command;

	//int ret = 0 ;
	// First check data lenght whether overflow or not.
	if( Len > INTMICOM_SHARE_MEMORY_SPACE_BYTES) {
		INT_MCU_WARN("share memoy length fail \n");
		return SH_MEM_DATALEN_OVERFLOW;
	}

	/** prepare section****/
	if( RorW == SH_MEM_OP_READ )  {			// For read operation
		command = 0 | ( SH_MEM_WRT_ID_SCPU  <<   SH_MEM_CMMD_WRT_ID_POS )
				| ( SH_MEM_RD_ID_8051   <<   SH_MEM_CMMD_RD_ID_POS )
				| ( SH_MEM_OP_READ  <<   SH_MEM_CMMD_OP_POS )
				| (  (Len & 0xFF ) <<   SH_MEM_CMMD_LENGTH_POS )  ;
	}
	else if( RorW == SH_MEM_OP_WRITE ) { 			// for write operation
		command = 0 | ( SH_MEM_WRT_ID_SCPU  <<   SH_MEM_CMMD_WRT_ID_POS )
				| ( SH_MEM_RD_ID_8051   <<   SH_MEM_CMMD_RD_ID_POS )
				| ( SH_MEM_OP_WRITE  <<   SH_MEM_CMMD_OP_POS )
				| (  (Len & 0xFF ) <<   SH_MEM_CMMD_LENGTH_POS )  ;
	}
	else {															// invalid operation
		INT_MCU_WARN("unkown command \n");
		return SH_MEM_UNKNOWN_CMD;
	}
/** prepare section end****/

	// Check whether emcu release done or not.
        preempt_disable();
        timeoutCnt = 50000;
        while(1)
        {
            timeoutCnt++;
            if( rtd_inl(INTMICOM_CTRL_reg)  == 0x00000000)
                break;

            if(timeoutCnt % 1000000 == 0){
                printk("Wait Micom shared memory over %u us!\n", timeoutCnt);
                printk("INTMICOM_COMMAND_reg[0xB8060500]:%x\n", rtd_inl(INTMICOM_COMMAND_reg));
                printk("INTMICOM_DATA_reg[0xB8060504]:%x\n", rtd_inl(INTMICOM_DATA_reg));
                printk("INTMICOM_DATA_reg[0xB8060508]:%x\n", rtd_inl(INTMICOM_DATA_reg+4));
                printk("INTMICOM_DATA_reg[0xB806050c]:%x\n", rtd_inl(INTMICOM_DATA_reg+8));
                printk("INTMICOM_CTRL_reg[0xb8060574]:%x\n", rtd_inl(INTMICOM_CTRL_reg));
                printk("Micom debug register 1 [0xb806014C]:%x\n", rtd_inl(0xb806014C));
                printk("Micom debug register 2 [0xb8060154]:%x\n", rtd_inl(0xb8060154));
                preempt_enable();
                return SH_MEM_NO_RESPONSE;
            }
            udelay(1);
        }
        preempt_enable();

	if( timeoutCnt == 0 ) {
		INT_MCU_WARN("%s 8501 no response\n" , __func__ );
		return SH_MEM_NO_RESPONSE;
	}

	rtd_outl( INTMICOM_COMMAND_reg , command );
	//INT_MCU_WARN("command = 0x%08X \n" , rtd_inl( INTMICOM_COMMAND_reg ) );


	/**read command back**/
	if( RorW == SH_MEM_OP_READ ) {
                if(w_flag)
                {
                    pBuf[0]=0xff;
                    pBuf[1]=0xff;
                    pBuf[2]=0xff;
		    
                    w_flag=0;
                    return SH_MEM_OK;
                }
                else{
                    g_no_irda = 0;
		    shareMemoryScpuInfo8051();
                }
		// Check whether emcu release done or not.
		timeoutCnt = 0;

              preempt_disable();
              while(1)
		{
                    timeoutCnt++;
                    if( rtd_inl(INTMICOM_CTRL_reg)	== 0x00000000)
				break;

                    if(timeoutCnt % 1000000 == 0){
                        printk("Wait Micom shared memory over %u us!\n", timeoutCnt);
                        printk("INTMICOM_COMMAND_reg[0xB8060500]:%x\n", rtd_inl(INTMICOM_COMMAND_reg));
                        printk("INTMICOM_DATA_reg[0xB8060504]:%x\n", rtd_inl(INTMICOM_DATA_reg));
                        printk("INTMICOM_DATA_reg[0xB8060508]:%x\n", rtd_inl(INTMICOM_DATA_reg+4));
                        printk("INTMICOM_DATA_reg[0xB806050c]:%x\n", rtd_inl(INTMICOM_DATA_reg+8));
                        printk("INTMICOM_CTRL_reg[0xb8060574]:%x\n", rtd_inl(INTMICOM_CTRL_reg));
                        printk("Micom debug register 1 [0xb806014C]:%x\n", rtd_inl(0xb806014C));
                        printk("Micom debug register 2 [0xb8060154]:%x\n", rtd_inl(0xb8060154));
                        preempt_enable();
                        return SH_MEM_NO_RESPONSE;
                    }
                    udelay(1);
		}
              preempt_enable();

		for( i = 0 ; i < Len ; i++ ) {
			if((i % 4) == 0) {
				regdata = rtd_inl( INTMICOM_DATA_reg + ( ( i /4 )	<< 2 ) );
				pBuf[i] = ( regdata >> 0 )& 0xFF;
                                                     	
			}
			else {
				pBuf[i] = (regdata >> ((i%4) * 8))& 0xFF;
			}
		}

                // if the irda key event from micom is single key, set the flag; If we get the corresponding release key, clear it.
                if(g_no_irda == 0)
                {
                    if(pBuf[0] == SINGLE_KEY)
                        single_key = 1;
                    else if(pBuf[0] == RELEASE_KEY)
                        single_key = 0;
                }
	}
	else {
		if(pBuf[0] == 0xc0)  {
                if((rtd_inl(INTMICOM_CHECK_IRDA_reg)  == 0x0 &&(g_no_irda < 15)) && (single_key == 0)){
				w_flag = 1;
				g_no_irda++;
				return SH_MEM_OK;
			}
			g_no_irda = 0;
		}
		for( i = 0 ; i < Len ; i++ ) {
			if((i % 4) == 0) {
				regdata = pBuf[i] & 0xFF;
       			}
			else {
				regdata |= ((pBuf[i] & 0xFF) << ((i%4) * 8));
			}

			if((i % 4) == 3) {
				rtd_outl( INTMICOM_DATA_reg + ((i /4 ) * 4), regdata);
			}
		}

		if(i % 4) {
			rtd_outl( INTMICOM_DATA_reg + (((i - 1) /4 ) * 4), regdata);
		}
		shareMemoryScpuInfo8051();
	}

	/**read command back end **/
	return SH_MEM_OK;
}

EXPORT_SYMBOL(do_intMicomShareMemory);

/******  share memory section ***************/
#endif
#endif



__setup("POWERDOWN",rtk_pm_load_8051_code);
__setup("ac_on",    rtk_pm_act_8051_code);
__setup("51_file",  rtk_pm_8051_infile);

/* end of file */
