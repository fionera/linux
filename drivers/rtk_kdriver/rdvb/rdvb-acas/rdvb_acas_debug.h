#ifndef __RDVB_ACAS_DEBUG_H
#define __RDVB_ACAS_DEBUG_H

#include <linux/types.h>

/* FOR DEBUG*/
#define DGB_COLOR_RED_WHITE      "\033[1;31;47m"
#define DGB_COLOR_RED            "\033[1;31m"
#define DGB_COLOR_GREEN          "\033[1;32m"
#define DGB_COLOR_YELLOW         "\033[1;33m"
#define DGB_COLOR_BLUE           "\033[1;34m"
#define DGB_COLOR_PURPLE         "\033[1;35m"
#define DGB_COLOR_CYAN           "\033[1;36m"
#define DGB_COLOR_NONE           "\033[m"
#define DGB_COLOR_NONE_N         "\033[m \n"

#define ACAS_ERROR(fmt, args...)         printk(KERN_ERR "[ACAS] Error, " fmt, ## args)
#define ACAS_WARNING(fmt, args...)       printk(KERN_WARNING "[ACAS] Warn, " fmt, ## args)
#define ACAS_INFO(fmt, args...)          printk(KERN_INFO "[ACAS] Info, " fmt, ## args)
#define ACAS_DEBUG(fmt, args...)         printk(KERN_DEBUG "[ACAS] Debug, " fmt, ## args)

struct ACAS_DBG_SETTING {
	unsigned long apdu_log_flag;
	unsigned long atr_log_flag;
	struct proc_dir_entry *proc_dir;
	struct proc_dir_entry *proc_entry;
};
extern struct ACAS_DBG_SETTING g_dbg_setting;

void acas_apdu_tx_dump(unsigned char* apdu, int len);
void acas_apdu_rx_dump(unsigned char* apdu, int len);

void acas_atr_dump(unsigned char* atr, int len);

bool rdvb_acas_proc_init(void);
void rdvb_acas_proc_uninit(void);
void rdvb_acas_debug_execute(unsigned char chid, const int cmd_index, char **cmd_pointer);

#endif /* __RDVB_ACAS_DEBUG_H */
