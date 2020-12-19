#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include "dmx_buf_mgr.h"
#include "dmx_buf_dbg.h"
#include "rdvb_dmx_buf.h"
#include <rdvb-common/error.h>

static char * getopt(const  char * opt, const char *buf, int size)
{
	char *free_ptr, *pstr, *psearch;
	char * result =  NULL;
	free_ptr = pstr = kstrdup(buf, GFP_KERNEL);

	while((psearch = strsep(&pstr, " \n")) != NULL){
		if (strcmp(psearch, "") ==0)
			continue;
		if ((psearch = strstr(psearch, opt))!=NULL){
			psearch = strchr(psearch, '=');
			if (psearch){
				result = kstrdup(psearch+1, GFP_KERNEL);
				goto DONE;
			}
		}

	}
DONE:
	if (free_ptr)
		kfree(free_ptr);
	return result;

}
//===================debug===========
extern char * rdvb_dmxbuf_status(char * buf);
static ssize_t buf_status_store(struct device * dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	return count;
}
static ssize_t buf_status_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	printk("%s_%d @@@@#### dev:%s, att:%s\n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	return  rdvb_dmxbuf_status(buf) - buf;
}

DEVICE_ATTR_RW(buf_status);

static ssize_t buf_ops_store(struct device * dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	char *free_ptr, *pstr;
	bool release = false;
	free_ptr = pstr = kstrdup(buf, GFP_KERNEL);
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	if (pstr == NULL)
		return -1;
	while((pstr = strsep(&pstr, " ")) != NULL){
		if ((pstr = strstr(pstr, "ops"))!=NULL){
			pstr = strchr(pstr, '=');
			if (strncmp(pstr+1, "1",1) == 0)
				release = false;
			else if (strncmp(pstr+1, "0",1) ==0)
				release = true;
			else
				goto DONE;
		}
		else
			goto DONE;
		break;

	}
	if (release == false){
		if (rdvb_dmxbuf_request(PIN_TYPE_VTP, 0,HOST_TYPE_SDEC, 0, NULL, NULL) <0){
			goto DONE;
		}
		if (rdvb_dmxbuf_request(PIN_TYPE_VTP, 0,HOST_TYPE_VDEC, 1, NULL, NULL) <0){
			goto DONE;
		}



		if (rdvb_dmxbuf_request(PIN_TYPE_ATP, 0,HOST_TYPE_SDEC, 0, NULL, NULL) <0){
			goto DONE;
		}
		if (rdvb_dmxbuf_request(PIN_TYPE_ATP, 0,HOST_TYPE_ADEC, 1, NULL, NULL) <0){
			goto DONE;
		}
	}
	else {
		rdvb_dmxbuf_release(PIN_TYPE_VTP, 0,HOST_TYPE_SDEC, 0);
		rdvb_dmxbuf_release(PIN_TYPE_VTP, 0,HOST_TYPE_VDEC, 1);


		rdvb_dmxbuf_release(PIN_TYPE_ATP, 0,HOST_TYPE_SDEC, 0);
		rdvb_dmxbuf_release(PIN_TYPE_ATP, 0,HOST_TYPE_ADEC, 1);
	}
DONE:
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, pstr);
	if (free_ptr)
		kfree(free_ptr);
	return count;
}
static ssize_t buf_ops_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("%s_%d @@@@#### dev:%s, att:%s\n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	return print_to_buf(buf, "%s", "echo ops=[0/1] [vtp/atp/pes]=[0/1] [sdec/vdec/adec]=[0...] > buf_pos\n"
		"\t ops: 0: release buffer\n"
		"\t      1: alloc buffer\n"
		"\t vtp/atp/pes: 0/1: buffer type and pin num\n"
		"\t sdec/vdec/adac: 0/1: request buffer host and host port\n") - buf;
}

DEVICE_ATTR_RW(buf_ops);


static ssize_t buf_dump_store(struct device * dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	char * opt = NULL;
	enum pin_type pin_type;
	int pin_port = -1;
	bool flush=false, dump= false;
	char * path = NULL;
	struct PIN_INFO_T * pPin = NULL;
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	//rdvb_dumbuf_dump(PIN_TYPE_VTP, VTP_PORT_0);
	opt = getopt("VTP", buf, count);
	if (opt) {
		pin_type = PIN_TYPE_VTP;
	} else {
		opt = getopt("ATP", buf, count);
		if (opt)
		{
			pin_type = PIN_TYPE_ATP;
		} else {

			opt = getopt("PES", buf, count);
			if (opt)
			{
				pin_type = PIN_TYPE_PES;
			}
		}
	
	}
	if (opt){
		if (strcmp(opt, "1") ==0)
			pin_port =1;
		else if (strcmp(opt, "0") ==0)
			pin_port = 0;
		else {
			kfree(opt);
			return -1;
		}
		kfree(opt);
	}else {
		return -1;
	}

	opt = getopt("FLUSH", buf, count);
	if (opt){
		if (strcmp(opt, "1")==0)
			flush = true;
		else if (strcmp(opt, "0") ==0)
			flush = false;
		else {
			kfree(opt);
			return -1;
		}
		kfree(opt);

	}

	opt = getopt("dump", buf, count);
	if (opt){
		if (strcmp(opt, "1") ==0)
			dump = true;
		else if (strcmp(opt, "0") ==0)
			dump = false;
		else {
			kfree(opt);
			return -1;
		}
		kfree(opt);

	} else {
		return -1;
	}

	opt = getopt("PATH",buf, count);
	if (opt){
		path = opt;
	}else {
		return -1;
	}
	printk("%s_%d: dump=%d, vtp/atp=%d, path=%s, flush=%d\n",
		__func__, __LINE__, dump, pin_port, path, flush);
	
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (dmx_buf_dbg_dump_ctrl(dump, &pPin->debug, path, flush ) <0){
		kfree(path);
		return -1;
	}
	kfree(path);
	return count;
}
static ssize_t buf_dump_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	printk("%s_%d @@@@#### dev:%s, att:%s\n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	return  print_to_buf(buf, "%s", "\necho [VTP/ATP/PES]=[1/0] dump=[1/0] "
		"PATH=/XXX/X/X FLUSH=[1/0] > buf_dump\n") -buf;
}

DEVICE_ATTR_RW(buf_dump);


static ssize_t err_test_store(struct device * dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int err = -1;
	printk("%s_%d @@@@####(%s)\n", __func__, __LINE__, buf);
	if (kstrtoint(buf, 10,  &err) != 0) {
		return -1;
	}
	//error_set(err);
	return count;
}
static ssize_t err_test_show(struct device *dev, struct device_attribute *attr,
				 char *buf)
{
	printk("%s_%d @@@@#### dev:%s, att:%s\n",
	 __func__, __LINE__, dev_name(dev), attr->attr.name);
	//error_print();
	return 0;
}

DEVICE_ATTR_RW(err_test);

struct attribute * rdvb_buf_debug_attrs[] = {
	&dev_attr_buf_status.attr,
	&dev_attr_buf_ops.attr,
	&dev_attr_buf_dump.attr,
	&dev_attr_err_test.attr,
	NULL,
};
const struct attribute_group rdvb_buf_debug_group  = {
	.name = "rdvb_buf_debug",
	.attrs = rdvb_buf_debug_attrs,
};
EXPORT_SYMBOL(rdvb_buf_debug_group);

