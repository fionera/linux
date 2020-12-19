#ifndef __LGTV_DRIVER_PROCFS_H__
#define __LGTV_DRIVER_PROCFS_H__

#include <common/linux/linuxtv-ext-ver.h>

#define LGTV_DRIVER_PROC_DIR           "lgtv-driver"
#define LGTV_EXTERNAL_INPUT_PROC_DIR   "external-input"
#define LGTV_EXTERNAL_INPUT_PROC_DIR_E "lgtv-driver/external-input"
#define LGTV_STATUS_PROC_ENTRY_HDMI1   "hdmi1"
#define LGTV_STATUS_PROC_ENTRY_HDMI2   "hdmi2"
#define LGTV_STATUS_PROC_ENTRY_HDMI3   "hdmi3"
#define LGTV_STATUS_PROC_ENTRY_HDMI4   "hdmi4"

#define LGTV_HDMI_STATUS_PROC_DIR      "hdmi_status"
#define LGTV_HDMI_STATUS_PROC_DIR_E    "lgtv-driver/hdmi_status"

#define LGTV_ADC_STATUS_PROC_DIR      "adc_status"
#define LGTV_ADC_STATUS_PROC_DIR_E    "lgtv-driver/adc_status"

#define LGTV_AVD_STATUS_PROC_DIR      "avd_status"
#define LGTV_AVD_STATUS_PROC_DIR_E    "lgtv-driver/avd_status"

///////////////////////////////////////////////////////////////////////////////////
// LGTV External input Procfs
///////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    struct file_operations f_ops;
    struct proc_dir_entry* pde;
    int (*proc_read)(void* private_data, char* buff, unsigned int buff_size);
    void* private_data;
}EXT_INPUT_PROC_ENTY;


extern EXT_INPUT_PROC_ENTY* create_lgtv_external_input_status_proc_entry(const char* name, int (*proc_read)(void* private_data, char* buff, unsigned int buff_size), void* data);

extern EXT_INPUT_PROC_ENTY* create_lgtv_hdmi_status_proc_entry(const int port, int (*proc_read)(void* private_data, char* buff, unsigned int buff_size), void* data);

extern EXT_INPUT_PROC_ENTY* create_lgtv_adc_status_proc_entry(const char* name, int (*proc_read)(void* private_data, char* buff, unsigned int buff_size), void* data);

extern EXT_INPUT_PROC_ENTY* create_lgtv_avd_status_proc_entry(const char* name, int (*proc_read)(void* private_data, char* buff, unsigned int buff_size), void* data);

#endif // __LGTV_DRIVER_PROCFS_H__
