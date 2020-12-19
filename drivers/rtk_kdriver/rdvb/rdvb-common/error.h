#ifndef __DMX_ERROR__
#define __DMX_ERROR__

enum dmxerr {
	DMX_ERR_BUF_NO_DEST,
	DMX_ERR_BUF_NO_SRC,
	DMX_ERR_BUF_WRITE_FAIL,
	DMX_ERR_BUF_FULL,
	DMX_ERR_BUF_INBAND_WRITE,
	
	DMX_ERR_MAX,
};

typedef struct errmask { DECLARE_BITMAP(bits, DMX_ERR_MAX); } errmask_t;
void error_clear(errmask_t * err);
void error_set(errmask_t * err, enum dmxerr err_bit);
void error_help(void);
void error_print(void);
#endif