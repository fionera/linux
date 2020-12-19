#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <asm/sections.h>
#include <mach/rtk_platform.h>

#include <linuxtv-ext-ver.h>
#include "rdvb-adapter/rdvb_adapter.h"
#include "rdvb-cip/rdvb_cip_ctrl.h"

#include "rdvb_ca.h"
extern char * print_to_buf(char *buf, const char *fmt,...);


static const char * const input_list[] = {
	"Internal","External", "CIP", "MEM", "None"
};

static const char * const slot_state_list[] = {
	"DVB_CA_SLOTSTATE_NONE",
	"DVB_CA_SLOTSTATE_UNINITIALISED", 
	"DVB_CA_SLOTSTATE_RUNNING", 
	"DVB_CA_SLOTSTATE_INVALID", 
	"DVB_CA_SLOTSTATE_WAITREADY",
	"DVB_CA_SLOTSTATE_VALIDATE", 
	"DVB_CA_SLOTSTATE_WAITFR", 
	"DVB_CA_SLOTSTATE_LINKINIT"
};

static const char * const card_status_list[] = {
	"DVB_CA_EN50221_CAMCHANGE_REMOVED",
	"DVB_CA_EN50221_CAMCHANGE_INSERTED", 
};


static ssize_t status_dump(char *buf)
{
	int i,j;
	UINT32 pre_div, post_div, div16, tp_out_speed;
	RDVB_CA* rdvb_ca =	rdvb_get_ca();
	struct dvb_ca_private*	ca_priv = rdvb_ca->pubca.private;
	char *pbuf = buf;
	//tpp_priv_data* rdvb_tpp ;
	//struct rdvb_cip_private *cip_priv;
	
	pbuf = print_to_buf(pbuf, "\nlinuxtv-ext-headerver.%d.%d.%d (submissions/%d)\n\n",
				LINUXTV_EXT_VER_MAJOR, LINUXTV_EXT_VER_MINOR,
				LINUXTV_EXT_VER_PATCH, LINUXTV_EXT_VER_SUBMISSION);

	pbuf = print_to_buf(pbuf, "\n1.1. CI Input\n");

	pbuf = print_to_buf(pbuf, "[External 0,1,2 / Internal 0,1,2 / None ]\n");
	if(rdvb_ca->ca_src.input_src_type < 2){
		pbuf = print_to_buf(pbuf, ": %s %d\n",input_list[(int)rdvb_ca->ca_src.input_src_type],rdvb_ca->ca_src.input_port_num);
	}else{
	    pbuf = print_to_buf(pbuf, ": %s \n",input_list[(int)rdvb_ca->ca_src.input_src_type]);
	}
	
	pbuf = print_to_buf(pbuf, "\n1.2. CI Output\n");
	pbuf = print_to_buf(pbuf, "Destination           	[DMX 0,1,2 / None ] \n");
	pbuf = print_to_buf(pbuf, ": DMX-0 \n");
	pbuf = print_to_buf(pbuf, "\n\n");

	for(i= 0 ;i < MAX_TP_P_COUNT ; i++){
		tpp_priv_data* rdvb_tpp = rdvb_get_cip(i);
		struct rdvb_cip_private *cip_priv = (struct rdvb_cip_private*)rdvb_tpp->tpp.private;
		pbuf = print_to_buf(pbuf, "2.%d. CI+1.4 Sub-Ch-%d \n",(i+1),i);
		pbuf = print_to_buf(pbuf, "Input Source      	[External 0,1,2 / Internal 0,1,2 / UploadBuf / None ] \n");

		if(cip_priv->input_src_type == CA_EXT_SRC_TYPE_IN_DEMOD || cip_priv->input_src_type == CA_EXT_SRC_TYPE_EXT_DEMOD){
			pbuf = print_to_buf(pbuf, ": %s %d\n",input_list[(int)cip_priv->input_src_type],cip_priv->input_port_num);
		}else if(cip_priv->input_src_type ==3){
			pbuf = print_to_buf(pbuf, ": UploadBuf \n");
		}else{
			pbuf = print_to_buf(pbuf, ": None \n");
		}
		pbuf = print_to_buf(pbuf, "Output Destination 	[DMX 0,1,2 / DownloadBuf ] \n");

		if(cip_priv->delivery_mode == CIPLUS_HOST_DELIVERY_MODE){
			pbuf = print_to_buf(pbuf, ": DownloadBuf \n");
		}else{
			pbuf = print_to_buf(pbuf, ": DMX %d \n",i);
		}
		pbuf = print_to_buf(pbuf, "\n\n");
	}

	pbuf = print_to_buf(pbuf, "3. FrontEnd(Tuner/Demod) clock is bypassed to CI ? \n");
	pbuf = print_to_buf(pbuf, ": No \n");
	pbuf = print_to_buf(pbuf, "\n\n");

	RHAL_TPOUT_GetClockSpeed(0,&pre_div, &post_div, &div16);
	if(div16 == 0)
		tp_out_speed = (250 * 100 / (pre_div + 2) / (post_div + 2));
	else
		tp_out_speed = (250 * 100 / (pre_div + 2) / (post_div + 2) / 16);

	pbuf = print_to_buf(pbuf, "4. Clock Frequency \n");
	pbuf = print_to_buf(pbuf, ": %u.%u MHz \n",tp_out_speed / 100, tp_out_speed % 100);
	pbuf = print_to_buf(pbuf, "\n\n");

	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "B. struct dvb_ca_slot (dvb_ca_en50221.c) \n");
	pbuf = print_to_buf(pbuf, "1. slot_state            	[current state of the CAM]  \n");
	pbuf = print_to_buf(pbuf, ": %s \n",slot_state_list[ca_priv->slot_info[0].slot_state]);
	pbuf = print_to_buf(pbuf, "\n\n");
	

	pbuf = print_to_buf(pbuf, "2.   camchange_count     	[Number of CAMCHANGES that have occurred since lastprocessing] \n");
	pbuf = print_to_buf(pbuf, ": %d \n",atomic_read(&ca_priv->slot_info[0].camchange_count));
	pbuf = print_to_buf(pbuf, "\n\n");

	pbuf = print_to_buf(pbuf, "3.  camchange_type        	[Type of last CAMCHANGE ] \n");
	pbuf = print_to_buf(pbuf, ": %s \n",card_status_list[ca_priv->slot_info[0].camchange_type]);
	pbuf = print_to_buf(pbuf, "\n\n");


	pbuf = print_to_buf(pbuf, "4.   config_base          	[base address of CAM config ]  \n"); 
	pbuf = print_to_buf(pbuf, ": 0x%x  \n",ca_priv->slot_info[0].config_base); 
	pbuf = print_to_buf(pbuf, "\n\n");


	pbuf = print_to_buf(pbuf, "5.   da_irq_supported:1   	[if 1, the CAM supports DA IRQs ]  \n");   
 	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->slot_info[0].da_irq_supported); 
	pbuf = print_to_buf(pbuf, "\n\n");

	pbuf = print_to_buf(pbuf, "6.   link_buf_size        	[size of the buffer to use when talking to the CAM] \n");      
	pbuf = print_to_buf(pbuf, ": %d Bytes \n",ca_priv->slot_info[0].link_buf_size);     
	pbuf = print_to_buf(pbuf, "\n\n");

	pbuf = print_to_buf(pbuf, "7.  rx_buffer             	[buffer for incoming packets]  \n");     
	pbuf = print_to_buf(pbuf, "size 	: 	%d Bytes     \n",ca_priv->slot_info[0].rx_buffer.size); 
	pbuf = print_to_buf(pbuf, "pread  	: 	0x%x \n",ca_priv->slot_info[0].rx_buffer.pread); 
	pbuf = print_to_buf(pbuf, "pwrite	:  	0x%x \n",ca_priv->slot_info[0].rx_buffer.pwrite); 
	pbuf = print_to_buf(pbuf, "error   :  	%d \n",ca_priv->slot_info[0].rx_buffer.error); 
	i = dvb_ringbuffer_avail(&ca_priv->slot_info[0].rx_buffer);
	pbuf = print_to_buf(pbuf, "Fill    :	%d Bytes, %d %% \n",i,(i*100)/ca_priv->slot_info[0].rx_buffer.size); 
	pbuf = print_to_buf(pbuf, "Empty 	:	%d Bytes, %d %% \n",(ca_priv->slot_info[0].rx_buffer.size-i),((ca_priv->slot_info->rx_buffer.size-i)*100)/ca_priv->slot_info->rx_buffer.size);  
	pbuf = print_to_buf(pbuf, "\n\n");

	pbuf = print_to_buf(pbuf, "8.	 Timeout              	[timer used during various states of the slot] \n");     	  
	pbuf = print_to_buf(pbuf, ": %lu msec \n",ca_priv->slot_info[0].timeout);     
	pbuf = print_to_buf(pbuf, "\n\n");
	
	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "C. struct dvb_ca_private (dvb_ca_en50221.c) \n"); 

	pbuf = print_to_buf(pbuf, "1.Flags                  	[Flags describing the interface (DVB_CA_FLAG_*) ] \n"); 
	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->flags); 

	pbuf = print_to_buf(pbuf, "2.slot_count               	[number of slots supported by this CA interface] \n"); 
	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->slot_count); 

	pbuf = print_to_buf(pbuf, "3. open                   	[Flag indicating if the CA device is open] \n"); 
	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->open); 

	pbuf = print_to_buf(pbuf, "4. wakeup                	[Flag indicating the thread should wake up now ] \n"); 
	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->wakeup); 

	pbuf = print_to_buf(pbuf, "5.Delay                   	[Delay the main thread should use ] \n"); 
	pbuf = print_to_buf(pbuf, ": %lu msec \n",ca_priv->delay*10); 

	pbuf = print_to_buf(pbuf, "6. next_read_slot          	[Slot to start looking for data to read from in the next user-space read operation] \n"); 
	pbuf = print_to_buf(pbuf, ": %d \n",ca_priv->next_read_slot);
	pbuf = print_to_buf(pbuf, "\n");
	
	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "D. Tuple Data \n"); 
	pbuf = print_to_buf(pbuf, "1. Raw data of CISTPL_VERS_1  (0x15) \n"); 
	pbuf = print_to_buf(pbuf, ":  "); 
	for(i= 0 ;i < rdvb_ca->ci_camInfo.cistpl_ver1.size ; i++){
		pbuf = print_to_buf(pbuf, " %02x ",rdvb_ca->ci_camInfo.cistpl_ver1.data[i]); 
	}
	pbuf = print_to_buf(pbuf, "\n");

	pbuf = print_to_buf(pbuf, "2. Raw data of CISTPL_MANFID (0x20) \n"); 
 	pbuf = print_to_buf(pbuf, ":  "); 
	for(i= 0 ;i < rdvb_ca->ci_camInfo.cistpl_manfid.size ; i++){
		pbuf = print_to_buf(pbuf, " %02x ",rdvb_ca->ci_camInfo.cistpl_manfid.data[i]); 
	}
	pbuf = print_to_buf(pbuf, "\n\n");


	pbuf = print_to_buf(pbuf, "E. CI+1.4 Filter \n"); 
	for(i= 0 ;i < MAX_TP_P_COUNT ; i++){
		tpp_priv_data* rdvb_tpp = rdvb_get_cip(i);
		pbuf = print_to_buf(pbuf, "%d. Sub-Channel-%d \n",i,i); 
		if(rdvb_tpp->pidTable.count > 0){
			for(j = 0; j < PID_MAX_NUM_TO_SET; j++) {
				if(rdvb_tpp->pidTable.pidList[j].valid) {
					pbuf = print_to_buf(pbuf, " %d \n",rdvb_tpp->pidTable.pidList[j].pid); 
				}
			}
		}else{
			pbuf = print_to_buf(pbuf, " NONE \n"); 
		}	
	}

	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "F. CI+1.4 Buffer \n\n"); 
	for(i= 0 ;i < MAX_TP_P_COUNT ; i++){
		UINT8 *pWritePhyAddr = 0;
		UINT32 buf_size=0,empty_size=0,fill_size=0,ret;
		UINT32 contiguousWritableSize = 0 ,writableSize = 0;
		UINT32 phyBase=0,phyLimit=0,phyRead=0,phyWrite=0;
		
		tpp_priv_data* rdvb_tpp = rdvb_get_cip(i);

		pbuf = print_to_buf(pbuf, "%d. Sub-Channel-%d \n",i,i); 
		pbuf = print_to_buf(pbuf, "UpLoadBuf \n");

		if(rdvb_tpp->mtpBuffer.inited){
			buf_size = rdvb_tpp->mtpBuffer.size;
			ret = RHAL_GetMTPBufferStatus(rdvb_tpp->mtp_id, &pWritePhyAddr, &contiguousWritableSize,&writableSize);
			if(ret == TPK_SUCCESS){
				empty_size = writableSize;
				fill_size = buf_size - writableSize;
			}
			pbuf = print_to_buf(pbuf, ": Size = %d Bytes,  Empty = %d Bytes, %d%%,	Fill = %d Bytes, %d%% \n",buf_size,empty_size,((empty_size*100)/buf_size),fill_size,((fill_size*100)/buf_size)); 
		}else{
			pbuf = print_to_buf(pbuf, ": Size = %d Bytes,  Empty = %d Bytes, %d%%,	Fill = %d Bytes, %d%% \n",buf_size,empty_size,0,fill_size,0);
		}

		pbuf = print_to_buf(pbuf, "DownLoadBuf \n"); 
		buf_size = empty_size = fill_size =0;
		if(RHAL_CIP_GetBufferStatus((TPK_TP_ENGINE_T)rdvb_tpp->tpp_id, &phyBase, &phyLimit, &phyRead, &phyWrite) == TPK_SUCCESS){
			buf_size = phyLimit - phyBase;
			fill_size = (phyWrite >= phyRead) ? (phyWrite - phyRead) : (buf_size - (phyRead - phyWrite));
			empty_size = buf_size - fill_size;
			pbuf = print_to_buf(pbuf, ": Size = %d Bytes,  Empty = %d Bytes, %d%%,	Fill = %d Bytes, %d%% \n",buf_size,empty_size,((empty_size*100)/buf_size),fill_size,((fill_size*100)/buf_size));
		}else{
			pbuf = print_to_buf(pbuf, ": Size = %d Bytes,  Empty = %d Bytes, %d%%,	Fill = %d Bytes, %d%% \n",buf_size,empty_size,0,fill_size,0);
		}
		pbuf = print_to_buf(pbuf, "\n\n");

	}
	pbuf = print_to_buf(pbuf, "4. ECP enable ecp enabled = 0 \n"); 

	
	return pbuf - buf;
}

static ssize_t status_store(struct device * dev, struct device_attribute *attr, const char *buf, size_t count)
{
	printk("%s_%d (%s)\n", __func__, __LINE__, buf);
	return count;
}

static ssize_t status_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return status_dump(buf);
}
				
static DEVICE_ATTR_RW(status);


static ssize_t ca_log_mask_store(struct device * dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long mask = 0;
	printk("rtk_ca_dbg Level: %s \n", buf);
	if (kstrtoll(buf, 0, &mask) != 0) {
		return -1;
	}
	rdvb_ca_dbg_log_mask = mask;

	return count;

}
static ssize_t ca_log_mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf = print_to_buf(pbuf, "\n[rtk ca attr & io data log onoff]\n\n");
	pbuf = print_to_buf(pbuf, "Enable attr log: \n");
	pbuf = print_to_buf(pbuf, "echo 1 > /sys/devices/platform/rdvb/rdvb_ca_debug/ca_log_mask \n\n");
	pbuf = print_to_buf(pbuf, "Enable io log: \n");
	pbuf = print_to_buf(pbuf, "echo 2 > /sys/devices/platform/rdvb/rdvb_ca_debug/ca_log_mask \n\n");
	pbuf = print_to_buf(pbuf, "Enable both: \n");
	pbuf = print_to_buf(pbuf, "echo 3 > /sys/devices/platform/rdvb/rdvb_ca_debug/ca_log_mask \n\n");

	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "The current value is:%d \n\n",rdvb_ca_dbg_log_mask); 

	return pbuf - buf;
}

static DEVICE_ATTR_RW(ca_log_mask);


static ssize_t cip_log_mask_store(struct device * dev, struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long long mask = 0;
	printk("rtk_cip_dbg Level: %s \n", buf);
	if (kstrtoll(buf, 0, &mask) != 0) {
		return -1;
	}
	rdvb_cip_dbg_log_mask = mask;

	return count;

}
static ssize_t cip_log_mask_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *pbuf = buf;

	pbuf = print_to_buf(pbuf, "\n[rtk cip log onoff]\n\n");
	pbuf = print_to_buf(pbuf, "Enable debug log: \n");
	pbuf = print_to_buf(pbuf, "echo 1 > /sys/devices/platform/rdvb/rdvb_ca_debug/cip_log_mask \n\n");
	pbuf = print_to_buf(pbuf, "Enable trace log: \n");
	pbuf = print_to_buf(pbuf, "echo 2 > /sys/devices/platform/rdvb/rdvb_ca_debug/cip_log_mask \n\n");
	pbuf = print_to_buf(pbuf, "Enable both: \n");
	pbuf = print_to_buf(pbuf, "echo 3 > /sys/devices/platform/rdvb/rdvb_ca_debug/cip_log_mask \n\n");

	pbuf = print_to_buf(pbuf, "\n\n");
	pbuf = print_to_buf(pbuf, "The current value is:%d \n\n",rdvb_cip_dbg_log_mask); 

	return pbuf - buf;
}

static DEVICE_ATTR_RW(cip_log_mask);


struct attribute * rdvb_ca_debug_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_ca_log_mask.attr,
	&dev_attr_cip_log_mask.attr,
	NULL,
};

const struct attribute_group rdvb_ca_debug_group  = {
	.name = "rdvb_ca_debug",
	.attrs = rdvb_ca_debug_attrs,
};

EXPORT_SYMBOL(rdvb_ca_debug_group);


static ssize_t status_proc_open(struct inode * inode, struct file * pfile){
	char *status  = NULL;
	status = vmalloc(1024*4);
	if (status == NULL)
		return -ENOENT;
	pfile->private_data = status;
	status_dump(status);
	return 0;
}

static ssize_t status_proc_read (struct file * pfile, char __user *buf,
										size_t size, loff_t * offset){
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
										
static ssize_t status_proc_release(struct inode * inode, struct file * pfile){
	char * status = pfile->private_data;
	pfile->private_data = NULL;
	if (status)
		vfree(status);
	return 0;
}	

const struct file_operations ca_status_proc_fops = {
	.owner = THIS_MODULE,
	.open = status_proc_open,
	.read = status_proc_read,
	.release = status_proc_release,
};


