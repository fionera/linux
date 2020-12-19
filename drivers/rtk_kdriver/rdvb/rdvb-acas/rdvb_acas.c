#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/delay.h>

#include <dvbdev.h>
#include <dmxdev.h>
#include <dvb_demux.h>

#include "rdvb-adapter/rdvb_adapter.h"
#include "rdvb_acas.h"
#include "rtk_lg_board.h"

#include "rdvb_acas_atr.h"
#include "rdvb_acas_session_t1.h"
#include "rdvb_acas_debug.h"

/************** globa para *****************/
static struct rdvb_acas_priv grdvb_acas_priv;
extern scd *p_acas_scd;

static int rdvb_acas_io_open(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_acas_priv *acas_priv = dvbdev->priv;
	int err;

	ACAS_INFO("%s %d\n", __func__, __LINE__);

	if(acas_priv->open != 0) {
		acas_priv->open++;
		ACAS_INFO("%s %d already open cnt = %d\n", __func__, __LINE__, acas_priv->open);
		return 0;
	}

	if (!try_module_get(acas_priv->owner))
		return -EIO;

	err = dvb_generic_open(inode, file);
	if (err < 0) {
		module_put(acas_priv->owner);
		ACAS_INFO("%s %d dvb_generic_open error!!\n", __func__, __LINE__);
		return err;
	}

	acas_priv->open++;
	ACAS_INFO("%s %d open cnt = %d\n", __func__, __LINE__, acas_priv->open);
	return 0;
}

static int rdvb_acas_io_release(struct inode *inode, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_acas_priv *acas_priv = dvbdev->priv;

	int err;

	ACAS_INFO("%s cnt = %d\n", __func__, acas_priv->open);

	acas_priv->open--;

	err = dvb_generic_release(inode, file);

	module_put(acas_priv->owner);
#if 0
	if(acas_priv->pscd != NULL){
		scd_deactivate(acas_priv->pscd);
	}
#endif
	return err;
}

static int rdvb_acas_io_ext_cid_init(struct rdvb_acas_priv *acas_priv, unsigned char ubyChipIndex)
{
	int err = 0;

	//acas_priv->pscd = scd_open(ubyChipIndex);
	acas_priv->pscd = p_acas_scd;
	if(NULL == acas_priv->pscd){
		ACAS_ERROR("open scd failed \n");
		return -1;
	}
	return err;
}

static int rdvb_acas_io_ext_cid_reset(struct rdvb_acas_priv *acas_priv)
{
	int err = 0;
	scd_atr_info *patr_info;
	unsigned long etu;
	int count;

	if(NULL == acas_priv->pscd){
		ACAS_ERROR("acas not init or init failed \n");
		return -1;
	}
	scd_enable(acas_priv->pscd, 1);
	scd_deactivate(acas_priv->pscd);
	msleep(200);

	err = scd_reset(acas_priv->pscd);
	count = 0;
	while(0 != err && count < 3){
		msleep(200);
		err = scd_reset(acas_priv->pscd);
		ACAS_ERROR(" reset scd failed, retry reset count %d \n", count);
		count++;
	}

	patr_info = &(acas_priv->pscd->atr_info);
	acas_atr_dump((unsigned char *)patr_info, sizeof(scd_atr_info));

	if(0 != err){
		ACAS_ERROR(" reset scd failed \n");
		return -1;
	}

	if (has_ta1(patr_info)){
		unsigned long clock;
		SC_PARITY     parity;
		unsigned long guard_interval;
		scd_get_clock(acas_priv->pscd, &clock);
		scd_get_etu(acas_priv->pscd, &etu);
		scd_get_parity(acas_priv->pscd, &parity);
		scd_get_guard_interval(acas_priv->pscd, &guard_interval);
		if (SC_PROTOCOL_T14 == sc_atr_protocol(patr_info)){
			err = SC_GetT14ETU(patr_info->T1[0]);
			if (err < 0){
				ACAS_ERROR("unknown TA (%02x) !=0x21, force set ETU to 620\n", patr_info->T1[0]);
			}
			etu = 620;
		}else {
			etu = SC_GetFi(ta1_f(patr_info))/SC_GetDi(ta1_d(patr_info));
			ACAS_INFO("sc_atr_nigoation_mode(atr_info)=%d, fi=%d (%d), di=%d (%d), etu=%ld\n",
				sc_atr_nigoation_mode(patr_info),
				ta1_f(patr_info), SC_GetFi(ta1_f(patr_info)),
				ta1_d(patr_info), SC_GetDi(ta1_d(patr_info)),
				etu);
		}
		#ifdef USE_PPS
		if (sc_atr_nigoation_mode(patr_info)){
			if (SC_SetPPS(acas_priv->pscd, ta1_f(patr_info), ta1_d(patr_info)) != 0){	
				ACAS_ERROR("Set PPS failed\n");
				err = -1;
			}
		}
		#endif
		scd_set_clock(acas_priv->pscd, clock);
		scd_set_etu(acas_priv->pscd, etu);
		scd_set_parity(acas_priv->pscd, parity);
		scd_set_guard_interval(acas_priv->pscd, guard_interval);
	}

	/* get reset time */
	if(patr_info->nHistoryBytes >= 2) {
		acas_priv->reset_time = patr_info->History[1];
	} else {
		acas_priv->reset_time = 0;
	}

	rdvb_acas_io_init_session_t1();
	if (sc_atr_protocol(patr_info)==SC_PROTOCOL_T1){
		rdvb_acas_session_t1_set_param(SC_PROTOCOL_PARAM_T1_IFSC, (has_ta3(patr_info)) ? ta3_ifsc(patr_info) : 32);
		rdvb_acas_session_t1_set_param(SC_PROTOCOL_PARAM_T1_EDC_ALGO, (has_td3(patr_info)) ? tc3_edc(patr_info) : 0); 
	}

	scd_activate(acas_priv->pscd);
	return err;
}

static int rdvb_acas_io_ext_cid_transfer_apdu(struct rdvb_acas_priv *acas_priv, struct acas_ext_transfer_apdu  *pacas_apdu_info)
{
	int err = 0;
	unsigned char *papdu = NULL;
	unsigned char *acas_papdu;
	int acas_apdu_len;
	unsigned char apdu_resp[ACAS_APDU_RESP_MAX_LEN];
	__u32 apdu_resp_len = ACAS_APDU_RESP_MAX_LEN;

	if(NULL == acas_priv->pscd){
		ACAS_ERROR("not init or init failed \n");
		return -1;
	}

	if(pacas_apdu_info->command_apdu_len > 1024)
	{
		ACAS_ERROR("%s %d, buffer length failed [%u]\n", __FUNCTION__, __LINE__, pacas_apdu_info->command_apdu_len);
		return -1;
	}


	papdu = kmalloc(pacas_apdu_info->command_apdu_len, GFP_KERNEL);
	if(NULL == papdu){
		ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (copy_from_user(papdu, (void __user *)pacas_apdu_info->command_apdu, pacas_apdu_info->command_apdu_len)){	
		ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
		kfree(papdu);
		return -1;
	}

	acas_apdu_tx_dump(papdu, pacas_apdu_info->command_apdu_len);
	acas_papdu = papdu+3;
	acas_apdu_len = papdu[2];
	err = rdvb_acas_session_t1_transfer(acas_priv->pscd,
		                            acas_papdu, acas_apdu_len,
		                            apdu_resp, &apdu_resp_len,
		                            0);
	if(err){
		ACAS_ERROR("%s %d, failed err:%d\n", __FUNCTION__, __LINE__, err);
	}
	acas_apdu_rx_dump(apdu_resp, apdu_resp_len);

	if (copy_to_user((void __user *)(pacas_apdu_info->response_apdu_len), &apdu_resp_len, sizeof(__u32))){
		ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
		err = -1;
	}

	if (copy_to_user((void __user *)pacas_apdu_info->response_apdu, apdu_resp, apdu_resp_len)){
		ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
		err = -1;
	}

	kfree(papdu);
	return err;
}


static int rdvb_acas_io_ext_set_ioctl(struct rdvb_acas_priv *acas_priv, struct acas_ext_control *ext){
	int err = 0;

	ACAS_DEBUG("%s SET ca_ext_control.id=%d\n", __func__,ext->id);

	switch(ext->id){
		case ACAS_EXT_CID_INIT:{
			if(ext->size){
				ACAS_ERROR("ACAS_EXT_CID_INIT para error.L:%d \n", __LINE__);
				return -1;
			}
			err = rdvb_acas_io_ext_cid_init(acas_priv, (unsigned char)(ext->value64));
			break;
		}
		case ACAS_EXT_CID_RESET:{
			if(ext->size){
				ACAS_ERROR("ACAS_EXT_CID_RESET para error.L:%d \n", __LINE__);
				return -1;
			}
			err = rdvb_acas_io_ext_cid_reset(acas_priv);
			break;
		}
		case ACAS_EXT_CID_TRANSFER_APDU:{
			struct acas_ext_transfer_apdu  acas_apdu_info;
			if(0 == ext->size){
				ACAS_ERROR("ACAS_EXT_CID_TRANSFER_APDU para error.L:%d \n", __LINE__);
				return -1;
			}
			if (copy_from_user(&acas_apdu_info, ext->ptr, ext->size)){
				ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
				return -1;
			}

			err = rdvb_acas_io_ext_cid_transfer_apdu(acas_priv, &acas_apdu_info);

			if (copy_to_user((void __user*)ext->ptr, (void __user*)(&acas_apdu_info), ext->size)){	
				ACAS_ERROR("%s %d, failed \n", __FUNCTION__, __LINE__);
				err = -1;
			}

			break;
		}

		default:
			err = -EINVAL;
			break;
	}

	ACAS_DEBUG("%s SET ca_ext_control.id=%d, err:%d\n", __func__,ext->id, err);

	return err;
}

static int rdvb_acas_io_ext_get_ioctl(struct rdvb_acas_priv *acas_priv, struct acas_ext_control *ext){
	int err = 0;

	ACAS_DEBUG("%s GET acas_ext_control.id=%d\n", __func__,ext->id);

	switch(ext->id){

		case ACAS_EXT_CID_GET_RESET_TIME:{

			ext->value64 = acas_priv->reset_time;

			ACAS_DEBUG("%s GET ACAS_EXT_CID_GET_RESET_TIME ext->value64=%lld\n", __func__,ext->value64);

			break;
		}
		default:
			err = -EINVAL;
			break;
	}
	return err;
}

static int rdvb_cacs_io_do_ioctl(struct file *file,unsigned int cmd, void *parg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct rdvb_acas_priv *acas_priv = dvbdev->priv;
	int err = 0;

	if (mutex_lock_interruptible(&acas_priv->ioctl_mutex))
	    return -ERESTARTSYS;

	switch (cmd) {

		case ACAS_EXT_S_CTL: {
			struct acas_ext_control *acas_set_ext = parg;

			err = rdvb_acas_io_ext_set_ioctl(acas_priv,acas_set_ext);

			break;
		}
		case ACAS_EXT_G_CTL: {
			struct acas_ext_control *acas_get_ext = parg;

			err = rdvb_acas_io_ext_get_ioctl(acas_priv,acas_get_ext);

			break;
		}
		default:
			err = -EINVAL;
			break;
	}

//out_unlock:
	mutex_unlock(&acas_priv->ioctl_mutex);
	return err;

}


static long rdvb_acas_io_ioctl(struct file *file,unsigned int cmd, unsigned long arg)
{
	return dvb_usercopy(file, cmd, arg, rdvb_cacs_io_do_ioctl);
}


static const struct file_operations rdvb_acas_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = rdvb_acas_io_ioctl,
	.open = rdvb_acas_io_open,
	.release = rdvb_acas_io_release,
	.llseek = noop_llseek,
};

static const struct dvb_device dvbdev_acas_plus = {
	.priv = NULL,
	.users = 1,
	.readers = 1,
	.writers = 1,
#if defined(CONFIG_MEDIA_CONTROLLER_DVB)
	.name = "dvb-acas",
#endif
	.fops = &rdvb_acas_fops,
};

/*************************************************************/
int rdvb_acas_init(void)
{
	int ret = 0;
	struct rdvb_adapter* adapter = rdvb_get_adapter();

#ifdef CONFIG_CUSTOMER_TV006
	ACAS_WARNING("ca:%s isPcbWithSmc = %d \n" , __func__ , isPcbWithSmc());
	if( 0 == isPcbWithSmc() )
	{
		ACAS_WARNING("this pcb is eu type , acas driver is not needed \n");
		return -EINTR;
	}
#endif
	memset(&grdvb_acas_priv,0,sizeof(grdvb_acas_priv));

	/* init grdvb_acas_priv here */
	/* todo */

	/* register the DVB device */
	ret = dvb_register_device(adapter->dvb_adapter, &grdvb_acas_priv.dvbdev, &dvbdev_acas_plus, &grdvb_acas_priv, DVB_DEVICE_ACAS);

	if (ret)
		goto exit;

	if(rdvb_acas_proc_init())
		goto unregister_device;

	mutex_init(&grdvb_acas_priv.ioctl_mutex);

	if (signal_pending(current)) {
		ret = -EINTR;
		goto uninit_debug_proc;
	}
	mb();

	return 0;

uninit_debug_proc:
	rdvb_acas_proc_uninit();

unregister_device:
	dvb_unregister_device(grdvb_acas_priv.dvbdev);

exit:
	return ret;
}

void rdvb_acas_exit(void)
{
#ifdef CONFIG_CUSTOMER_TV006
	ACAS_WARNING("ca:%s isPcbWithPcmcia = %d \n" , __func__ , isPcbWithSmc());
	if( 0 == isPcbWithSmc() )
	{
		ACAS_WARNING("this pcb is eu type , acas driver is not needed \n");
		return ;
	}
#endif

	dvb_unregister_device(grdvb_acas_priv.dvbdev);
	rdvb_acas_proc_uninit();
}

module_init(rdvb_acas_init);
module_exit(rdvb_acas_exit);

MODULE_AUTHOR("xiangzhu_yang@apowertec.com>");
MODULE_DESCRIPTION("RTK TV dvb ACAS Driver");
MODULE_LICENSE("GPL");

