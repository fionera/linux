/*
 *
 *	VBI v4l2 structure header file
 *
 *  from customer
 */
#ifndef _VBI_V4L2_STRUCT_H
#define _VBI_V4L2_STRUCT_H
#define VBI_V4L2_MAIN_DEVICE_NAME "vbi1" //main scaler /tmp use
#if 0
/* User-class control Bases */
/* User-class control Bases */
//#define V4L2_CID_USER_EXT_VBI_BASE (V4L2_CID_USER_BASE + 0x5000)
#define V4L2_CTRL_CLASS_USER		0x00980000	/* Old-style 'user' controls */
#define V4L2_CID_BASE			(V4L2_CTRL_CLASS_USER | 0x900)
#define V4L2_CID_USER_BASE 		V4L2_CID_BASE
#define V4L2_CID_USER_EXT_VBI_BASE (V4L2_CID_USER_BASE + 0x9000)
#define V4L2_CID_EXT_VBI_COPY_PROTECTION_INFO (V4L2_CID_USER_EXT_VBI_BASE + 0)
#endif
//**************************************************************************//
//*************************** control id ***********************************//
//**************************************************************************//
/* VBI class control IDs */
//#define V4L2_CID_EXT_VBI_BASE							(V4L2_CID_USER_EXT_VBI_BASE)
//#define V4L2_CID_EXT_VBI_COPY_PROTECTION_INFO			(V4L2_CID_EXT_VBI_BASE + 0)
#if 0
/* CGMS copy protection enum */
enum v4l2_ext_vbi_cgms {
    V4L2_EXT_VBI_CGMS_PERMIT = 0,
    V4L2_EXT_VBI_CGMS_ONCE,
    V4L2_EXT_VBI_CGMS_RESERVED,
    V4L2_EXT_VBI_CGMS_NO_PERMIT,
};

/* APS copy protection enum */
enum v4l2_ext_vbi_aps {
    V4L2_EXT_VBI_APS_OFF = 0,
    V4L2_EXT_VBI_APS_ON_BURST_OFF,
    V4L2_EXT_VBI_APS_ON_BURST_2,
    V4L2_EXT_VBI_APS_ON_BURST_4,
};

/* MACROVISION copy protection enum */
enum v4l2_ext_vbi_macrovision {
    V4L2_EXT_VBI_MACROVISION_PSP_OFF = 0,
    V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_OFF,
    V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_2,
    V4L2_EXT_VBI_MACROVISION_PSP_ON_BURST_4,
};

struct v4l2_ext_vbi_copy_protection {
    enum v4l2_ext_vbi_cgms        cgms_cp_info;
    enum v4l2_ext_vbi_aps         aps_cp_info;
    enum v4l2_ext_vbi_macrovision macrovision_cp_info;
};
#endif
#endif
