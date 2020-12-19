#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include "rdvb_cip_dev.h"


/************** globa para *****************/
UINT8 rdvb_cip_dbg_log_mask = 0;


static int rdvb_cip_io_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_cip_private *cip_priv = dvbdev->priv;

	RDVB_CIP_LOG_TRACE("%s index=%d\n", __func__,cip_priv->index);
	
	if(cip_priv->open==0){
		cip_priv->tpp->init(cip_priv->tpp);
	}

	if (mutex_lock_interruptible(&cip_priv->mutex))
		return -ERESTARTSYS;
	
	cip_priv->open++;
	
	mutex_unlock(&cip_priv->mutex);

	return 0;
}

static ssize_t rdvb_cip_io_read(struct file *file, char __user * buf,
				      size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_cip_private *cip_priv = dvbdev->priv;
	int ret; 
	
	RDVB_CIP_LOG_TRACE("%s index=%d,count=%d\n", __func__,cip_priv->index,(int)count);

	if (mutex_lock_interruptible(&cip_priv->mutex))
		return -ERESTARTSYS;
	
	if(cip_priv->bypass_mode){
		if(cip_priv->bypass_buf == NULL){
			ret = -ENOMEM;
		}else{
			ret = cip_priv->bypass_buf_size;
			if (copy_to_user(buf, cip_priv->bypass_buf,cip_priv->bypass_buf_size)){
				RDVB_CIP_LOG_ERROR("%s copy_to_user Fail\n", __func__);
				ret = -EFAULT;
			}
			kfree(cip_priv->bypass_buf);
			cip_priv->bypass_buf = NULL;
		}
	}else{
		ret = cip_priv->tpp->read(cip_priv->tpp, buf, count);
	}

	mutex_unlock(&cip_priv->mutex);
	  
	return ret;
}

					  
static ssize_t rdvb_cip_io_write(struct file *file,const char __user * buf,
						 size_t count, loff_t * ppos)

{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_cip_private *cip_priv = dvbdev->priv;
	int ret;

	RDVB_CIP_LOG_TRACE("%s index=%d,count=%d\n", __func__,cip_priv->index,(int)count);

	//if ((file->f_flags & O_ACCMODE) != O_WRONLY)
	//	return -EINVAL;
	
	if (mutex_lock_interruptible(&cip_priv->mutex))
		return -ERESTARTSYS;
	
	if(cip_priv->bypass_mode){
		cip_priv->bypass_buf_size = count;
		if(cip_priv->bypass_buf==NULL){
			if ((cip_priv->bypass_buf = kzalloc(count, GFP_KERNEL)) == NULL) {
				ret = -ENOMEM;
				mutex_unlock(&cip_priv->mutex);
				return ret;
			}
		}
		ret = count;
		if (copy_from_user((void *)cip_priv->bypass_buf, buf, count)) {
			RDVB_CIP_LOG_ERROR("[%s, %d]: Copy data to bypass_buf failed !! \n",__func__, __LINE__);
			ret = -EFAULT;
		}
		
	}else{
		ret = cip_priv->tpp->write(cip_priv->tpp, buf, count);
	}
	mutex_unlock(&cip_priv->mutex);

	return ret;
}



static int rdvb_cip_io_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_cip_private *cip_priv = dvbdev->priv;

	RDVB_CIP_LOG_TRACE("%s index=%d\n", __func__,cip_priv->index);
	
	if (mutex_lock_interruptible(&cip_priv->mutex))
		return -ERESTARTSYS;

	cip_priv->open--;

	if(cip_priv->open==0){
		cip_priv->tpp->unInit(cip_priv->tpp);
	}

	mutex_unlock(&cip_priv->mutex);

	return 0;
}


static int rdvb_cip_io_ext_set_ioctl(struct rdvb_cip_private *cip_priv, struct ca_ext_control *ext){
	int err = 0;
	
	//RDVB_CIP_LOG_TRACE("%s SET ca_ext_control.id=%d\n", __func__,ext->id);
	
	switch(ext->id){
		case CA_EXT_CID_PLUS14_INPUTSOURCE:{
			struct ca_ext_source cip_source;
			cip_source.input_src_type = CA_EXT_SRC_TYPE_IN_DEMOD;
			cip_source.input_port_num = 0;
			if(ext->size){
			    if (copy_from_user(&cip_source, (void __user *) ext->ptr, ext->size)){
		            return -EINVAL;
			    }
			}
			if(cip_source.input_src_type == CA_EXT_SRC_TYPE_IN_DEMOD || cip_source.input_src_type == CA_EXT_SRC_TYPE_EXT_DEMOD )
				cip_priv->ts_mode = true;
			else
				cip_priv->ts_mode = false;
			
			err = cip_priv->tpp->setConfig(cip_priv->tpp,cip_source);
			if(!err){
				cip_priv->input_port_num = cip_source.input_port_num;
				cip_priv->input_src_type = cip_source.input_src_type;
			}
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_INPUTSOURCE input_port_num=%d, input_src_type=%d(0:INDEMOD/1:EXTDEMOD/2:CIP/3:MEM/4:NULL),err=%d\n", __func__,cip_source.input_port_num,cip_source.input_src_type,err);
			break;
		}
		case CA_EXT_CID_PLUS14_ADD_PID_FILTER:{
			UINT16 PID = ext->value64;
			
			err = cip_priv->tpp->addFilter(cip_priv->tpp,PID);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_ADD_PID_FILTER PID=%x ,err=%d\n", __func__,PID,err);
			break;
		}
		case CA_EXT_CID_PLUS14_REMOVE_PID_FILTER:{
			UINT16 PID = ext->value64;
			
			err = cip_priv->tpp->removeFilter(cip_priv->tpp,PID);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_REMOVE_PID_FILTER PID=%x ,err=%d\n", __func__,PID,err);
			break;
		}
		case CA_EXT_CID_PLUS14_START_SENDING_DATA:{
				
			err = cip_priv->tpp->sendingEnable(cip_priv->tpp,1);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_START_SENDING_DATA err=%d \n", __func__,err);
			break;
		}
		case CA_EXT_CID_PLUS14_STOP_SENDING_DATA:{
			
			err = cip_priv->tpp->sendingEnable(cip_priv->tpp,0);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_STOP_SENDING_DATA ,err=%d\n", __func__,err);	
			break;	
		}
		case CA_EXT_CID_PLUS14_DOWNLOAD_MODE:{
			cip_priv->delivery_mode = (enum ciplus_delivery_mode)ext->value64;		
			
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_DOWNLOAD_MODE delivery_mode=%lld ,err=%d\n", __func__,ext->value64,err); 
			break;	
		}
		case CA_EXT_CID_PLUS14_START_RECEIVING_DATA:{
			
			err = cip_priv->tpp->ReceivingEnable(cip_priv->tpp,1);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_STOP_SENDING_DATA ,err=%d\n", __func__,err);	 
			break;	
		}
		case CA_EXT_CID_PLUS14_STOP_RECEIVING_DATA:{
			
			err = cip_priv->tpp->ReceivingEnable(cip_priv->tpp,0);
			RDVB_CIP_LOG_DEBUG("%s SET CA_EXT_CID_PLUS14_STOP_SENDING_DATA ,err=%d\n", __func__,err);	 
			break;	
		}
		case CA_EXT_CID_CTS_IO_BYPASS_MODE:{
			
			cip_priv->bypass_mode = ext->value64;
			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_CTS_IO_BYPASS_MODE ext->value64=%lld ,err=%d\n", __func__,ext->value64,err);
			break;
		}

		default:
			err = -EINVAL;
			break;
	}
	return err;
}

static int rdvb_cip_io_ext_get_ioctl(struct rdvb_cip_private *cip_priv, struct ca_ext_control *ext){

	int err = 0;
	//RDVB_CIP_LOG_TRACE("%s GET ca_ext_control.id=%d\n", __func__,ext->id);
	
	switch(ext->id){
		case CA_EXT_CID_PLUS14_INPUTSOURCE:{
			struct ca_ext_source tpp_source;
			tpp_source.input_port_num = cip_priv->input_port_num;
			tpp_source.input_src_type = cip_priv->input_src_type;
			//copy_to_user
			ext->size = sizeof(tpp_source);
			if (copy_to_user(ext->ptr,&tpp_source, sizeof(tpp_source))){
			    return -EINVAL;
			}

			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_PLUS14_INPUTSOURCE input_port_num=%d, input_src_type=%d(0:INDEMOD/1:EXTDEMOD/2:CIP/3:MEM/4:NULL)\n", __func__,tpp_source.input_port_num,tpp_source.input_src_type);
			break;
		}
		case CA_EXT_CID_PLUS14_NUM_OF_PID_FILTER_PER_CH:{
			ext->value64 = cip_priv->tpp->getFilterNum(cip_priv->tpp);
			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_PLUS14_NUM_OF_PID_FILTER_PER_CH getFilterNum=%lld\n", __func__,ext->value64);
			
			break;
		}
		case CA_EXT_CID_PLUS14_CICAM_IN_BITRATE:{
			UINT32 inBitRate,outBitRate;
			err = cip_priv->tpp->getCamBitRate(cip_priv->tpp,&inBitRate,&outBitRate);
			ext->value64 = inBitRate;
			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_PLUS14_CICAM_IN_BITRATE inBitRate=%lld\n", __func__,ext->value64);
			break;	
		}
		case CA_EXT_CID_PLUS14_CICAM_OUT_BITRATE:{
			UINT32 inBitRate,outBitRate;
			err = cip_priv->tpp->getCamBitRate(cip_priv->tpp,&inBitRate,&outBitRate);
			ext->value64 = outBitRate;
			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_PLUS14_CICAM_OUT_BITRATE outBitRate=%lld\n", __func__,ext->value64);
			break;	
		}

		case CA_EXT_CID_CTS_IO_BYPASS_MODE:{
			ext->value64 = cip_priv->bypass_mode;
			RDVB_CIP_LOG_DEBUG("%s GET CA_EXT_CID_CTS_IO_BYPASS_MODE bypass_mode=%lld\n", __func__,ext->value64);
			break;
		}
		default:
			err = -EINVAL;
			break;
	}	
	return err;
}

static int rdvb_cip_io_do_ioctl(struct file *file,unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_cip_private *cip_priv = dvbdev->priv;
	int err = 0;
	
	if (mutex_lock_interruptible(&cip_priv->mutex))
	    return -ERESTARTSYS;

	switch (cmd) {

		case CA_EXT_S_CTL: {
			struct ca_ext_control *cip_set_ext = parg;

			if (rdvb_cip_io_ext_set_ioctl(cip_priv,cip_set_ext)< 0) {
			    err = -EINVAL;
			}

			break;
		}
		case CA_EXT_G_CTL: {
			struct ca_ext_control *cip_get_ext = parg;
			
			if (rdvb_cip_io_ext_get_ioctl(cip_priv,cip_get_ext)< 0) {
				err = -EINVAL;
			}

			break;
		}
		default:
			err = -EINVAL;
			break;
	}

//out_unlock:
	mutex_unlock(&cip_priv->mutex);
	return err;
	
}


static long rdvb_cip_io_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, rdvb_cip_io_do_ioctl);
}


static const struct file_operations rdvb_cip_fops = {
	.owner = THIS_MODULE,
	.read = rdvb_cip_io_read,
	.write = rdvb_cip_io_write,
	.unlocked_ioctl = rdvb_cip_io_ioctl,
	.open = rdvb_cip_io_open,
	.release = rdvb_cip_io_release,
	.llseek = noop_llseek,
};

static const struct dvb_device dvbdev_cip = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1,
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	.name = "dvb-cip",
#endif
	.fops = &rdvb_cip_fops,
};


int rdvb_cip_dev_init(struct dvb_adapter *dvb_adapter,struct rdvb_cip_tpp *tpp)
{
	int ret;
	struct rdvb_cip_private *cip_priv = NULL;  
	
	RDVB_CIP_MUST_PRINT("%s\n", __func__);

	/* initialise the system data */
	if ((cip_priv = kzalloc(sizeof(struct rdvb_cip_private), GFP_KERNEL)) == NULL) {
		ret = -ENOMEM;
		RDVB_CIP_LOG_ERROR("%s kzalloc error\n", __func__);
		goto exit;
	}
	
	init_waitqueue_head(&cip_priv->wait_queue);

	cip_priv->tpp           	= tpp;
	cip_priv->open           	= 0;
	cip_priv->wakeup         	= 0;
	cip_priv->index          	= tpp->id;
	cip_priv->input_src_type  	= CA_EXT_SRC_TYPE_NULL;
	cip_priv->input_port_num  	= 0;
	//cip_priv->input_port_type = CA_EXT_PORT_TYPE_PARALLEL;
	cip_priv->delivery_mode 	= CIPLUS_CICAM_DELIVERY_MODE;
	cip_priv->bypass_mode    	= 0;
	cip_priv->bypass_buf     	= NULL;

	tpp->private              	= cip_priv;
	
	/* register the DVB device */
	ret = dvb_register_device(dvb_adapter, &cip_priv->dvbdev, &dvbdev_cip, cip_priv, DVB_DEVICE_CA);
	if (ret)
		goto exit;
	
	mutex_init(&cip_priv->mutex);
	
	if (signal_pending(current)) {
	    ret = -EINTR;
	    goto unregister_device;
	}
	mb();

	return 0;

unregister_device:
	dvb_unregister_device(cip_priv->dvbdev);
exit:
	tpp->private = NULL;
	return ret;
}



/**
 * Release a DVB CA EN50221 interface device.
 *
 * @ca_dev: The dvb_device_t instance for the CA device.
 * @ca: The associated dvb_ca instance.
 */
void rdvb_cip_dev_release(struct rdvb_cip_tpp *tpp)
{
	struct rdvb_cip_private *cip_priv = tpp->private;

	RDVB_CIP_MUST_PRINT("%s\n", __func__);

	/* shutdown the thread if there was one */
	//kthread_stop(ca_plus->thread);

	dvb_unregister_device(cip_priv->dvbdev);
	kfree(cip_priv);
	tpp->private = NULL;
	
}

EXPORT_SYMBOL(rdvb_cip_dev_release);


