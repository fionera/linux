#include "ir_input.h"

static struct venus_key tcl_jp_tv_keys[] = {

    // customize for jp
    {0x53, KEY_3_LINE_INPUT},
    {0x59, KEY_D_DATA_BML},
    {0x30, KEY_SETTING},
    {0x71, KEY_GUIDE},
    {0x55, KEY_TERMINAL},
    {0x66, KEY_SATELLITE_BS},
    {0x5E, KEY_SATELLITE_CS},
    {0xE8, KEY_RECORD},
    {0x5D, KEY_RECORD_LIST},
    {0x7E, KEY_MORE_INFO},
    {0x45, KEY_PREVIOUS},
    {0xAC, KEY_NEXT},
    {0xEA, KEY_PLAY},
    {0xE6, KEY_PAUSE},
    {0xE2, KEY_FAST_BACK},
    {0xE3, KEY_FAST_FORWARD},
    {0xE3, KEY_STOP},
    {0x1C, KEY_T_LAUNCH},
    {0x5A, KEY_TIME},
    {0x5B, KEY_T_SCHEDULE},

    // common for tcl
    {0x10, KEY_NETFLIX},
    {0x7F, KEY_SUBTITLE},
    {0xF7, KEY_HOME},
    {0x0c, KEY_AV1},  // AV1
    {0x0d, KEY_AV2},  // AV2
    {0x01, KEY_ZOOMOUT},    // CMD_FAVCH
    {0x12, KEY_GLOBO_PLAY},   // CMD_MENU
    {0xe0, KEY_NETFLIX_STOP},
    {0xd5, KEY_POWER},
    {0xc0, KEY_MUTE},//tcl
    {0x1d, KEY_YOUTUBE},
    {0x20, KEY_LANG },
    {0xe1, KEY_TEXT },
    {0x21, KEY_LAST_CHANNEL},
    {0x32, KEY_NUMBER},
    {0x45, KEY_NETFLIX_FORWARD},  // CMD_POP
    {0x46, KEY_TV_ON},
    {0x47, KEY_TV_OFF},
    {0x48, KEY_HDMI_1},
    {0x49, KEY_HDMI2},
    {0x4a, KEY_HDMI3},
    {0x4b, KEY_HDMI4},
    {0x4c, KEY_CMP},
    {0x4d, KEY_VGA},
    {0xF1, KEY_APP_CAREVISION}, //APP_CAREVISION
    {0x55, KEY_TCL_TUNER},
    {0x61, KEY_ZOOMIN},
    {0x66, KEY_MTS },//mts
    {0x6c, KEY_RC_DOT}, //3D_MODE
    {0x9e, KEY_LIST},
    {0xc3, KEY_REPLY},    // CMD_DISPLAY tcl
    {0xe2, KEY_NETFLIX_FB},
    {0xe3, KEY_NETFLIX_FF},
    {0xe6, KEY_PAUSECD},
    {0xe8, KEY_MEDIA_RECORD_AEHA},
    {0xea, KEY_MEDIA_PLAY},
    {0xfd, KEY_USB_MEDIA},      // (C)USB
    {0x50, KEY_0},
    {0xce, KEY_1},
    {0xcd, KEY_2},
    {0xcc, KEY_3},
    {0xcb, KEY_4},
    {0xca, KEY_5},
    {0xc9, KEY_6},
    {0xc8, KEY_7},
    {0xc7, KEY_8},
    {0xc6, KEY_9},
    {0x51, KEY_11},
    {0x52, KEY_12},
    {0xd2, KEY_CHANNELUP},
    {0xd3, KEY_CHANNELDOWN},
    {0xd0, KEY_VOLUMEUP},   // CMD_VOL_UP
    {0xd1, KEY_VOLUMEDOWN},     // CMD_VOL_DOWN
    {0x0b, KEY_ENTER},  // CMD_ENTER
    {0xa6, KEY_UP},
    {0xa7, KEY_DOWN},
    {0xa8, KEY_RIGHT},
    {0xa9, KEY_LEFT},
    {0xf7, KEY_HOME},   // CMD_GUIDE tcl
    {0xd8, KEY_BACK},   // CMD_RETURN
    {0xff, KEY_RED},    // CMD_OPTION_RED
    {0x17, KEY_GREEN},  // CMD_OPTION_GREEN
    {0x1b, KEY_YELLOW}, // CMD_OPTION_YELLOW
    {0x27, KEY_BLUE},   // CMD_OPTION_BLUE
    {0xe5, KEY_EPG},    // CMD_EPG
    {0x13, KEY_MENU},   // CMD_MENU
    {0xf8, KEY_SLEEP},
    {0xc3, KEY_DISPLAY},    // CMD_DISPLAY tcl
    {0x6f, KEY_DISPLAY_RATIO},//tcl
    {0xf9, KEY_EXIT},
    { 0x67, KEY_3D_MODE_AEHA}, //3D_MODE
    { 0x62, KEY_MENU},//menu
    { 0xc5, KEY_TV},//tv
    { 0xd8, KEY_CHANNEL},
    { 0xa5, KEY_AUDIO },      // (C)SOUND_MODE
    { 0x6f, KEY_ZOOM },       // (C)ASPECT RATIO
    { 0xf3, KEY_FN_F1 },      // (C)FREEZE
    { 0xfd, KEY_FN_F2 },      // (C)USB
    { 0xa0, KEY_RF_LINK_S },
    { 0xa3, KEY_RC_RF_UNCONNECT },
    { 0xaa, KEY_RF_LINK_F},
    { 0x5c, KEY_TV_INPUT_AEHA}, //CMD_SOURCE
    { 0x30, KEY_SET},//set
    { 0x1c, KEY_QUICK_MENU},//QUICK_MENU
    { 0x19, KEY_GLOBAL_PLAY},//GLOBAL_PLAY
    { 0xab, KEY_RF_LINK_STOP},
    { 0xac, KEY_NETFLIX_NEXT},
    { 0xed, KEY_PICTURE },     // (C)PICTURE_MODE
};
struct  venus_key_table tcl_jp_tv_key_table = {
    .keys = tcl_jp_tv_keys,
    .size = ARRAY_SIZE(tcl_jp_tv_keys),
};


//end add by TCL_FACTORY
static struct venus_key tcl_tv_keys[] = {

    {0xd5, KEY_POWER},
    {0xc0, KEY_MUTE},//tcl
    {0xce, KEY_1},
    {0xcd, KEY_2},
    {0xcc, KEY_3},
    //{0x40, KEY_CHANNEL_INC},  // CMD_CHANNEL_INC
    {0xd2, KEY_CHANNELUP},
    {0xcb, KEY_4},
    {0xca, KEY_5},
    {0xc9, KEY_6},
    //{0x5d, KEY_CHANNEL_DEC},  // CMD_CHANNEL_DEC
    {0xd3, KEY_CHANNELDOWN},
    {0xc8, KEY_7},
    {0xc7, KEY_8},
    {0xc6, KEY_9},
    {0xd0, KEY_VOLUMEUP},   // CMD_VOL_UP
    //{0xd1, KEY_DELETE},
    {0xcf, KEY_0},
    {0x0b, KEY_ENTER},  // CMD_ENTER
    {0xd1, KEY_VOLUMEDOWN},     // CMD_VOL_DOWN
    //{0x08, KEY_TOOLS},    // CMD_TOOLS
    {0xa6, KEY_UP},
    {0xa7, KEY_DOWN},
    {0xa9, KEY_LEFT},
    {0xa8, KEY_RIGHT},
    //{0x46, KEY_PIP},  // CMD_PIP
    //{0x45, KEY_POP},  // CMD_POP
    {0xf7, KEY_HOME},   // CMD_GUIDE tcl
    //{0x41, KEY_MP},   // CMD_MP
    //{0x52, KEY_JUMP}, // CMD_JUMP
    {0xf9, KEY_BACK},   // CMD_RETURN
    {0xff, KEY_RED},    // CMD_OPTION_RED
    {0x17, KEY_GREEN},  // CMD_OPTION_GREEN
    {0x1b, KEY_YELLOW}, // CMD_OPTION_YELLOW
    {0x27, KEY_BLUE},   // CMD_OPTION_BLUE
    //{0x01, KEY_FAVCH},    // CMD_FAVCH
    {0xe5, KEY_EPG},    // CMD_EPG
    //{0x49, KEY_PROGINFO}, // CMD_PROGINFO
    {0x13, KEY_MENU},   // CMD_MENU
    {0x43, KEY_SLEEP},
    {0xc3, KEY_DISPLAY},    // CMD_DISPLAY tcl
    {0x6f, KEY_DISPLAY_RATIO},//tcl
    //{0x1a, KEY_WIDE}, // CMD_WIDE
    //{0x54, KEY_CAPTION},  // CMD_CAPTION
    //{0x17, KEY_VIDEO},
    //{0x16, KEY_AUDIO},
    //{0xd8, KEY_EXIT},
    //{0x05, KEY_ROI},  // CMD_ROI
    //{0x04, KEY_FREEZE},   // CMD_FREEZE
    //{0x00, KEY_CARD_READER},  // CMD_CARD_READER


    { 0x67, KEY_3D_MODE_TCL}, //3D_MODE

    { 0x62, KEY_MENU},//menu
    { 0xc5, KEY_TV},//tv


    { 0xd8, KEY_CHANNEL},
    { 0xa5, KEY_AUDIO },      // (C)SOUND_MODE
    { 0x6f, KEY_ZOOM },       // (C)ASPECT RATIO
    { 0xf3, KEY_FN_F1 },      // (C)FREEZE
    { 0xfd, KEY_FN_F2 },      // (C)USB
    { 0xa0, KEY_RF_LINK_S },
    { 0xa3, KEY_RC_RF_UNCONNECT },
    { 0xaa, KEY_RF_LINK_F},
    {0x5c, KEY_TV_INPUT_TCL}, //CMD_SOURCE
    {0x30, KEY_SET},//set
    {0x1c, KEY_QUICK_MENU},//QUICK_MENU
    {0x19, KEY_GLOBAL_PLAY},//GLOBAL_PLAY
    {0xed, KEY_PICTURE },     // (C)PICTURE_MODE
};
struct  venus_key_table tcl_tv_key_table = {
    .keys = tcl_tv_keys,
    .size = ARRAY_SIZE(tcl_tv_keys),
};

static struct  venus_key tcl_tv_factory_keys[] = {
    {0x17,0x17},
    {0x16,0x16},
    {0x01,0x01},
    {0x02,0x02},
    {0x07,0x07},
    {0x08,0x08},
    {0x09,0x09},
    {0x0a,0x0a},
    {0x0b,0x0b},
    {0x05,0x05},
    {0x06,0x06},
    {0x0c,0x0c},
    {0x2d,0x2d},
    {0x3f,0x3f},
    {0x14,0x14},
    {0x15,0x15},
    {0x1d,0x1d},
    {0x18,0x18},
    {0x19,0x19},
    {0x3d,0x3d},
    {0x3e,0x3e},
    {0x14,0x14},
    {0x0d,0x0d},
    {0x12,0x12},
    {0x13,0x13},
    {0x1a,0x1a},
    {0x1b,0x1b},
    {0x16,0x16},
    {0x17,0x17},
    {0x47,0x47},
    {0x48,0x48},
    {0x29,0x29},
    {0x60,0x60},
    {0x61,0x61},
    {0x20,0x20},
    {0x2e,0x2e},
    {0x7f,0x7f},
    {0x28,0x28},
};
struct venus_key_table tcl_tv_factory_key_table = {
    .keys = tcl_tv_factory_keys,
    .size = ARRAY_SIZE(tcl_tv_factory_keys),
};

#define FactoryCustomerCodeMax      8
static unsigned int factory_customer_code_map[FactoryCustomerCodeMax] =
{
    5, 6, 7, 8, 9, 11, 12, 13
};

int is_factory_customer_code(unsigned int customer_code)
{
    int i = 0;
    for(i = 0 ; i < FactoryCustomerCodeMax; i ++){
        if(factory_customer_code_map[i] == customer_code)
            break;
    }
    if(i >= FactoryCustomerCodeMax){
        return 0;
    }else{
        return 1;
    }
}

