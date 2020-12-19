#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/io.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <lgtv_driver_proc/procfs.h>
#include <tvscalercontrol/vip/ypbpr_auto.h>
#include <tvscalercontrol/scaler/scalerstruct.h>

#ifdef CONFIG_KDRIVER_USE_NEW_COMMON
	#include <scaler/scalerCommon.h>
#else
	#include <scalercommon/scalerCommon.h>
#endif

extern StructDisplayInfo * Get_ADC_Dispinfo(void);
extern unsigned short Get_ADC_PhaseValue(void);
extern void ADC_Get_GainOffset(ADCGainOffset *adcinfo);
extern unsigned char get_adc_input_source(void);
extern unsigned char get_adc_calibration_type(void);
extern unsigned char get_adc_fast_switch_status(void);

static int v4l2_get_adc_status(void* private_data, char* buff, unsigned int buff_size)
{
    int len, total = 0;
	unsigned char timing_scan_type = 0;
	StructDisplayInfo *p_dispinfo = NULL;
	ADCGainOffset rtk_adc_info;

	//---------------------------------------------------------------------
    // Title info
    //---------------------------------------------------------------------
    len = snprintf(buff, buff_size, "===== ADC STATUS =====\n");
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // General info
    //---------------------------------------------------------------------
    //len = snprintf(buff, buff_size, "open=%d\n", 1);
    //total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "input_src=%d\n", get_adc_input_source());
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // Timing info
    //---------------------------------------------------------------------
	p_dispinfo = Get_ADC_Dispinfo();
	if(!p_dispinfo) return 0;

	timing_scan_type = Scaler_GetDispStatusFromSource(p_dispinfo, SLR_DISP_INTERLACE);

	len = snprintf(buff, buff_size, "timing_h_freq=%d\n", p_dispinfo->IHFreq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_freq=%d\n", p_dispinfo->IVFreq);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_h_total=%d\n", p_dispinfo->IHTotal);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_total=%d\n", p_dispinfo->IVTotal << timing_scan_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_h_porch=%d\n", p_dispinfo->IPH_ACT_STA_PRE);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_v_porch=%d\n", p_dispinfo->IPV_ACT_STA_PRE << timing_scan_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_x=%d\n", p_dispinfo->IPH_ACT_STA_PRE);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_y=%d\n", p_dispinfo->IPV_ACT_STA_PRE);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_w=%d\n", p_dispinfo->IPH_ACT_WID_PRE);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_active_h=%d\n", p_dispinfo->IPV_ACT_LEN_PRE << timing_scan_type);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "timing_scan_type=%d\n", timing_scan_type ? 0 : 1); //0 : Interlaced, 1 : Progressive  LGE SPEC
    total += len; buff_size-=len; buff+= len;

    //---------------------------------------------------------------------
    // Packet info
    //---------------------------------------------------------------------
	len = snprintf(buff, buff_size, "phase=%d\n", Get_ADC_PhaseValue());
    total += len; buff_size-=len; buff+= len;

	ADC_Get_GainOffset(&rtk_adc_info);//ADC gain and offset

    len = snprintf(buff, buff_size, "r_gain=%d\n", rtk_adc_info.Gain_R);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "g_gain=%d\n", rtk_adc_info.Gain_G);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "b_gain=%d\n", rtk_adc_info.Gain_B);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "r_offset=%d\n", rtk_adc_info.Offset_R);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "g_offset=%d\n", rtk_adc_info.Offset_G);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "b_offset=%d\n", rtk_adc_info.Offset_B);
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "calibration_type=%d\n", get_adc_calibration_type());
    total += len; buff_size-=len; buff+= len;

    len = snprintf(buff, buff_size, "fast_switch_mode=%d\n", get_adc_fast_switch_status());
    total += len; buff_size-=len; buff+= len;

	return total;
}


///////////////////////////////////////////////////////////////////
// Module init/exit
///////////////////////////////////////////////////////////////////

static int lgtv_adc_proc_init(void)
{
    create_lgtv_adc_status_proc_entry("adc", v4l2_get_adc_status, (void*) 0);

    return 0;
}

late_initcall(lgtv_adc_proc_init);
