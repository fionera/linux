#ifndef _OPTEE_MC_PGTABLE_H
#define _OPTEE_MC_PGTABLE_H

#define MC_VDEC_4K_MEMORY_REQUEST 0x1
#define MC_VDEC_2K_MEMORY_REQUEST 0x2
#define MC_VDEC_720P_MEMORY_REQUEST 0x4

#define MC_VDEC_4K_MAX_NUM 1
#define MC_VDEC_2K_MAX_NUM 5
#define MC_VDEC_720P_MAX_NUM 8

enum remap_module_id {
	MC_VDEC_VBM_4K_START_INDEX = 0,
	MC_VDEC_VBM_4K = MC_VDEC_VBM_4K_START_INDEX,
	MC_VDEC_VBM_2K_START_INDEX,
	MC_VDEC_VBM_2K_0 = MC_VDEC_VBM_2K_START_INDEX,
	MC_VDEC_VBM_2K_1,
	MC_VDEC_VBM_2K_2,
	MC_VDEC_VBM_2K_3,
	MC_VDEC_VBM_2K_4,
	MC_VDEC_VBM_720P_START_INDEX,
	MC_VDEC_VBM_720p_0 = MC_VDEC_VBM_720P_START_INDEX,
	MC_VDEC_VBM_720p_1,
	MC_VDEC_VBM_720p_2,
	MC_VDEC_VBM_720p_3,
	MC_VDEC_VBM_720p_4,
	MC_VDEC_VBM_720p_5,
	MC_VDEC_VBM_720p_6,
	MC_VDEC_VBM_720p_7,
	MC_VDEC_VBM_MAX_INDX = MC_VDEC_VBM_720p_7,
	MC_SCALER_MDOMAIN,
	MC_SCALER_OD,
	MC_SCALER_DI,
	MC_SCALER_ME,
	MC_SCALER_MC,
	MC_SCALER_I3DDMA,
	MC_SCALER_DEXC,
	MC_MODULE_MAX_NUM
};

struct mc_module_range {
	enum remap_module_id id;
	int entry_count;
	unsigned long va;
	int used;
};

int mc_pgtable_init(void);
void mc_pgtable_deinit(void);
int mc_pgtable_get_entries (enum remap_module_id module_id, unsigned long *pa_array);
int mc_pgtable_set_entries (enum remap_module_id module_id);
int mc_pgtable_unset_entries (enum remap_module_id module_id, unsigned long *pa_array);
int mc_pgtable_debug(int cmd);

#endif

