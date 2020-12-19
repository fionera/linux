#ifndef _LINUX_RTKAUDIO_DEBUG_H
#define _LINUX_RTKAUDIO_DEBUG_H


#define RTD_PRINT_AUDIO_EANBLE

#ifdef RTD_PRINT_AUDIO_EANBLE
#include <mach/rtk_log.h>

#undef pr_debug
#undef pr_info
#undef pr_notice
#undef pr_warning
#undef pr_err
#undef pr_crit
#undef pr_alert
#undef pr_emerg

#define TAG_NAME "ADSP"

#define pr_debug(format, args...)   rtd_printk(KERN_DEBUG, TAG_NAME, format, ## args)
#define pr_info(format, args...)    rtd_printk(KERN_INFO, TAG_NAME, format, ## args)
#define pr_notice(format, args...)  rtd_printk(KERN_NOTICE, TAG_NAME, format, ## args)
#define pr_warning(format, args...) rtd_printk(KERN_WARNING, TAG_NAME, format, ## args)
#define pr_err(format, args...)     rtd_printk(KERN_ERR, TAG_NAME, format, ## args)
#define pr_crit(format, args...)    rtd_printk(KERN_CRIT, TAG_NAME, format, ## args)
#define pr_alert(format, args...)   rtd_printk(KERN_ALERT, TAG_NAME, format, ## args)
#define pr_emerg(format, args...)   rtd_printk(KERN_EMERG, TAG_NAME, format, ## args)
#endif

enum DUMP_ID {
	DUMP_STOP = 0,
	DUMP_LOG = 1,
	DUMP_PP_FOCUS,
	DUMP_AO_PCM,
	DUMP_AO_RAW
};

enum LGSE_FN_DEBUG {
    DEBUG_LGSE_FN000 = 0,
    DEBUG_LGSE_FN001 = 1,
    DEBUG_LGSE_FN004 = 2,
    DEBUG_LGSE_FN005 = 3,
    DEBUG_LGSE_FN008 = 4,
    DEBUG_LGSE_FN009 = 5,
    DEBUG_LGSE_FN010 = 6,
    DEBUG_LGSE_FN014 = 7,
    DEBUG_LGSE_FN016 = 8,
    DEBUG_LGSE_FN017 = 9,
    DEBUG_LGSE_FN019 = 10,
    DEBUG_LGSE_FN022 = 11,
    DEBUG_LGSE_FN023 = 12,
    DEBUG_LGSE_FN024 = 13,
    DEBUG_LGSE_FN026 = 14,
    DEBUG_LGSE_FN028 = 15,
    DEBUG_LGSE_FN029 = 16,
    DEBUG_LGSE_MAIN  = 17,
    DEBUG_LGSE_FN_MAX
};

enum LGSE_DAP_DEBUG {
    DEBUG_LGSE_POSTPROCESS                 = 0,
    DEBUG_LGSE_DAP_SURROUNDVIRTUALIZERMODE = 1,
    DEBUG_LGSE_DAP_DIALOGUEENHANCER        = 2,
    DEBUG_LGSE_DAP_VOLUMELEVELER           = 3,
    DEBUG_LGSE_DAP_VOLUMEMODELER           = 4,
    DEBUG_LGSE_DAP_VOLUMEMAXIMIZER         = 5,
    DEBUG_LGSE_DAP_OPTIMIZER               = 6,
    DEBUG_LGSE_DAP_PROCESSOPTIMIZER        = 7,
    DEBUG_LGSE_DAP_SURROUNDDECODER         = 8,
    DEBUG_LGSE_DAP_SURROUNDCOMPRESSOR      = 9,
    DEBUG_LGSE_DAP_VIRTUALIZERSPEAKERANGLE = 10,
    DEBUG_LGSE_DAP_INTELLIGENCEEQ          = 11,
    DEBUG_LGSE_DAP_MEDIAINTELLIGENCE       = 12,
    DEBUG_LGSE_DAP_GRAPHICALEQ             = 13,
    DEBUG_LGSE_DAP_PERCEPTUALHEIGHTFILTER  = 14,
    DEBUG_LGSE_DAP_BASSENHANCER            = 15,
    DEBUG_LGSE_DAP_VIRTUALBASS             = 16,
    DEBUG_LGSE_DAP_REGULATOR               = 17,
    DEBUG_LGSE_DAP_MAX
};

int rtkaudio_dump_enable(unsigned int db_command);
void rtkaudio_dump_disable(unsigned int db_command);
void rtkaudio_send_string(const char* pattern, int length);
#endif
