#include <linux/platform_device.h>
#include <linux/module.h>
#include <dvb_ca_en50221.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <tp/tp_drv_global.h>
#include <ca-ext.h>

#include "rtk_pcmcia.h"
#include "rtk_lg_board.h"
#include "rdvb_ca_parse.h"
#include "rdvb_ca.h"
#include "rdvb-adapter/rdvb_adapter.h"
#include "rdvb-dmx/rdvb_dmx.h"


/************** globa para *****************/
#define CA_CTRLIF_DATA 0
RDVB_CA  g_rdvb_ca;
UINT8 rdvb_ca_dbg_log_mask = 0;
extern RTK_PCMCIA *rtk_pcmcia[2];
extern RTK_DVB_CA_FUNC rtk_rdvb_func;
void rdvb_ca_ready_irq(void);

int rdvb_read_attribute_mem(struct dvb_ca_en50221 *ca, int slot, int address)
{
    unsigned char attrdata = 0;
    int ret;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND ){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    if((ret = rtk_pcmcia_fifo_read(p_this, address, &attrdata, 1, PCMCIA_FLAGS_ATTR)) != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_fifo_read failed.  ret=%d\n", __func__, __LINE__, ret);
    }
    RDVB_CA_ATTR_DEBUG("%s, address=0x%x, attr=0x%02x \n", __func__, address,attrdata);

    return attrdata;
}

int rdvb_write_attribute_mem(struct dvb_ca_en50221 *ca, int slot, int address, u8 value)
{
    int  ret = 0;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    RDVB_CA_ATTR_DEBUG("%s, address=0x%x, value=0x%02x \n", __func__, address,value);

    ret = rtk_pcmcia_fifo_write(p_this, address, &value, 1, PCMCIA_FLAGS_ATTR);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_fifo_write failed.  ret=%d\n", __func__, __LINE__, ret);
    }

    return ret;
}

int rdvb_read_cam_control(struct dvb_ca_en50221 *ca, int slot, u8 address)
{
    unsigned char contrdata = 0;
    int ret;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    ret = rtk_pcmcia_fifo_read(p_this, address, &contrdata, 1, PCMCIA_FLAGS_IO);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] failed. rtk_pcmcia_fifo_read ret=%d\n", __func__, __LINE__, ret);
    }
    RDVB_CA_IO_DEBUG("%s, address=0x%x, contrdata=0x%02x \n", __func__, address,contrdata);
	
    return contrdata;
}

int rdvb_write_cam_control(struct dvb_ca_en50221 *ca, int slot, u8 address, u8 value)
{
    int  ret = 0;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    RDVB_CA_IO_DEBUG("%s, address=0x%x, value=0x%02x \n", __func__, address,value);

    ret = rtk_pcmcia_fifo_write(p_this, address, &value, 1, PCMCIA_FLAGS_IO);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_fifo_write failed.  ret=%d\n", __func__, __LINE__, ret);
    }

    return ret;
}

int rdvb_read_cam_multi_data(struct dvb_ca_en50221 *ca, int slot, unsigned short len, unsigned char* p_data)
{
    int ret;
    unsigned char   flags;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    flags = PCMCIA_FLAGS_IO | PCMCIA_FLAGS_RD | PCMCIA_FLAGS_FIFO;

    ret = rtk_pcmcia_fifo_read(p_this, CA_CTRLIF_DATA, p_data, len, flags);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] failed. rtk_pcmcia_fifo_read ret=%d\n", __func__, __LINE__, ret);
    }
    RDVB_CA_IO_DEBUG("%s, address=0x%x, len=0x%02x \n", __func__, CA_CTRLIF_DATA, len);

    return ret;
}

int rdvb_write_cam_multi_data(struct dvb_ca_en50221 *ca, int slot, unsigned short len, unsigned char* p_data)
{
    int ret;
    unsigned char   flags;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    flags = PCMCIA_FLAGS_IO | PCMCIA_FLAGS_WR | PCMCIA_FLAGS_FIFO;

    ret = rtk_pcmcia_fifo_write(p_this, CA_CTRLIF_DATA, p_data, len, flags);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] failed. rtk_pcmcia_fifo_read ret=%d\n", __func__, __LINE__, ret);
    }
    RDVB_CA_IO_DEBUG("%s, address=0x%x, len=0x%02x \n", __func__, CA_CTRLIF_DATA, len);

    return ret;
}


int rdvb_slot_reset(struct dvb_ca_en50221 *ca, int slot)
{
    int ret;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }
    ret = rtk_pcmcia_card_enable(p_this, CA_ENABLE);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_card_enable CA_ENABLE failed.  ret=%d\n", __func__, __LINE__, ret);
    }
    msleep(10);
    ret = rtk_pcmcia_card_reset(p_this, DEFAULT_PCMCIA_RESET_TIMEOUT);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_card_reset failed.  ret=%d\n", __func__, __LINE__, ret);
    }
    rdvb_ca_ready_irq();
    RDVB_CA_MUST_PRINT("[%s], done success\n", __func__);
    return ret;
}

int rdvb_slot_shutdown(struct dvb_ca_en50221 *ca, int slot)
{
    int ret;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        RDVB_CA_ERROR("[%s, %d], no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND ){
        RDVB_CA_ERROR("[%s, %d], Cam not ready!!! \n", __func__, __LINE__);
        return -1;
    }

    ret = rtk_pcmcia_card_enable(p_this, CA_DISABLE);
    if(ret != PCMCIA_OK){
        RDVB_CA_ERROR("[%s, %d] rtk_pcmcia_card_disable failed.  ret=%d\n", __func__, __LINE__, ret);
    }

    RDVB_CA_MUST_PRINT("[%s], done success\n", __func__);
    return ret;
}

int rdvb_slot_ts_enable(struct dvb_ca_en50221 *ca, int slot)
{
    int ret;
    ret = RHAL_TPOUT_Enable(0, TPOUT_STREAM_START);
    if(ret != TPK_SUCCESS){
        RDVB_CA_ERROR("[%s, %d] RHAL_TPOUT_Enable failed.  ret=%d\n", __func__, __LINE__, ret);
    }else{
        RDVB_CA_MUST_PRINT("[%s]  success\n", __func__);
    }

    return ret;
}

int rdvb_poll_slot_status(struct dvb_ca_en50221 *ca, int slot, int open)
{
    int ca_status = 0;
    int status;
    RDVB_CA *rdvb_ca   = ca->data;
    RTK_PCMCIA *p_this = rdvb_ca->rtkpcmcia[0];

    if(p_this == NULL){
        //printk(KERN_ERR "ca: F:%s,L:%d, no ca init !!! \n", __func__, __LINE__);
        return -1;
    }
    if(rtk_pcmcia_str_state_get() == PCMCIA_STATUS_SUSPEND){
        return rdvb_ca->laster_status;
    }

    status = rtk_pcmcia_get_card_status(p_this);
    if(status & RTK_PCMCIA_STS_CARD_PRESENT)
        ca_status = DVB_CA_EN50221_POLL_CAM_PRESENT | DVB_CA_EN50221_POLL_CAM_READY;
    else if(status & RTK_PCMCIA_STS_ACCESS_FAULT)
        ca_status = DVB_CA_EN50221_POLL_CAM_CHANGED;
    else if(status & RTK_PCMCIA_STS_CARD_READY)
        ca_status = DVB_CA_EN50221_POLL_CAM_READY;

    rdvb_ca->laster_status = ca_status;

    return ca_status;
}

int rdvb_set_input_source(struct dvb_ca_en50221 *ca, int slot, struct ca_ext_source *ca_src)
{
    int ret = 0;
    RDVB_CA *rdvb_ca = ca->data;
    TPK_INPUT_PORT_T portType    	= TPK_PORT_NONE;
    TPK_TP_SOURCE_T tpp_src      	= TS_UNMAPPING;
    TPK_TPOUT_SOURCE_T tpout_src  	= TPK_TPOUT_UNMAPPING;
    #define FIX_TO_TPP 1 	
	
    RDVB_CA_MUST_PRINT("[%s, %d], input_src_type=%d(0:INDEMOD/1:EXTDEMOD/2:CIP/3:MEM/4:NULL), input_port_num=%d!!! \n", __func__, __LINE__,ca_src->input_src_type,ca_src->input_port_num);

    if(ca_src->input_port_num > 2){
        RDVB_CA_ERROR("[%s, %d] failed. Num:%d, input port num >2  !!! \n", __func__, __LINE__, ca_src->input_port_num);
        return -1;
    }

    switch(ca_src->input_src_type){
        case CA_EXT_SRC_TYPE_IN_DEMOD:
			
#ifdef FIX_TO_TPP 
            tpout_src 	= TPK_TPP_TO_TPOUT;
#else
            tpout_src 	= TPK_IN_DEMOD_TO_TPOUT;       
#endif
            tpp_src      = Internal_Demod;
            break;
        case CA_EXT_SRC_TYPE_EXT_DEMOD:
            if(ca_src->input_port_num == 0)
                portType  	= TPK_PORT_EXT_INPUT0;
            else if(ca_src->input_port_num == 1)
                portType  	= TPK_PORT_EXT_INPUT1;
            else if(ca_src->input_port_num == 2)
                portType  	= TPK_PORT_EXT_INPUT2;
            tpp_src  	= RHAL_GetTPSource(portType);
			
#ifdef FIX_TO_TPP 
            tpout_src	= TPK_TPP_TO_TPOUT;
#else
            if(tpp_src == TS_SRC_0)
                tpout_src = TPK_TPI0_TO_TPOUT;
            else if(tpp_src == TS_SRC_1)
                tpout_src = TPK_TPI1_TO_TPOUT;
            else if(tpp_src == TS_SRC_2)
                tpout_src = TPK_TPI2_TO_TPOUT;
#endif 
            break;
        case CA_EXT_SRC_TYPE_MEM:
			
#ifdef FIX_TO_TPP 
            tpout_src	= TPK_TPP_TO_TPOUT;
#else
            tpout_src 	= TPK_TP_MTP_TO_TPOUT;
#endif 
            tpp_src  	= MTP;
            break;
        case CA_EXT_SRC_TYPE_CIP:
            tpout_src = TPK_TPP_TO_TPOUT;
            tpp_src  	= Internal_Demod;
            break;
        default:
            tpout_src = TPK_TPOUT_UNMAPPING;
            break;
    }

    rdvb_ca->ca_src.input_port_num = ca_src->input_port_num;
    rdvb_ca->ca_src.input_src_type = ca_src->input_src_type;

    if(tpout_src == TPK_TPP_TO_TPOUT && (RHAL_TPP_GetDataSource(TP_TPP0) != (INT32)tpp_src)){

        RHAL_TPP_OutEnable(TP_TPP0,0);
		
        RHAL_TPP_StreamControl(TP_TPP0,TPP_STREAM_STOP);
		
        if (RHAL_TPP_SetDataSource(TP_TPP0, tpp_src) != TPK_SUCCESS)
            return -1;

        RHAL_TPP_StreamControl(TP_TPP0,TPP_STREAM_START);

        RHAL_TPP_OutEnable(TP_TPP0,1);
    }
	
    if ((ret = RHAL_TPOUT_SetDataSource(0, tpout_src)) != 0){
        RDVB_CA_ERROR("[%s, %d] RHAL_TPOUT_SetDataSource failed. tpout_src=%d (-1:NONE/0:MTP2TPO/1:TPP/2:INDEMOD/3:EXTDEMOD0/4:EXTDEMOD1/5:EXTDEMOD2), ret=%d\n", __func__, __LINE__, tpout_src ,ret);
    }else{
        RDVB_CA_MUST_PRINT("[%s, %d], done success tpout_src=%d (-1:NONE/0:MTP2TPO/1:TPP/2:INDEMOD/3:EXTDEMOD0/4:EXTDEMOD1/5:EXTDEMOD2) \n", __func__, __LINE__,tpout_src);
    }

    return ret;
}


int rdvb_get_input_source(struct dvb_ca_en50221 *ca, int slot, struct ca_ext_source *ca_src)
{
    RDVB_CA *rdvb_ca   = ca->data;

    ca_src->input_port_num = rdvb_ca->ca_src.input_port_num;
    ca_src->input_src_type = rdvb_ca->ca_src.input_src_type;

    RDVB_CA_MUST_PRINT("[%s, %d], input_port_num:%d, input_src_type:%d(0:INDEMOD/1:EXTDEMOD/2:CIP/3:MEM/4:NULL) \n", __func__, __LINE__,
        ca_src->input_port_num, ca_src->input_src_type);
    return 0;
}

int rdvb_parse_cam_version(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size)
{
    RDVB_CA *rdvb_ca   = ca->data;

    #if 0
    int i;
    for (i = 0; i<size; i++)
        printk(KERN_ERR "ca: F:%s,L:%d, 0x%x\n", __func__, __LINE__, para[i]);
    #endif
	
    memcpy(&rdvb_ca->ci_camInfo.cistpl_ver1.data,para,size);
    rdvb_ca->ci_camInfo.cistpl_ver1.size = size;

    CI_ParseCistplVer1(&(rdvb_ca->ci_camInfo), para, size);

    RDVB_CA_MUST_PRINT("[%s, %d],found:%d, prof:0x%x, ciplus ver:0x%x, ciplus enable:%d \n", __func__, __LINE__,
          rdvb_ca->ci_camInfo.stCIS.bFoundVers1,
          rdvb_ca->ci_camInfo.stCIS.u32CiPlusProfile,
          rdvb_ca->ci_camInfo.stCIS.u32CiPlusVersion,
          rdvb_ca->ci_camInfo.stCIS.bCiPlusEnable);

    return 0;
}

int rdvb_get_ca_plus_version(struct dvb_ca_en50221 *ca, int slot, signed long long *ver)
{
    RDVB_CA *rdvb_ca   = ca->data;

    *ver = rdvb_ca->ci_camInfo.stCIS.u32CiPlusVersion;
    RDVB_CA_MUST_PRINT("[%s, %d], ciplus version:0x%x, ciplus enable:%d \n", __func__, __LINE__,
          rdvb_ca->ci_camInfo.stCIS.u32CiPlusVersion,
          rdvb_ca->ci_camInfo.stCIS.bCiPlusEnable);

    return 0;
}

int rdvb_get_ca_plus_enable(struct dvb_ca_en50221 *ca, int slot)
{
    RDVB_CA *rdvb_ca   = ca->data;

    RDVB_CA_NORMAL_DEBUG("[%s, %d],  ciplus enable:%d \n", __func__, __LINE__, rdvb_ca->ci_camInfo.stCIS.bCiPlusEnable);
         
    return rdvb_ca->ci_camInfo.stCIS.bCiPlusEnable;
}

int rdvb_parse_cam_manfid(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size)
{
    RDVB_CA *rdvb_ca   = ca->data;
    memcpy(&rdvb_ca->ci_camInfo.cistpl_manfid.data,para,size);
    rdvb_ca->ci_camInfo.cistpl_manfid.size = size;
    return 0;
}

int rdvb_parse_cam_ireq_enable(struct dvb_ca_en50221 *ca, int slot, u8 *para, u8 size)
{
    RDVB_CA *rdvb_ca   = ca->data;

    CI_ParseCistplCftableEntry(&(rdvb_ca->ci_camInfo), para, size);

    RDVB_CA_MUST_PRINT("[%s, %d],bIreqEnable:%d, bFoundCFtableEntry:0x%x\n", __func__, __LINE__,
          rdvb_ca->ci_camInfo.stCIS.bFoundCFtableEntry,
          rdvb_ca->ci_camInfo.stCIS.bIreqEnable);

    return 0;
}

int rdvb_get_ca_ireq_enable(struct dvb_ca_en50221 *ca, int slot)
{
    RDVB_CA *rdvb_ca   = ca->data;

    RDVB_CA_MUST_PRINT("[%s, %d],  bIreqEnable:%d \n", __func__, __LINE__, rdvb_ca->ci_camInfo.stCIS.bIreqEnable);

    return rdvb_ca->ci_camInfo.stCIS.bIreqEnable;
}


RDVB_CA* rdvb_get_ca(void)
{
    return &g_rdvb_ca;
}

extern const struct file_operations ca_status_proc_fops;

static struct proc_dir_entry * dvbca_proc = NULL;
static void dvb_ca_proc_register(void)
{
	dvbca_proc = rdvb_adap_register_proc("lgtv-driver/dvb_ca");
	if (dvbca_proc)
		proc_create("status", 0444, dvbca_proc, &ca_status_proc_fops);
}
static void dvb_ca_proc_unregister(void)
{
	if (dvbca_proc) {
		proc_remove(dvbca_proc);
		dvbca_proc = NULL;
	}
}

static int _init_tpo_config(void){
    TPK_TPOUT_SOURCE_T tpout_src = TPK_TPP_TO_TPOUT;
	
    if(tpout_src == TPK_TPP_TO_TPOUT){
		
		//init tpp 
		RHAL_TPP_Init(TP_TPP0);
		
		//set tpp source
		if (RHAL_TPP_SetDataSource(TP_TPP0, Internal_Demod) != TPK_SUCCESS)
			return -1;
		
		//set tpp sync replace
		if (RHAL_TPP_SetSyncReplace(TP_TPP0, 0x47) != TPK_SUCCESS)
			return -1;
		
		//set tpp output to tpo
		RHAL_TPP_OutEnable(TP_TPP0,1);

		//set tpp stream start
		if (RHAL_TPP_StreamControl(TP_TPP0,TPP_STREAM_START) != TPK_SUCCESS)
			return -1;
		
		//set tpo source to tpp
		if (RHAL_TPOUT_SetDataSource(0, TPK_TPP_TO_TPOUT) != TPK_SUCCESS)
			return -1;
		
	}
    /*
    else
    {
        // coverity :dead code
		if (RHAL_TPOUT_SetDataSource(0, TPK_IN_DEMOD_TO_TPOUT) != TPK_SUCCESS)
			return -1;
	}
    */
	
	RDVB_CA_MUST_PRINT("[%s, %d], done success\n", __func__, __LINE__);
	return 0;
}

int rdvb_slot_enable(struct dvb_ca_en50221 *ca, int slot, int enable)
{
    rtk_pcmcia_enable(rtk_pcmcia[0], CA_ENABLE);
    RDVB_CA_MUST_PRINT("[%s. %d] enable pcmcia detect pin\n", __func__, __LINE__);
    return 0;
}


void rdvb_ca_camchange_irq(int cam_type)
{
    int change_type = 0xFF;
    if(0 == cam_type)
        change_type = DVB_CA_EN50221_CAMCHANGE_REMOVED;
    else if(1 == cam_type)
        change_type = DVB_CA_EN50221_CAMCHANGE_INSERTED;

    if(NULL != g_rdvb_ca.rdvb_ca_camchange_irq)
        g_rdvb_ca.rdvb_ca_camchange_irq(&g_rdvb_ca.pubca, 0, change_type);

    RDVB_CA_MUST_PRINT("[%s,happend. %d], cam_type:%d\n", __func__, __LINE__, cam_type);
}

void rdvb_ca_ready_irq(void)
{
    if(NULL != g_rdvb_ca.rdvb_ca_camready_irq)
        g_rdvb_ca.rdvb_ca_camready_irq(&g_rdvb_ca.pubca, 0);
}

void rdvb_ca_frda_irq(void)
{
}

void rdvb_install_camchange_irq_func(struct dvb_ca_en50221 *ca, void (*dvb_ca_en50221_camchange_irq)(struct dvb_ca_en50221 *pubca, int slot, int change_type) )
{
    g_rdvb_ca.rdvb_ca_camchange_irq = dvb_ca_en50221_camchange_irq;
}

void rdvb_install_camready_irq_func(struct dvb_ca_en50221 *ca, void (*dvb_ca_en50221_camready_irq)(struct dvb_ca_en50221 *pubca, int slot) )
{
    g_rdvb_ca.rdvb_ca_camready_irq = dvb_ca_en50221_camready_irq;
}

void rdvb_install_frda_irq_func(struct dvb_ca_en50221 *ca, void (*dvb_ca_en50221_frda_irq)(struct dvb_ca_en50221 *pubca, int slot) )
{
    g_rdvb_ca.rdvb_ca_frda_irq = dvb_ca_en50221_frda_irq;
}

int rdvb_str_state_get(void)
{
	int rdvb_str;
	unsigned long resume_time;
	unsigned long timeout = msecs_to_jiffies(5000);
	
	resume_time = rtk_pcmcia_get_resume_time();
	rdvb_str = rtk_pcmcia_str_state_get();
	if(((jiffies - resume_time) > timeout) && (rdvb_str == PCMCIA_STATUS_RESUME)){
		rdvb_str = PCMCIA_STATUS_NORMOL;
	}
	return rdvb_str;
}

void rdvb_slot_shutdown_ext(void)
{
    dvb_ca_en50221_slot_shutdown_ext(&g_rdvb_ca.pubca, 0);
}

void rdvb_str_state_set(int str)
{
	rtk_pcmcia_str_state_set(str);
}
/*************************************************************/
int rdvb_ca_init(void)
{
    int ret = -1;
    g_rdvb_ca.rdvb_adapter = rdvb_get_adapter();
    if (g_rdvb_ca.rdvb_adapter == NULL)
        return -ENOMEM;

#ifdef CONFIG_CUSTOMER_TV006
    RDVB_CA_MUST_PRINT("%s isPcbWithPcmcia = %d \n" , __func__ , isPcbWithPcmcia());
    if( 0 == isPcbWithPcmcia() )
    {
        RDVB_CA_ERROR("this pcb is japan type , ca driver is not needed !!!\n");
        RDVB_CA_ERROR("this pcb is japan type , ca driver is not needed !!!\n");
        RDVB_CA_ERROR("this pcb is japan type , ca driver is not needed !!!\n");
        return -ENODEV;
    }
#endif

    g_rdvb_ca.pubca.owner = THIS_MODULE;
    g_rdvb_ca.pubca.read_attribute_mem  = rdvb_read_attribute_mem;
    g_rdvb_ca.pubca.write_attribute_mem = rdvb_write_attribute_mem;
    g_rdvb_ca.pubca.read_cam_control    = rdvb_read_cam_control;
    g_rdvb_ca.pubca.write_cam_control   = rdvb_write_cam_control;
    g_rdvb_ca.pubca.slot_reset       	= rdvb_slot_reset;
    g_rdvb_ca.pubca.slot_shutdown    	= rdvb_slot_shutdown;
    g_rdvb_ca.pubca.slot_ts_enable   	= rdvb_slot_ts_enable;
    g_rdvb_ca.pubca.poll_slot_status 	= rdvb_poll_slot_status;
    g_rdvb_ca.pubca.set_input_source 	= rdvb_set_input_source;
    g_rdvb_ca.pubca.get_input_source 	= rdvb_get_input_source;
    g_rdvb_ca.pubca.parse_cam_version 	= rdvb_parse_cam_version;
    g_rdvb_ca.pubca.get_ca_plus_version = rdvb_get_ca_plus_version;
    g_rdvb_ca.pubca.get_ca_plus_enable  = rdvb_get_ca_plus_enable;
    g_rdvb_ca.pubca.parse_cam_manfid 	= rdvb_parse_cam_manfid;

    g_rdvb_ca.pubca.parse_cam_ireq    	= rdvb_parse_cam_ireq_enable;
    g_rdvb_ca.pubca.get_ca_ireq      	= rdvb_get_ca_ireq_enable;
	
    g_rdvb_ca.pubca.slot_enable         = rdvb_slot_enable;
    g_rdvb_ca.pubca.install_camchange_irq = rdvb_install_camchange_irq_func;
    g_rdvb_ca.pubca.install_camready_irq  = rdvb_install_camready_irq_func;
    g_rdvb_ca.pubca.install_frda_irq      = rdvb_install_frda_irq_func;
    g_rdvb_ca.pubca.str_status_get = rdvb_str_state_get;
    g_rdvb_ca.pubca.str_status_set = rdvb_str_state_set;
    g_rdvb_ca.pubca.read_cam_multi_data = rdvb_read_cam_multi_data;
    g_rdvb_ca.pubca.write_cam_multi_data = rdvb_write_cam_multi_data;
    g_rdvb_ca.pubca.data = &g_rdvb_ca;
    g_rdvb_ca.laster_status = DVB_CA_EN50221_POLL_CAM_PRESENT;

    _init_tpo_config();

    ret = dvb_ca_en50221_init(g_rdvb_ca.rdvb_adapter->dvb_adapter, &g_rdvb_ca.pubca, DVB_CA_EN50221_FLAG_IRQ_CAMCHANGE|DVB_CA_EN50221_FLAG_IRQ_FR|DVB_CA_EN50221_FLAG_IRQ_DA, 1);
    if (ret)
        goto err;

    g_rdvb_ca.rtkpcmcia[0] = rtk_pcmcia[0];

    dvb_ca_proc_register();

    rtk_rdvb_func.rtk_pcmcia_change_irq = rdvb_ca_camchange_irq;
    rtk_rdvb_func.rtk_pcmcia_ready_irq  = rdvb_ca_ready_irq;
    rtk_rdvb_func.rtk_pcmcia_frda_irq   = rdvb_ca_frda_irq;
    rtk_rdvb_func.rtk_pcmcia_shutdown = rdvb_slot_shutdown_ext;
    RDVB_CA_MUST_PRINT("[%s, %d], done success\n", __func__, __LINE__);
    return 0;

err:
    RDVB_CA_ERROR("[%s, %d], fail!!!!!\n", __func__, __LINE__);
    return -ENOMEM;
}

void rdvb_ca_exit(void)
{
#ifdef CONFIG_CUSTOMER_TV006
    RDVB_CA_MUST_PRINT("%s isPcbWithPcmcia = %d \n" , __func__ , isPcbWithPcmcia());
    if( 0 == isPcbWithPcmcia() )
    {
        RDVB_CA_ERROR("this pcb is japan type , ca driver is not needed !!!\n");
        return ;
    }
#endif
    rtk_rdvb_func.rtk_pcmcia_change_irq = NULL;
    rtk_rdvb_func.rtk_pcmcia_ready_irq  = NULL;
    rtk_rdvb_func.rtk_pcmcia_frda_irq   = NULL;
    dvb_ca_proc_unregister();

    dvb_ca_en50221_release(&g_rdvb_ca.pubca);
}

module_init(rdvb_ca_init);
module_exit(rdvb_ca_exit);

MODULE_AUTHOR("xiangzhu_yang@apowertec.com>");
MODULE_DESCRIPTION("RTK TV dvb CA Driver");
MODULE_LICENSE("GPL");

