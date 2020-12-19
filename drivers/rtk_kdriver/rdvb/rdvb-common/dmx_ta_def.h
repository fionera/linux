#ifndef __DMX_TA_DEF__
#define __DMX_TA_DEF__

#define DMX_TA_VERSION "2019-12-06+1527"
//COMMAND
#define DEMUX_TA_ECP_SET_CONTENT_PROTECTION 0
#define DEMUX_TA_ECP_GET_CONTENT_PROTECTION 1
#define DMX_TA_CMD_DATA_PROCESS  2
#define DMX_TA_CMD_BUF_MAP       3
#define DMX_TA_CMD_BUF_UNMAP     4
#define DMX_TA_CMD_DMX_INIT      5
#define DMX_TA_CMD_DMX_UNINIT    6
#define DMX_TA_CMD_TPC_CREATE    7
#define DMX_TA_CMD_TPC_RELEASE   8
#define DMX_TA_CMD_IB_WRITE      9
#define DMX_TA_CMD_PVR_AUTH_VERIFY 10
//BUFFER ID
#define buf_type_vtp0 0
#define buf_type_vtp1 1
#define buf_type_atp0 2
#define buf_type_atp1 3
#define buf_type_pes0 4
#define buf_type_pes1 5
#define buf_type_pes2 6
#define buf_type_pes3 7
#define buf_type_tp   8
#define buf_type_poll 9


struct tpc_info {
	uint16_t pid;
	uint8_t  type;
	uint8_t  dest_port;
};
struct ta_tpc_info{
	uint16_t num;
	struct tpc_info * tpc_infos;
};
struct ta_buf_info {
	uint32_t pa;
	uint32_t pa_size;
	uint32_t hdr_pa;
	uint32_t hdr_pa_size;
	uint32_t i_pa;
	uint32_t i_pa_size;
	uint32_t i_hdr_pa;
	uint32_t i_hdr_pa_size;
};


struct dmx_result{
	uint32_t cur_phy;
	// result
	int64_t apts;
	int64_t vpts;
	uint16_t pes_pid0;
	uint16_t pes_pid1;
};
#endif