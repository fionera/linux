///////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Realtek Semiconductor Corp.
///////////////////////////////////////////////////////////////////////////////

#include <getopt.h>

#define OPTSTRING "i:s:t:w:h:"
enum {
    OPT_INPUT,
    OPT_SOURCE,
    OPT_TEST,
    OPT_LIST_TESTS,
    OPT_WIDTH,
    OPT_HEIGHT,
    OPT_BUFCNT,
    OPT_HELP
};

static struct option long_options[] = {
    {"input",      required_argument, 0, OPT_INPUT},
    {"source",     required_argument, 0, OPT_SOURCE},
    {"test",       required_argument, 0, OPT_TEST},
    {"list_tests",       no_argument, 0, OPT_LIST_TESTS},
    {"width",      required_argument, 0, OPT_WIDTH},
    {"height",     required_argument, 0, OPT_HEIGHT},
    {"bufcnt",     required_argument, 0, OPT_BUFCNT},
    {"help",             no_argument, 0, OPT_HELP},
    {0, 0, 0, 0}
};

typedef struct {
    char* input;
    int   input_cnt;
    char* source;
    int   source_cnt;
    char* test;
    int   test_cnt;
    char  list_tests;
    int   list_tests_cnt;
    int   width;
    int   width_cnt;
    int   height;
    int   height_cnt;
    int   bufcnt;
    int   bufcnt_cnt;
} opts_t;

typedef enum _rtk_dmadev_cmd_t {
  RTK_DMADEV_CMD_NONE,
  RTK_DMADEV_CMD_DMABUF_ACQUIRE,
  RTK_DMADEV_CMD_DMABUF_RELEASE,
} rtk_dmadev_cmd_t;

typedef enum _rtk_dmadev_status_t {
  RTK_DMADEV_STATUS_OK,
  RTK_DMADEV_STATUS_ERROR,
  RTK_DMADEV_STATUS_INVALID_CMD,
} rtk_dmadev_status_t;

typedef struct _rtk_dmadev_acquire_t {
  unsigned int group;
  unsigned long phyaddr;
  size_t size;
  int flags;
  int fd;
} rtk_dmadev_acquire_t;

typedef union _rtk_dmadev_data_t {
  rtk_dmadev_acquire_t dma_acquire;
} rtk_dmadev_data_t;

typedef struct _rtk_dmadev_request_t {
  rtk_dmadev_cmd_t cmd;
  rtk_dmadev_status_t status;
  rtk_dmadev_data_t data;
} rtk_dmadev_request_t;

typedef struct _rtk_dma_priv_t {
  unsigned long va;
  unsigned long pa;
  size_t size;
} rtk_dma_priv_t;

typedef struct _rtk_dmadev_handle_t {
  REFCLOCK *refclock;
  ulong phyaddr;
  int flags;
  int fd;
  unsigned int group;
  unsigned int num_client;
  uint64_t time;
} __attribute__((__aligned__(256))) rtk_dmadev_handle_t;


typedef struct _rtk_dma_request_t {
	  unsigned long phyaddr;
	  unsigned int attach;
	  size_t size;
	  int fd;
} rtk_dma_request_t;

static void printUsage ()
{
    printf("\t-i, --input file    : input file (Elementary Stream)\n");
    printf("\t-s, --source type   : source type (mpeg2, avc, hevc, avs, avs2) (Default: avc)\n");
    printf("\t-t, --test IDs      : test IDs (Default: 1)\n");
    printf("\t--list_tests        : list tests\n");
    printf("\t-w, --width number  : width (Default: 1920)\n");
    printf("\t-h, --height number : height (Default: 1080)\n");
    printf("\t--bufcnt number     : bufcnt (Default: 10)\n");
}

#define RTK_DMADEC_PROC_DIR "rtkdma"
#define RTK_DMADEC_DEVICE_PATH "/dev/rtkdma"
#define RTK_DMADEC_IOC_MAGIC 'o'
#define RTK_DMADEC_IOC_REQUEST \
  _IOWR(RTK_DMADEC_IOC_MAGIC, 1, rtk_dmadev_request_t)

#define RTK_VDEC_DEVICE_PATH "/dev/rtkvdec"
#define RTK_ADEC_DEVICE_PATH "/dev/rtkaudio"
#define VDEC_IOC_MAGIC  'v'
#define VDEC_IOC_DMA_OPEN   _IOWR(VDEC_IOC_MAGIC,26, rtk_dma_request_t)
#define VDEC_IOC_DMA_CLOSE  _IOWR(VDEC_IOC_MAGIC,27, rtk_dma_request_t)

#define RTKAUDIO_IOC_MAGIC  'a'
#define RTKAUDIO_IOC_DMA_OPEN               _IOWR(RTKAUDIO_IOC_MAGIC, 16, rtk_dma_request_t)
#define RTKAUDIO_IOC_DMA_CLOSE              _IOWR(RTKAUDIO_IOC_MAGIC, 17, rtk_dma_request_t)


static void parseArgs(opts_t *opts, int argc, char **argv)
{
    int opt;
    while ((opt = getopt_long (argc, argv, OPTSTRING, long_options, NULL)) != -1)
        switch (opt) {
            case 'i':
            case OPT_INPUT:
                opts->input = optarg;
                opts->input_cnt++;
                break;
            case 's':
            case OPT_SOURCE:
                opts->source = optarg;
                opts->source_cnt++;
                break;
            case 't':
            case OPT_TEST:
                opts->test = optarg;
                opts->test_cnt++;
                break;
            case OPT_LIST_TESTS:
                opts->list_tests = 1;
                opts->list_tests_cnt++;
                break;
            case 'w':
            case OPT_WIDTH:
                opts->width = strtoul(optarg, NULL, 0);
                opts->width_cnt++;
                break;
            case 'h':
            case OPT_HEIGHT:
                opts->height = strtoul(optarg, NULL, 0);
                opts->height_cnt++;
                break;
            case OPT_BUFCNT:
                opts->bufcnt = strtoul(optarg, NULL, 0);
                opts->bufcnt_cnt++;
                break;
            case OPT_HELP:
            default:
                printUsage();
                exit(0);
                break;
        }
}

opts_t opts = {
    .source = "avc",
    .test = "1",
    .width = 1920,
    .height = 1080,
    .bufcnt = 10,
};
