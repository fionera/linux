#include "rdvb_dmx_filtermgr.h"
#include "dmx_parse_context.h"
#include "rdvb_dmx_ctrl_internal.h"
#include "dmx_service.h"

extern struct rdvb_dmxdev * get_rdvb_dmxdev(void);

static ssize_t filter_list_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	return count;
}
static ssize_t filter_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rdvb_dmxdev * rdvb_dmxdev =  get_rdvb_dmxdev();
	int i  = 0;
	char *pbuf = buf;
	pbuf = print_to_buf(pbuf, "%s_%d @@@@#### dev:%s, att:%s \n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	for (i=0; i< rdvb_dmxdev->dmxnum; i++) {
		pbuf = print_to_buf(pbuf, "============@@@@@ CH %d @@@@@@=========\n", i);
		pbuf = rdvbdmx_filter_dump(&rdvb_dmxdev->rdvb_dmx[i].filter_list, pbuf);
	}
	return  pbuf - buf;
}

DEVICE_ATTR_RW(filter_list);

static ssize_t dmx_status_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	return count;
}
static ssize_t dmx_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rdvb_dmxdev * rdvb_dmxdev =  get_rdvb_dmxdev();
	SDEC_CHANNEL_T *sdec = NULL;
	int i  = 0;
	printk("%s_%d @@@@#### dev:%s, att:%s \n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	for (i=0; i< rdvb_dmxdev->dmxnum; i++)
	{
		sdec = (SDEC_CHANNEL_T *) rdvb_dmxdev->rdvb_dmx[i].dmxdev.demux;
		printk("ch:%d, status:%d avsyncMode:%d packetsize:%d ECP_MODE:%d\n",
			i, sdec->threadState, sdec->avSyncMode, sdec->tsPacketSize, sdec->is_ecp);
	}
	return  0;
}
DEVICE_ATTR_RW(dmx_status);


static ssize_t dmx_context_list_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	return count;
}
static ssize_t dmx_context_list_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rdvb_dmxdev * rdvb_dmxdev =  get_rdvb_dmxdev();
	SDEC_CHANNEL_T *sdec = NULL;
	int i  = 0;
	printk("%s_%d @@@@#### dev:%s, att:%s \n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	for (i=0; i< rdvb_dmxdev->dmxnum; i++) {
		sdec = (SDEC_CHANNEL_T *) rdvb_dmxdev->rdvb_dmx[i].dmxdev.demux;

		printk("%s_%d @@@@#### dmx:%d\n", __func__, __LINE__, sdec->idx);
		ts_parse_context_dump(&sdec->tpc_list_head);

	}
	return  0;
}
DEVICE_ATTR_RW(dmx_context_list);

static ssize_t log_mask_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long level = 0;
	printk("log Level: %s \n", buf);
	if (kstrtoll(buf, 0, &level) != 0) {
		return -1;
	}
	set_log_level(level);

	return count;
}

static ssize_t log_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%016x\n", get_log_level());
}
DEVICE_ATTR_RW(log_mask);
static ssize_t ecp_onoff_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long level = 0;
	printk("ecp on off : %s \n", buf);
	if (kstrtoll(buf, 0, &level) != 0) {
		return -1;
	}
	if (level == 0)
		dmx_service_off();
	else
		dmx_service_on();

	return count;
}

static ssize_t ecp_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%016x\n", get_log_level());
}
DEVICE_ATTR_RW(ecp_onoff);

extern bool dmx_ecp_status;
static ssize_t status_dump(char *buf)
{
	struct rdvb_dmxdev * rdvb_dmxdev =  get_rdvb_dmxdev();
	char *pbuf = buf;
	int i = 0;

	pbuf = print_to_buf(pbuf, "linuxtv-ext-headerver.%d.%d.%d (submissions/%d)\n\n",
			LINUXTV_EXT_VER_MAJOR, LINUXTV_EXT_VER_MINOR,
			LINUXTV_EXT_VER_PATCH, LINUXTV_EXT_VER_SUBMISSION);

	pbuf = print_to_buf(pbuf, "\nInput Port Status\n");
	pbuf = print_to_buf(pbuf, "Num Mode   Source     Total     Error      Drop\n");
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++)
	{
		struct rdvb_dmx *rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		SDEC_CHANNEL_T *sdec = (SDEC_CHANNEL_T *)rdvb_dmx->dmxdev.demux;
		pbuf = print_to_buf(pbuf, "[%d][%5s][%10s]", rdvb_dmx->index,
			rdvb_dmx->input_src_type < DMX_EXT_SRC_TYPE_CI ? "TS" :
			rdvb_dmx->input_src_type < DMX_EXT_SRC_TYPE_MEM ? "CI":
			rdvb_dmx->input_src_type < DMX_EXT_SRC_TYPE_NULL ? "MEM":
			"None",
			rdvb_dmx->input_port_type == DMX_EXT_PORT_TYPE_PARALLEL ?
			 "Parallel" : "Serial");

		pbuf = rdvb_dmx_ctrl_dump_tp_status(sdec, pbuf);
	}
	pbuf = print_to_buf(pbuf, "\nPES filter status(audio/video/teletext/subtitle)\n");
	pbuf = print_to_buf(pbuf, "idx   ch    PID         EN   PES   DEST\n");
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++)
	{
		struct list_head *header = &rdvb_dmxdev->rdvb_dmx[i].filter_list;
		pbuf = rdvbdmx_filter_dump_pes(header, pbuf);
	}

	pbuf = print_to_buf(pbuf, "\nPCR Recover Status\n");
	pbuf = print_to_buf(pbuf, "CH EN MAIN PID    STC      PCR      FREQ  TIME\n");
	//pbuf = rdvb->dmx_ctrl_dump_pcr_status(sdec, pbuf);
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++)
	{
		struct rdvb_dmx *rdvb_dmx = &rdvb_dmxdev->rdvb_dmx[i];
		SDEC_CHANNEL_T *sdec = (SDEC_CHANNEL_T *)rdvb_dmx->dmxdev.demux;
		pbuf = rdvb_dmx_ctrl_dump_pcr_status(sdec, pbuf);
	}

	pbuf = print_to_buf(pbuf, "\nSection filter list\n");
	pbuf = print_to_buf(pbuf, "idx   ch    PID         REG            EN   SDEC  GPB\n");
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++)
	{
		struct list_head *header = &rdvb_dmxdev->rdvb_dmx[i].filter_list;
		pbuf = rdvbdmx_filter_dump_sec(header, pbuf);
	}
	pbuf = print_to_buf(pbuf, "\nScrambler checker pid list\n");
	pbuf = print_to_buf(pbuf, " idx  ch    PID\n");
	for (i = 0; i < rdvb_dmxdev->dmxnum; i++)
	{
		struct list_head *header = &rdvb_dmxdev->rdvb_dmx[i].filter_list;
		pbuf = rdvbdmx_filter_dump_scrmbck(header, pbuf);
	}

	pbuf = print_to_buf(pbuf, "\nECP status\n");
	pbuf = print_to_buf(pbuf, "HW demuxer = %d (0 = hw demux, 1 = sw demux)\n"
			"ECP Mode(sw demux only) = %d\n",
			1, dmx_ecp_status);

	pbuf = print_to_buf(pbuf, "\nMax Support filter=%d\n", 96);
	return pbuf - buf;
}
static ssize_t status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return status_dump(buf);
}
static DEVICE_ATTR_RO(status);

DEFINE_MUTEX(mutex_test);
static ssize_t mutex_test_store(struct device * dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long level = 0;
	printk("mutext: %s \n", buf);
	if (kstrtoll(buf, 0, &level) != 0) {
		return -1;
	}

	if (level == 0){
		UNLOCK(&mutex_test);
	}
	else
		LOCK(&mutex_test);
	return count;
}
/*
static ssize_t mutext_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return -1;
}*/
DEVICE_ATTR_WO(mutex_test);

struct attribute * rdvb_dmx_debug_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_filter_list.attr,
	&dev_attr_dmx_status.attr,
	&dev_attr_dmx_context_list.attr,
	&dev_attr_log_mask.attr,
	&dev_attr_mutex_test.attr,
	&dev_attr_ecp_onoff.attr,
	NULL,
};
const struct attribute_group rdvb_dmx_debug_group  = {
	.name = "rdvb_dmx_debug",
	.attrs = rdvb_dmx_debug_attrs,
};
EXPORT_SYMBOL(rdvb_dmx_debug_group);

/*
******************************************************************************
*
*      proc file system function define
*
*******************************************************************************
*/
static int status_proc_open(struct inode * inode, struct file * pfile)
{

	char *status  = NULL;
	status = vmalloc(2048);
	if (status == NULL)
		return -ENOENT;
	pfile->private_data = status;
	status_dump(status);
	return 0;
}
static int status_proc_release(struct inode * inode, struct file * pfile)
{
	char * status = pfile->private_data;
	pfile->private_data = NULL;
	if (status)
		vfree(status);
	return 0;
}

static ssize_t status_proc_read (struct file * pfile, char __user *buf,
										size_t size, loff_t * offset)
{
	char *status  = NULL;
	ssize_t da_size = 0;
	ssize_t read_size = 0;
	int ret = 0;
	if (pfile->private_data == NULL)
		return -EIO;

	status = pfile->private_data;
	da_size = strlen(status);
	if (*offset >= da_size)
		return 0;

	ret = copy_to_user(buf, status+*offset, da_size- *offset);
	read_size = da_size- *offset - ret;

	return read_size;
}
const struct file_operations status_ops = {
	.owner = THIS_MODULE,
	.open = status_proc_open,
	.read = status_proc_read,
	.release = status_proc_release,
};
