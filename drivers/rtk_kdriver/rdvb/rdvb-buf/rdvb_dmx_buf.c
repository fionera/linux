#include "dmx_buf_mgr.h"
#include "rdvb_dmx_buf.h"
//==============================================================================
int rdvb_dmxbuf_tpbuf_get(enum tp_con_type tct, enum tp_buf_type tbt,
		u32 *phyaddr, u8** viraddr, unsigned int *size, bool iscache)
{
	if (dmxbuf_request_tpbuf(tct, tbt, phyaddr, viraddr,
		 size, iscache) <  0){
		dmx_err(tct,"[ERROR] request tpbuf FAIL. caller:%d, type:%d\n",
			 tct, tbt);
		return -1;
	}
	return 0;
}
int rdvb_dmxbuf_tpbuf_put(enum tp_con_type tct)
{
	if (dmxbuf_release_tpbuf(tct) < 0) {
		dmx_err(tct,"[ERROR] release tpbuf FAIL. caller:%d \n", tct);
		return -1;

	}
	return 0;
}
int rdvb_dmxbuf_request(enum pin_type pin_type, int pin_port, 
	enum host_type host_type, int host_port,
	void* priv, struct buffer_header_info *data)
{
	struct PIN_INFO_T * pin = NULL;

	dmx_err(NOCH, "pin_type:%d, pin_port:%d, host:%d, host_type:%d\n",
		pin_type, pin_port, host_type, host_port);
	pin = dmxbuf_get_pinInfo(pin_type, pin_port);

	switch(pin_type){
	case PIN_TYPE_VTP:
		if (dmxbuf_setup_vtpbuffer(pin)< 0) {
			dmx_err(pin->src, "[ERROR] vtp buf setup fail,"
				"pin(%d/%d) host(%d/%d\n",
				pin_type, pin_port, host_type, host_port);
			return -1;
		}
	break;
	case PIN_TYPE_ATP:
		if (dmxbuf_setup_atpbuffer(pin) < 0) {
			dmx_err(pin->src, "[ERROR] atp buf setup fail,"
				"pin(%d/%d) host(%d/%d\n",
				pin_type, pin_port, host_type, host_port);
			return -1;
		}
	break;
	case PIN_TYPE_PES:
		if (dmxbuf_setup_pesbuffe(pin) <0) {
			dmx_err(pin->src, "[ERROR] PES buf setup fail,"
				"pin(%d/%d) host(%d/%d\n",
				pin_type, pin_port, host_type, host_port);
			return -1;			
		}
		if (priv){
			pin->dest = pin->index ;
			*(int *)priv = pin->pin_index;
		}

	break;
	default:
		return -1;
	break;

	}
	pin = dmxbuf_pin_connect(pin, host_type, host_port);
	if (pin == NULL){
		dmx_err(NOCH, "[ERROR] PIN connect FAIL,pin(%d/%d) host(%d/%d\n",
		 	pin_type, pin_port, host_type, host_port);
		return -1;
	}
	if (data){
		data->bs_header_addr = pin->es.bufHeader.phyaddr;
		data->ib_header_addr = pin->ib.bufHeader.phyaddr;
		data->bs_header_size = pin->es.bufHeader.bufSize;
		data->ib_header_size = pin->ib.bufHeader.bufSize;
	}
	//*priv = pin;
	return 0;
}

int rdvb_dmxbuf_release(enum pin_type pin_type, int pin_port, 
	enum host_type host_type, int host_port)
{

	struct PIN_INFO_T * pin = NULL;

	dmx_err(NOCH, "pin_type:%d, pin_port:%d, host:%d, host_type:%d\n",
		pin_type, pin_port, host_type, host_port);
	pin = dmxbuf_get_pinInfo(pin_type, pin_port);
	pin = dmxbuf_pin_disconnect(pin, host_type, host_port);
	if (pin)
		return 0;
	else
		return -1;
}

int rdvb_dmxbuf_get_decoder_port(enum pin_type pin_type, int pin_port)
{

	struct PIN_INFO_T * pPin = NULL;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);

	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return  -1;
	}
	return dmxbuf_get_decoder_port(pPin);
}

unsigned int rdvb_dmxbuf_get_avail_size(enum pin_type pin_type, int pin_port)
{

	struct PIN_INFO_T * pPin = NULL;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);

	if (pPin == NULL){
		return  -1;
	}
	if (pPin->used==false || pPin->src == -1)
	{
		dmx_err(NOCH, "ERROR pin(%d,%d)\n",pPin->pin_type, pPin->pin_index);
		return -1;
	}
	return dmxbuf_get_avail_size(pPin);
}

int rdvb_dmxbuf_write(enum pin_type pin_type, int pin_port, void *data,
			unsigned int size)
{

	struct PIN_INFO_T * pPin = NULL;

	//dmx_err(NOCH, "@@@@####\n", __func__, __LINE__);
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return  -1;
	}
	if (pPin->used==false || pPin->src == -1)
	{
		dmx_err(NOCH, "ERROR pin(%d,%d)\n",pPin->pin_type, pPin->pin_index);
		return -1;
	}
	if (dmxbuf_write(pPin, data, size) < 0){
		return -1;
	}
	return 0;
}

int rdvb_dmxbuf_sync(enum pin_type pin_type, int pin_port, unsigned char owner)
{
	struct PIN_INFO_T * pPin = NULL;

	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n",	pin_type, pin_port);
		return  -1;
	}
	if (!pPin->used || pPin->src != owner)
		return 0;

	dmxbuf_sync(pPin);
	return 0;
}

int rdvb_dmxbuf_writeIB(enum pin_type pin_type, int pin_port,  
	void *data, unsigned int size, unsigned int *pW, bool is_ecp)
{

	struct PIN_INFO_T * pPin = NULL;

	//dmx_err(NOCH, "@@@@####\n", __func__, __LINE__);
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return  -1;
	}
	if (pPin->used==false || pPin->src == -1)
	{
		dmx_err(NOCH, "ERROR pin(type:%d, num:%d)\n", pPin->pin_type, pPin->pin_index);
		return -1;
	}
	if (dmxbuf_writeIB(pPin, data, size, pW, is_ecp) < 0){
		return -1;
	}
	return 0;
}

int rdvb_dmxbuf_write_rollback(enum pin_type pin_type, int pin_port)
{
	struct PIN_INFO_T * pPin = NULL;
	if (pin_type != PIN_TYPE_PES)
		return -1;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin(%d/%d) NOT find!!\n", pin_type, pin_port);
		return  -1;
	}
	if (pPin->used==false || pPin->src == -1)
	{
		dmx_err(NOCH, "ERROR pin(%d,%d)\n",pPin->pin_type, pPin->pin_index);
		return -1;
	}
	if (dmxbuf_write_rollback(pPin) < 0) {
		dmx_err(NOCH, "ERROR : rollback data!! \n");
		return -1;
	}
	return  0;
}

int rdvb_dmxbuf_trans_pesdata(enum pin_type pin_type, int pin_port, 
	ts_callback callback, void* filter)
{
	struct PIN_INFO_T * pPin = NULL;
	if (pin_type != PIN_TYPE_PES)
		return -1;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	//dmx_err(NOCH, "@@@@####\n", __func__, __LINE__);
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return  -1;
	}
	if (pPin->used==false || pPin->src == -1) {
		dmx_err(NOCH, "ERROR pin(%d,%d)\n",pPin->pin_type, pPin->pin_index);
		return -1;
	}
	if (dmxbuf_trans_pesdata(pPin, callback, filter) < 0){
		dmx_err(NOCH, "ERROR: deliver PES data fail!! \n");
		return -1;
	}
	return 0;
}

int rdvb_dmxbuf_get_dmxPort(enum pin_type pin_type, int pin_port)
{
	struct PIN_INFO_T* pPin = dmxbuf_get_pinInfo (pin_type, pin_port);
	if (pPin == NULL){
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", 
			pin_type, pin_port);
		return  -1;
	}
	if (pPin->used == false || pPin->src == -1){
		dmx_err(NOCH, "ERROR pin (%d/%d)not connect dmx: used:%d, dmx:%d!!!\n", 
			pin_type, pin_port, pPin->used, pPin->src);
		return -1;
	}
	return pPin->src;
}


int rdvb_dmxbuf_Init(unsigned int pollsize)
{
	return dmxbuf_init(pollsize);
}

int rdvb_dmxbuf_poll_alloc(struct BUF_INFO *pbuf, unsigned int size)
{
	if (pbuf == NULL)
		return -1;
	return dmxbuf_poll_alloc(pbuf, size);
}

void rdvb_dmxbuf_Destroy(void)
{
	dmxbuf_uninit();
}

int rdvb_dmxbuf_ta_map(enum pin_type pin_type, int pin_port)
{
	struct PIN_INFO_T *pPin = NULL;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL) {
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return -1;
	}
	if (pPin->used == false) {
		dmx_err(NOCH, "ERROR pin(%d,%d)\n", pPin->pin_type, pPin->pin_index);
		return -1;
	}
	dmxbuf_ta_map(pPin);
	return 0;
}
int rdvb_dmxbuf_ta_unmap(enum pin_type pin_type, int pin_port)
{
	struct PIN_INFO_T *pPin = NULL;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL) {
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return -1;
	}
	if (pPin->used == false) {
		dmx_err(NOCH, "ERROR pin(%d,%d)\n", pPin->pin_type, pPin->pin_index);
		return -1;
	}
	dmxbuf_ta_unmap(pPin);
	return 0;
}

int rdvb_dmxbuf_ta_invalid(enum pin_type pin_type, int pin_port)
{
	struct PIN_INFO_T *pPin = NULL;
	pPin = dmxbuf_get_pinInfo(pin_type, pin_port);
	if (pPin == NULL) {
		dmx_err(NOCH, "ERROR pin (%d/%d) NOT find!!\n", pin_type, pin_port);
		return -1;
	}
	if (pPin->used == false) {
		dmx_err(NOCH, "ERROR pin(%d,%d)\n", pPin->pin_type, pPin->pin_index);
		return -1;
	}
	dmxbuf_ta_invalid(pPin);
	return 0;
}

int rdvb_dmxbuf_ta_map_poll(void)
{
	dmxbuf_ta_map_poll();
	return 0;
}