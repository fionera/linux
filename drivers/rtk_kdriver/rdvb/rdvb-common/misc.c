#include <linux/kernel.h>
#include <linux/slab.h>
#include "misc.h"
unsigned int rdvb_log_level = 0;

char * print_to_buf(char *buf, const char *fmt,...)
{
	ssize_t count =0;
	va_list args;
	va_start(args, fmt);
	if (buf)
		count = vsprintf(buf, fmt, args);
	else
		vprintk(fmt, args);
	va_end(args);
	return buf + count;
}

void set_log_level(unsigned int level)
{
	rdvb_log_level = level;
}
unsigned int get_log_level(void)
{
	return rdvb_log_level;
}
u32 reverse_int32(u32 value)
{
	u32 b0 =  value & 0x000000ff;
	u32 b1 = (value & 0x0000ff00) >> 8;
	u32 b2 = (value & 0x00ff0000) >> 16;
	u32 b3 = (value & 0xff000000) >> 24;

	return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}
u64 reverse_int64(u64 value)
{
	u64 ret;
	unsigned char *des, *src;
	src = (unsigned char *)&value;
	des = (unsigned char *)&ret;
	des[0] = src[7];
	des[1] = src[6];
	des[2] = src[5];
	des[3] = src[4];
	des[4] = src[3];
	des[5] = src[2];
	des[6] = src[1];
	des[7] = src[0];
	return ret;
}
