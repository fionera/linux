#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/types.h>
#include <linux/unistd.h>
#include "rdvb_acas_debug.h"


#define RDVB_ACAS_PROC_ENTRY "acasdebug"
#define RDVB_MAX_CMD_LENGTH 256

enum {
	ACAS_DBG_CMD_APDU_LOG_ON_OFF = 0,
	ACAS_DBG_CMD_ATR_LOG_ON_OFF  = 1,
	ACAS_DBG_CMD_NUMBER
} ACAS_DBG_CMD_INDEX_T;

/* debug comment
** apdu log on/off: to enable/disable print apdu tx and rx log
   printd rx/tx log:  echo "apduLogOnOff=1" > /proc/acas0/apduLogOnOff
   disable print   :  echo "apduLogOnOff=0" > /proc/acas/acasdebug

** atr log on/off: to enable/disable print atr log
   printd atr log:    echo "atrLogOnOff=1" > /proc/acas/acasdebug
   disable print   :  echo "atrLogOnOff=0" > /proc/acas/acasdebug
*/
static const char *g_acas_cmd_str[] = {
	"apduLogOnOff=",			/* ACAS_DBG_CMD_APDU_LOG_ON_OFF */
	"atrLogOnOff=",                        /* ACAS_DBG_CMD_ATR_LOG_ON_OFF */
};

struct ACAS_DBG_SETTING g_dbg_setting;

void rdvb_acas_debug_execute(unsigned char chip, const int cmd_index, char **cmd_pointer);

void acas_apdu_tx_dump(unsigned char* apdu, int len)
{
    if(g_dbg_setting.apdu_log_flag){
    	int i;
    	printk("apdu tx cmd:");
    	for(i = 0; i < len; i++)
    		printk(" %x", apdu[i]);
    	printk("\n");
    }
}
void acas_apdu_rx_dump(unsigned char* apdu, int len)
{
    if(g_dbg_setting.apdu_log_flag){
    	int i;
    	printk("apdu rx data:");
    	for(i = 0; i < len; i++)
    		printk(" %x", apdu[i]);
    	printk("\n");
    }
}
void acas_atr_dump(unsigned char* atr, int len)
{
    if(g_dbg_setting.atr_log_flag){
    	int i;
    	printk("atr data:");
    	for(i = 0; i < len; i++)
    		printk(" %x", atr[i]);
    	printk("\n");
    }
}

static ssize_t rdvb_acas_proc_write(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
	int cmd_index;
	char *cmd_pointer = NULL;
	char str[RDVB_MAX_CMD_LENGTH] = {0};
	unsigned char chip = (unsigned char)(unsigned long)file->private_data;;

	if (buf == NULL) {
		return -EFAULT;
	}
	if (count > RDVB_MAX_CMD_LENGTH - 1)
		count = RDVB_MAX_CMD_LENGTH - 1;
	if (copy_from_user(str, buf, RDVB_MAX_CMD_LENGTH - 1)) {
		return -EFAULT;
	}

	if (count > 0)
		str[count-1] = '\0';
	printk("acas_debug:%d acas:%d >>%s\n", __LINE__,chip, str);


	for (cmd_index = 0; cmd_index < ACAS_DBG_CMD_NUMBER; cmd_index++) {
		if (strncmp(str, g_acas_cmd_str[cmd_index], strlen(g_acas_cmd_str[cmd_index])) == 0)
			break;
	}


	if (cmd_index >= ACAS_DBG_CMD_NUMBER)
		return count;

	cmd_pointer = &str[strlen(g_acas_cmd_str[cmd_index])];

	rdvb_acas_debug_execute(chip, cmd_index, &cmd_pointer);

	return count;
}

static const struct file_operations g_acas_proc_fops[1] = {
	{
	.owner = THIS_MODULE,
	.write = rdvb_acas_proc_write,
	},
};

bool rdvb_acas_proc_init(void)
{
	static const char* acas_dir = "acas";

	if (g_dbg_setting.proc_dir != NULL || g_dbg_setting.proc_entry != NULL)
		return -1;

	g_dbg_setting.proc_dir = proc_mkdir(acas_dir , NULL);

	if (g_dbg_setting.proc_dir == NULL) {
		printk("create proc entry (%s) failed\n", acas_dir);
		return -1;
	}

	g_dbg_setting.proc_entry = proc_create(RDVB_ACAS_PROC_ENTRY, 0666,
					g_dbg_setting.proc_dir, &g_acas_proc_fops[0]);

	if (g_dbg_setting.proc_entry == NULL) {
		proc_remove(g_dbg_setting.proc_dir);
		g_dbg_setting.proc_dir = NULL;
		printk("failed to get proc entry for %s/%s\n",
					acas_dir, RDVB_ACAS_PROC_ENTRY);
		return -1;
	}

	return 0;
}

void rdvb_acas_proc_uninit(void)
{

	if (g_dbg_setting.proc_entry) {
		proc_remove(g_dbg_setting.proc_entry);
		g_dbg_setting.proc_entry = NULL;
	}


	if (g_dbg_setting.proc_dir) {
		proc_remove(g_dbg_setting.proc_dir);
		g_dbg_setting.proc_dir = NULL;
	}
}


/* use IS_ERR(x) to check the (struct file *) */
void rdvb_acas_debug_create_file(const char *filename, struct file **handle)
{
	*handle = filp_open(filename, O_RDWR | O_CREAT | O_TRUNC, 0777);
}

void rdvb_acas_debug_close_file(struct file *handle)
{
	filp_close(handle, NULL);
}

static inline bool debug_parse_value(char *cmd_pointer, long long *parsed_data)
{
	if (kstrtoll(cmd_pointer, 0, parsed_data) == 0) {
		return 0;
	} else {
		printk("debug:%d parsing data failed. pCmd=%s\n", __LINE__, cmd_pointer);
		return -1;
	}
}

int rdvb_acas_debug_write_file(struct file *handle, char *buffer, unsigned int size, loff_t *pOffset)
{
	int ret = 0;

	if (handle && pOffset) {
		mm_segment_t fs;
		fs = get_fs();
		set_fs(get_ds());    /* set to KERNEL_DS */
		vfs_write(handle, buffer, size, pOffset);
		set_fs(fs);
	}

	return ret;
}

void rdvb_acas_debug_execute(unsigned char chip, const int cmd_index, char **cmd_pointer)
{
	long long parsed_data = 0;

	switch (cmd_index) {
	case ACAS_DBG_CMD_APDU_LOG_ON_OFF:
		debug_parse_value(*cmd_pointer, &parsed_data);
		g_dbg_setting.apdu_log_flag = parsed_data;
		printk("apdu_log_flag=%ld\n", g_dbg_setting.apdu_log_flag);
		break;
	case ACAS_DBG_CMD_ATR_LOG_ON_OFF:
		debug_parse_value(*cmd_pointer, &parsed_data);
		g_dbg_setting.atr_log_flag = parsed_data;
		printk("atr_log_flag=%ld\n", g_dbg_setting.atr_log_flag);
		break;
	default:
		return;
	}
    return;
}