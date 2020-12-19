/*
 *
 *	VBE v4l2 related api header file
 *
 *  drievr vbe internal api from vsc device
 */
#ifndef _VBE_V4L2_API_H
#define _VBE_V4L2_API_H

struct vbe_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
};

//v4l2 ioctrl callback api
int vbe_v4l2_main_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap);
int vbe_v4l2_main_ioctl_s_ctrl(struct file *file, void *fh,	struct v4l2_control *ctrls);
int vbe_v4l2_main_ioctl_s_ext_ctrl(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vbe_v4l2_main_ioctl_g_ext_ctrl(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);

//vbe internal api
extern void HAL_VBE_DISP_SetDisplayOutput(BOOLEAN bOnOff);
extern void HAL_VBE_DISP_SetMute(BOOLEAN bOnOff);
extern unsigned char HAL_VBE_DISP_Initialize(KADP_DISP_PANEL_INFO_T panelInfo);
extern unsigned char HAL_VBE_DISP_Resume(KADP_DISP_PANEL_INFO_T panelInfo);
extern void HAL_VBE_DISP_SetSpreadSpectrum(BOOLEAN bEnable, UINT16 u16Percent, UINT16 u16Period);
extern unsigned char HAL_VBE_DISP_SetVideoMirror(BOOLEAN bIsH,BOOLEAN bIsV);
extern void HAL_VBE_DISP_SetInnerPattern(UINT8 bOnOff, VBE_DISP_INNER_PTG_BLOCK_T block, VBE_DISP_INNER_PTG_TYPE_T type);
extern void HAL_VBE_DISP_VCOMPatternCtrl(KADP_DISP_PANEL_VCOM_PAT_CTRL_T nCtrl);
extern void HAL_VBE_DISP_VCOMPatternDraw(UINT16 *vcomPattern,UINT16 nSize);

extern void HAL_VBE_DISP_OLED_SetOrbit(BOOLEAN bEnable, unsigned char mode);



extern void HAL_VBE_DISP_SetBOERGBWBypass(BOOLEAN bEnable);
extern void HAL_VBE_DISP_SetMLEMode(enum v4l2_ext_vbe_mplus_mode index);
extern void HAL_VBE_DISP_SetBOEMode(unsigned char mode);

extern void HAL_VBE_DISP_SetFrameGainLimit(UINT16 nFrameGainLimit);
extern void HAL_VBE_DISP_GetFrameGainLimit (UINT16 *nFrameGainLimit);

extern void HAL_VBE_DISP_SetPixelGainLimit (UINT16 nPixelGainLimit);
extern void HAL_VBE_DISP_GetPixelGainLimit (UINT16 *nPixelGainLimit);

extern void HAL_VBE_DISP_MplusSet(UINT16 * pRegisterSet, UINT8 nPanelMaker);
extern void HAL_VBE_DISP_MplusGet(UINT16 * pRegisterGet, UINT8 nPanelMaker);



extern void HAL_VBE_SetDGA4CH (UINT32 *pRedGammaTable, UINT32 *pGreenGammaTable, UINT32 *pBlueGammaTable, UINT32 *pWhiteGammaTable, UINT16 nTableSize);
extern void fwif_color_set_fcic_TV006(unsigned int *u32pTSCICTbl, unsigned int u32Tscicsize, unsigned char *u8pControlTbl, unsigned int u32Ctrlsize, unsigned char bCtrl);
extern void HAL_VBE_TSCIC_Load(UINT32 *u32pTSCICTbl, UINT32 u32Tscicsize, UINT8 *u8pControlTbl, UINT32 u32Ctrlsize);


#endif
