#ifndef __V4L2_VDEC_TEST_DEF_H__
#define __V4L2_VDEC_TEST_DEF_H__

//#define BS_HDR_ADDR 0x038c0000
//#define IB_HDR_ADDR 0x038c1000
//#define REFCLK_ADDR 0x038c2000

//#define BS_BUF_ADDR 0x1b200000
//#define BS_BUF_SIZE 0x01000000
//#define IB_BUF_ADDR 0x03800000
//#define IB_BUF_SIZE 0x00040000


#define ENC_BS_HDR_ADDR 0x038c0000
#define ENC_MSG_HDR_ADDR 0x038c1000
#define DIN_IB_HDR_ADDR 0x038c2000
#define DIN_BS_HDR_ADDR 0x038c3000


#define ENC_BS_BUF_ADDR 0x1b200000
#define ENC_BS_BUF_SIZE 0x01000000

#define ENC_MSG_BUF_ADDR 0x03803000
#define ENC_MSG_BUF_SIZE 0x00002000


#define DIN_IB_BUF_ADDR 0x03800000
#define DIN_IB_BUF_SIZE 0x00002000

#define DIN_BS_BUF_ADDR 0x1b300000
#define DIN_BS_BUF_SIZE 0xfd2000//0x01000000

typedef struct {
        unsigned int   *cached;
        unsigned int   *uncached;
        unsigned int   phy;
        unsigned int   size;

}PLI_BUF;


#define ALLOCFRAME_NUM 8


#endif
