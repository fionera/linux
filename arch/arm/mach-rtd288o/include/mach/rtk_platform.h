#ifndef _RTK_PLATFORM_H
#define _RTK_PLATFORM_H

#include <linux/pageremap.h>
#include <mach/rtk_mem_layout.h>

typedef enum {
	/* address 512MB before */
	CARVEDOUT_BOOTCODE = 0,
	CARVEDOUT_DEMOD,        // device
	CARVEDOUT_AV_DMEM,
	CARVEDOUT_K_OS_1,
	CARVEDOUT_K_OS,
	CARVEDOUT_MAP_RBUS,
	CARVEDOUT_AV_OS,
	CARVEDOUT_MAP_RPC,
	CARVEDOUT_VDEC_RINGBUF, // device
	CARVEDOUT_LOGBUF,
	CARVEDOUT_ROMCODE,
	CARVEDOUT_IB_BOUNDARY,

	/* device */
	CARVEDOUT_GAL,
	CARVEDOUT_SNAPSHOT,
	CARVEDOUT_CMA_3,
	CARVEDOUT_SCALER,
#ifdef CONFIG_ARM64
    CARVEDOUT_SCALER_BAND,
#endif
	CARVEDOUT_SCALER_MEMC,
	CARVEDOUT_SCALER_MDOMAIN,
	CARVEDOUT_SCALER_DI_NR,
	CARVEDOUT_SCALER_NN,
	CARVEDOUT_SCALER_VIP,
	CARVEDOUT_SCALER_OD,
	CARVEDOUT_VDEC_VBM,
	CARVEDOUT_TP,

    /* reserve from cma-2 area */
    CARVEDOUT_VT,

    CARVEDOUT_DDR_BOUNDARY,

#ifdef CONFIG_ZONE_ZRAM
	CARVEDOUT_ZONE_ZRAM,
#endif

	/* for desired cma size calculation */
	CARVEDOUT_CMA_LOW,
	CARVEDOUT_CMA_HIGH,

#ifdef CONFIG_OPTEE_SUPPORT_MC_ALLOCATOR
	CARVEDOUT_CMA_MC,
#endif

	/* -- */
	CARVEDOUT_NUM
} carvedout_buf_t;

extern unsigned long carvedout_buf[][4];

enum PLAFTORM_TYPE
{
	PLATFORM_K2LP	= 0,
	PLATFORM_K2L	= 1,
	PLATFORM_K3LP	= 2,
	PLATFORM_K3L	= 3,
	PLATFORM_S4AP	= 4,
	PLATFORM_K4LP	= 5,
    PLATFORM_K5LP	= 6,
    PLATFORM_K5AP	= 7,
    PLATFORM_K6LP	= 8,
    PLATFORM_K6HP	= 9,
	PLATFORM_OTHER	= 255
};

#define PLATFORM_KXLP	PLATFORM_K6LP
#define PLATFORM_KXL	PLATFORM_K6HP

enum PLAFTORM_TYPE get_platform (void);
int is_DDR4(void);
unsigned long get_uart_clock(void);

unsigned long carvedout_buf_query(carvedout_buf_t what, void **addr);
int carvedout_buf_query_is_in_range(unsigned long in_addr, void **start, void **end);

/*****************************************
(Sam temp section)
attempt to remove
******************************************/
//*
typedef enum {
    direct=0,
    edgeled,
    oled
}webos_toolopt_backlight_type_t;

typedef enum {
    local_dim_block_6=0,
    local_dim_block_12,
    local_dim_block_32,
    local_dim_block_36
}webos_toolopt_local_dim_block_t;

typedef enum {
    local_dimming_off=0,
    local_dimming_on,
}webos_toolopt_local_dimming_t;

typedef enum {
    inch_22 = 0,
    inch_23,
    inch_24,
    inch_26,
    inch_27,
    inch_28,
    inch_32,
    inch_39,
    inch_40,
    inch_42,
    inch_43,
    inch_47,
    inch_49,
    inch_50,
    inch_55,
    inch_58,
    inch_60,
    inch_65,
    inch_70,

    //16Y Inch addition
    inch_75,

    inch_77,
    inch_79,
    inch_84,
    inch_86,
    inch_98,
    inch_105,

    //17Y Inch addition
    inch_48, // RMS_5446 Rookie TV

    //18Y Inch add list
    inch_82,
    inch_88,
    inch_BASE,
}webos_toolopt_inch_type_t;

typedef enum {
    tool_R9 = 0,    // 1
    tool_Z9,                // 2
    tool_W9,                // 3
    tool_E9,                // 4
    tool_C9,                // 5
    tool_B9,                // 6
    tool_B9S,               // 7
    tool_SM99,              // 8
    tool_SM95,              // 9
    tool_SM90,              // 10
    tool_SM85,              // 11
    tool_SM81,              // 12
    tool_UN77,              // 13
    tool_UN75,              // 14
    tool_UN73,              // 15
    tool_UN71,              // 16
    tool_NANO85,            // 17
    tool_LM63,              // 18
    tool_LM57,              // 19
    tool_LM636,             // 20
    tool_LM576,             // 21
    tool_B9_C,              // 22
    tool_UM77_IN,           // 23
    tool_C9_B,              // 24
    tool_UM75_120_US,       // 25
    tool_OBJ_UK75,          // 26
    tool_JDM_TPV,           // 27
    tool_W9S,               // 28
    tool_LCD_END,
    tool_PDP_END,
    tool_BASE,
}webos_toolopt_tool_type_t;

typedef enum {
    module_LGD = 0,
    module_AUO,
    module_SHARP,
    module_BOE,
    module_CSOT,
    module_INNOLUX,
    module_LGD_M,
    module_ODM_B,
    module_BOE_TPV,
    module_HKC,
    module_BOE_10P5,
    module_ODM_H,
    module_CHOT_JDM,
    module_LCD_END,
}webos_toolopt_maker_type_t;

typedef struct {
     webos_toolopt_backlight_type_t  eBackLight;
     webos_toolopt_local_dim_block_t eLEDBarType;
     webos_toolopt_local_dimming_t   bLocalDimming;
     webos_toolopt_inch_type_t       eModelInchType;
     webos_toolopt_tool_type_t       eModelToolType;
     webos_toolopt_maker_type_t     eModelModuleType;
} webos_info_t;

//*/
/*****************************************
(END Sam temp section)
******************************************/


typedef enum {
    str_local_dimming_off=0,
    str_local_dimming_on,
}webos_strToolOpt_local_dimming_t;

typedef struct {
	char 								eBackLight[8];
	char 								eLEDBarType[4];
	webos_strToolOpt_local_dimming_t 	bLocalDimming;
	char 								eModelInchType[4];
	char 								eModelToolType[9];
	char 								eModelModuleType[6];
} webos_strInfo_t;

#endif //_RTK_PLATFORM_H

