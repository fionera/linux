#include "linux/bitmap.h"
#include "error.h"

const char * dmx_err_str[] = {
	"BUF destination(atp/vtp) not connected",
	"BUF source(sdec) not connected",
	"BUF write  fail",
	"BUF full happened",
	"BUF inband write fail",
};

#define errmask_bits(maskp) ((maskp)->bits)

#define ERR_MASK_NONE							\
	(errmask_t) { {							\
	[0 ... BITS_TO_LONGS(DMX_ERR_MAX)-1] =  0UL				\
	} }


errmask_t err_code = ERR_MASK_NONE;
static inline bool errmask_empty(const struct errmask *srcp)
{
	return bitmap_empty(errmask_bits(srcp), DMX_ERR_MAX);
}

void error_help(void)
{
	int i = 0;
	printk("\tID:\tERROR MESSAGE\n ");
	for(i=0; i<DMX_ERR_MAX; i++) {
		printk("\t%02d:\t%s\n ", i, dmx_err_str[i]);
	}
}

void error_clear(errmask_t * err)
{
	bitmap_zero(errmask_bits(err), DMX_ERR_MAX);
}
void error_set(errmask_t * err, enum dmxerr err_bit)
{
	if(err_bit >=  DMX_ERR_MAX || err == NULL)
		return;
	set_bit(err_bit, errmask_bits(err));
}

void error_print()
{
	unsigned int bit;
	int i;
	printk("ERROR CODE: ");
	for(i= BITS_TO_LONGS(DMX_ERR_MAX)-1; i>=0;i--) {
		printk("%lx", errmask_bits(&err_code)[i]);
	}
	printk("\n");
	for_each_set_bit(bit, errmask_bits(&err_code), DMX_ERR_MAX) {
		printk("%s\n", dmx_err_str[bit]);
	}
}