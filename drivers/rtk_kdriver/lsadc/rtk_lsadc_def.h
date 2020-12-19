/**
 **
 */

#ifndef _RTK_LSADC_DEF_H_
#define _RTK_LSADC_DEF_H_

#include <rbus/rbus_types.h>
#include <rtk_kdriver/rtk_crt.h>
#include <rbus/lsadc_reg.h>




#define  PAD_0_CONTROL          LSADC_ST_pad0_reg
#define  PAD_1_CONTROL          LSADC_ST_pad1_reg
#define  PAD_2_CONTROL          LSADC_ST_pad2_reg
#define  PAD_3_CONTROL          LSADC_ST_pad3_reg
#define  PAD_4_CONTROL          LSADC_ST_pad4_reg
#define  PAD_5_CONTROL          LSADC_ST_pad5_reg
#define  PAD_6_CONTROL          LSADC_ST_pad6_reg
#define  PAD_7_CONTROL          LSADC_ST_pad7_reg
#define  PAD_8_CONTROL          LSADC_ST_pad8_reg
#define  PAD_9_CONTROL          LSADC_ST_pad9_reg
#define  PAD_10_CONTROL         LSADC_ST_pad10_reg

#define  LSADC_CTRL             LSADC_ST_LSADC_ctrl_reg
#define  LSADC_STATUS           LSADC_ST_LSADC_status_reg
#define  ANALOG_CTRL            LSADC_ST_LSADC_analog_ctrl_reg

#define  PAD1_COMPARE_SET0      LSADC_LSADC_pad1_level_set0_reg
#define  PAD1_COMPARE_SET1      LSADC_LSADC_pad1_level_set1_reg
#define  PAD1_COMPARE_SET2      LSADC_LSADC_pad1_level_set2_reg
#define  PAD1_COMPARE_SET3      LSADC_LSADC_pad1_level_set3_reg
#define  PAD1_COMPARE_SET4      LSADC_LSADC_pad1_level_set4_reg
#define  PAD1_COMPARE_SET5      LSADC_LSADC_pad1_level_set5_reg
#define  PAD2_COMPARE_SET0      LSADC_LSADC_pad2_level_set0_reg
#define  PAD2_COMPARE_SET1      LSADC_LSADC_pad2_level_set1_reg
#define  PAD2_COMPARE_SET2      LSADC_LSADC_pad2_level_set2_reg
#define  PAD2_COMPARE_SET3      LSADC_LSADC_pad2_level_set3_reg
#define  PAD2_COMPARE_SET4      LSADC_LSADC_pad2_level_set4_reg
#define  PAD2_COMPARE_SET5      LSADC_LSADC_pad2_level_set5_reg
#define  PAD3_COMPARE_SET0      LSADC_LSADC_pad3_level_set0_reg
#define  PAD3_COMPARE_SET1      LSADC_LSADC_pad3_level_set1_reg
#define  PAD3_COMPARE_SET2      LSADC_LSADC_pad3_level_set2_reg
#define  PAD3_COMPARE_SET3      LSADC_LSADC_pad3_level_set3_reg
#define  PAD3_COMPARE_SET4      LSADC_LSADC_pad3_level_set4_reg
#define  PAD3_COMPARE_SET5      LSADC_LSADC_pad3_level_set5_reg
#define  PAD1_COMPARE_STATUS    LSADC_LSADC_INT_pad1_reg
#define  PAD2_COMPARE_STATUS    LSADC_LSADC_INT_pad1_reg
#define  PAD3_COMPARE_STATUS    LSADC_LSADC_INT_pad1_reg

/************************************************/

typedef struct _rtk_lsdac_reg_map_t {
	unsigned long padCtrl;
	unsigned long lsadc_ctrl;
	unsigned long status;
	unsigned long analog_ctrl;
	unsigned long padCompare0;
	unsigned long padCompare1;
	unsigned long padCompare2;
	unsigned long padCompare3;
	unsigned long padCompare4;
	unsigned long padCompare5;
	unsigned long compareStatus;
} rtk_lsdac_reg_map ;


typedef struct _rtk_lsadc_phy_t {
	const rtk_lsdac_reg_map *p_reg_map;
} rtk_lsadc_phy;




static const rtk_lsdac_reg_map lsadc_0_reg = {
	 .padCtrl       = PAD_0_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};

static const rtk_lsdac_reg_map lsadc_1_reg = {
	 .padCtrl       = PAD_1_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};


static const rtk_lsdac_reg_map lsadc_2_reg = {
	 .padCtrl       = PAD_2_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};



static const rtk_lsdac_reg_map lsadc_3_reg = {
	 .padCtrl       = PAD_3_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};



static const rtk_lsdac_reg_map lsadc_4_reg = {
	 .padCtrl       = PAD_4_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};



static const rtk_lsdac_reg_map lsadc_5_reg = {
	 .padCtrl       = PAD_5_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD1_COMPARE_SET0,
	 .padCompare1   = PAD1_COMPARE_SET1,
	 .padCompare2   = PAD1_COMPARE_SET2,
	 .padCompare3   = PAD1_COMPARE_SET3,
	 .padCompare4   = PAD1_COMPARE_SET4,
	 .padCompare5   = PAD1_COMPARE_SET5,
	 .compareStatus = PAD1_COMPARE_STATUS,
};


static const rtk_lsdac_reg_map lsadc_6_reg = {
	 .padCtrl       = PAD_6_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD2_COMPARE_SET0,
	 .padCompare1   = PAD2_COMPARE_SET1,
	 .padCompare2   = PAD2_COMPARE_SET2,
	 .padCompare3   = PAD2_COMPARE_SET3,
	 .padCompare4   = PAD2_COMPARE_SET4,
	 .padCompare5   = PAD2_COMPARE_SET5,
	 .compareStatus = PAD2_COMPARE_STATUS,
};


static const rtk_lsdac_reg_map lsadc_7_reg = {
	 .padCtrl       = PAD_7_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
	 .padCompare0   = PAD3_COMPARE_SET0,
	 .padCompare1   = PAD3_COMPARE_SET1,
	 .padCompare2   = PAD3_COMPARE_SET2,
	 .padCompare3   = PAD3_COMPARE_SET3,
	 .padCompare4   = PAD3_COMPARE_SET4,
	 .padCompare5   = PAD3_COMPARE_SET5,
	 .compareStatus = PAD3_COMPARE_STATUS,
};

static const rtk_lsdac_reg_map lsadc_8_reg = {
	 .padCtrl       = PAD_8_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
};

static const rtk_lsdac_reg_map lsadc_9_reg = {
	 .padCtrl       = PAD_9_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
};

static const rtk_lsdac_reg_map lsadc_10_reg = {
	 .padCtrl       = PAD_10_CONTROL,
	 .lsadc_ctrl    = LSADC_CTRL,
	 .status        = LSADC_STATUS,
	 .analog_ctrl   = ANALOG_CTRL,
};



static const rtk_lsadc_phy lsadcArray[] = {

	{&lsadc_0_reg},
	{&lsadc_1_reg},
	{&lsadc_2_reg},
	{&lsadc_3_reg},
	{&lsadc_4_reg},
	{&lsadc_5_reg},
	{&lsadc_6_reg},
	{&lsadc_7_reg},
	{&lsadc_8_reg},
	{&lsadc_9_reg},
	{&lsadc_10_reg},
};


/*
typedef enum {
	PCB_LSADC_TYPE_VOLTAGE,
	PCB_LSADC_TYPE_CURRENT,
	PCB_LSADC_TYPE_UNDEF,
} PCB_LSADC_TYPE_T;
*/


#endif


