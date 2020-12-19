#ifndef _RTK_WOV_H
#define _RTK_WOV_H

#include <linux/module.h>
#include <linux/kernel.h>

#define EMMC_BLOCK_SIZE       (PAGE_SIZE / 8)
#define WOV_DUMP_DATA_DDR_START_ADDR		0x4f000000 //DMIC Data phy addr
#define WOV_AP_DDR_PHYSICAL_START_ADDR		0x53000000 //wovap usage, same with bootcode CONFIG_STANDALONE_LOAD_ADDR
#define WOV_DB_DDR_PHYSICAL_START_ADDR		0x53300000  //wovdb usage, can adjust this address by different project. this is within VBM curved out range
#define WOV_AP_DDR_DEFAULT_RECORD_LENGTH	0x20000	//128KB
#define WOV_DUMP_4CH_PREPROCESS_DATA_ADDR		0x52300000 	//preprocessed data address
#define HOT_WORDS_START_reg         0x1b300004                 //a fake register to feedback hot words start position
#define HOT_WORDS_END_reg           (HOT_WORDS_START_reg+0x4)  //a fake register to feedback hot words end position
#define MODEL_TYPE_reg              (HOT_WORDS_END_reg+0x4)    //a fake register to feedback model is eft or ft

//process.c
extern int wov_backup_ddr(void);
extern int is_wakeup_from_wov(int op);

//misc.c
bool updateSensoryModel(const char* pStrModelFilePath);
void dumpSensoryModel(void);

//pm.c
int wov_load_uboot_db(void);
int wov_save_file(void);
int wov_backup_ddr(void);
int is_wakeup_from_wov(int op);
int wov_release_ddr(void);
int wov_get_ddr_ptr(char **ptr);
int wov_load_uboot_ap(void);

//misc.c and pm.c
extern int lgemmc_get_partnum(const char *name);
extern char *lgemmc_get_partname(int partnum);
extern unsigned int lgemmc_get_partsize(int partnum);
extern unsigned int lgemmc_get_filesize(int partnum);
extern unsigned long long lgemmc_get_partoffset(int partnum);
extern int mmc_fast_read(unsigned int, unsigned int, unsigned char *);
extern int mmc_fast_write(unsigned blk_addr, unsigned data_size, unsigned char *buffer);


#endif //_RTK_WOV_H

