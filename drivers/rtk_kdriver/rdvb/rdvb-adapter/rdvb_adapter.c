#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <dvbdev.h>
#include "rdvb_adapter.h"
#include <linux/vmalloc.h>

static struct rdvb_adapter  rdvb_adapter;

extern const struct attribute_group rdvb_buf_debug_group;
extern const struct attribute_group rdvb_dmx_debug_group;
extern const struct attribute_group rdvb_ca_debug_group;

static const struct attribute_group* rdvbDebug_groups[]  = {
	&rdvb_buf_debug_group,
	&rdvb_dmx_debug_group,
	&rdvb_ca_debug_group,
	NULL,
};

//=======================================================================
extern struct dvb_adapter *dvb_find_adapter(int num) ;
static int rdvb_probe(struct platform_device *pdev)
{
	int ret = -1;
	short int ids[DVB_MAX_ADAPTERS] = { 0 };
	struct dvb_adapter *dvb_adap = dvb_find_adapter(0);

	rdvb_adapter.is_master = false;
	if (dvb_adap == NULL) {
		dvb_adap = vmalloc(sizeof(struct dvb_adapter));
		if (dvb_adap == NULL) return -ENOMEM;
		ret = dvb_register_adapter(dvb_adap, "rdvb", THIS_MODULE, &pdev->dev, ids);
		if (ret != 0) {
			printk(KERN_ERR "%s_%d: ERROR: fail register adaptor {0}\n",
			__func__, __LINE__);
			if (ret > 0) dvb_unregister_adapter(dvb_adap);
			vfree(dvb_adap);
			return  -1;
		}
		rdvb_adapter.is_master = true;
	}
	rdvb_adapter.dvb_adapter = dvb_adap;
	rdvb_adapter.proc_dbg_entry =  proc_mkdir("lgtv-driver" , NULL);

	printk("%s_%d:rdvb_adapter(%d)\n",
		__func__, __LINE__, rdvb_adapter.dvb_adapter->num);
	return 0;
}

static int rdvb_remove(struct platform_device *pdev)
{
	printk("%s_%d\n", __func__, __LINE__);

	if (rdvb_adapter.is_master) {
		dvb_unregister_adapter(rdvb_adapter.dvb_adapter);
		vfree(rdvb_adapter.dvb_adapter);
		rdvb_adapter.is_master = false;
	}
	if (!IS_ERR_OR_NULL(rdvb_adapter.proc_dbg_entry)) {
		proc_remove(rdvb_adapter.proc_dbg_entry);
		rdvb_adapter.proc_dbg_entry = NULL;
	}
	return 0;
}

struct rdvb_adapter* rdvb_get_adapter(void)
{
	return &rdvb_adapter;
}
EXPORT_SYMBOL(rdvb_get_adapter);

struct proc_dir_entry* rdvb_adap_register_proc(const char * name)
{
	return proc_mkdir(name, NULL);
}
EXPORT_SYMBOL(rdvb_adap_register_proc);

static struct platform_driver rdvb_drv = {
	.driver = {
		.name = "rdvb",
	},
	.probe = rdvb_probe,
	.remove = rdvb_remove,
};



//======================================================================
struct platform_device rdvb_dev = {
	.name = "rdvb",
	.id = -1,
	.dev = {
		.groups =  rdvbDebug_groups,
	},
	.num_resources = 0,
	
};
int rdvb_init(void)
{
//	prdvb_dev = platform_device_register_simple("rdvb", -1,NULL, 0);
	platform_device_register(&rdvb_dev);
	platform_driver_register(&rdvb_drv);
	return 0;
}

void rdvb_exit(void)
{
	platform_driver_unregister(&rdvb_drv);
	platform_device_unregister(&rdvb_dev);
}
module_init(rdvb_init);
module_exit(rdvb_exit);


MODULE_AUTHOR("jason_wang <jason_wang@realsil.com.cn>");
MODULE_DESCRIPTION("RTK TV dvb Driver");
MODULE_LICENSE("GPL");
