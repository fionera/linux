/*this file is used to :
1. keep dcmt old api to avoid compile and run time error
2. define some common global vari for all hw-monitor
*/
#include <mach/io.h>
#include "rtk_dc_mt.h"
#include "rtk_dc_mt_config.h"

//warning: DCMT not support this API any more...
int __maybe_unused _dc_mt_set_parameter(unsigned char i,DC_MT_DESC* p_desc)
{
	return -1;
}
EXPORT_SYMBOL(_dc_mt_set_parameter);

//warning: DCMT not support this API any more...
int __maybe_unused _dc_mtex_set_parameter(unsigned char i,DC_MT_DESC* p_desc)
{
	return -1;
}
EXPORT_SYMBOL(_dc_mtex_set_parameter);

//warning: DCMT not support this API any more...
void __maybe_unused _dc_mt_unset_mem_region(int i,unsigned char is_store)
{
	return;
}
EXPORT_SYMBOL(_dc_mt_unset_mem_region);

#ifdef DCMT_DC2
//warning: DCMT not support this API any more...
int __maybe_unused _dc2_mt_set_parameter(unsigned char i,DC_MT_DESC* p_desc)
{
	return -1;
}
EXPORT_SYMBOL(_dc2_mt_set_parameter);

//warning: DCMT not support this API any more...
int __maybe_unused _dc2_mtex_set_parameter(unsigned char i,DC_MT_DESC* p_desc)
{
	return -1;
}
EXPORT_SYMBOL(_dc2_mtex_set_parameter);

//warning: DCMT not support this API any more...
void __maybe_unused _dc2_mt_unset_mem_region(int i,unsigned char is_store)
{
	return;
}
EXPORT_SYMBOL(_dc2_mt_unset_mem_region);
#endif // #ifdef DCMT_DC2

#ifndef CONFIG_RTK_KDRV_DC_MEMORY_TRASH_DETCTER
void __maybe_unused set_monitor_redirect_code_DCU1_GPU(int index, unsigned int addr, unsigned int size) {
	return;
}
EXPORT_SYMBOL(set_monitor_redirect_code_DCU1_GPU);

#ifdef DCMT_DC2
void __maybe_unused set_monitor_redirect_code_DCU2_GPU(int index, unsigned int addr, unsigned int size) {
	return;
}
EXPORT_SYMBOL(set_monitor_redirect_code_DCU2_GPU);
#endif //#ifdef DCMT_DC2

#endif

