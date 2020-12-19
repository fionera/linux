#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>

#include <linux/videodev2.h>

#pragma scalar_storage_order big-endian
#include <rpc_common.h>
#include <InbandAPI.h>
#include <VideoRPC_System.h>
#pragma scalar_storage_order default

#include "syswrap.h"
#include "v4l2_vdec_ext.h"
#include "v4l2_venc_test_def.h"
#include "ringbuf.h"
#include "options_venc.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))


#define IF_ERR_EXIT(err_code) \
    if (err_code) { \
        printf("%s:%d ret %d\n", __func__, __LINE__, err_code); \
        exit(-1); \
    }

static struct v4l2_ext_controls ext_controls_arg;
static struct v4l2_ext_control ext_control_arg;
//static struct v4l2_ext_picinfo_msg picinfo_msg;
//static struct v4l2_ext_stream_info stream_info;
//static struct v4l2_ext_decoder_status decoder_status;
PLI_BUF 		  frame[ALLOCFRAME_NUM] ;
static unsigned int input_frame=0;//,timeout_count=0;

struct buffer {
        void   *start;
        size_t  length;
};
struct buffer          *buffers;
static unsigned int     n_buffers;


#define MAX_STREAM_SIZE 0x10000;//(2*1024*1024)
/*
typedef struct cap_buf_t {
	struct v4l2_buffer buffer;
	struct v4l2_plane planes[VIDEO_MAX_PLANES];
	int expbuf_fd[VIDEO_MAX_PLANES];
} cap_buf_t;
static cap_buf_t cap_bufs[64];
*/


typedef struct lookup_entry_t {
    char name[32];
    int value;
} lookup_entry_t;

typedef struct test_entry_t {
    int id;
    int (*fun)(void);
    char desc[128];
} test_entry_t;

static lookup_entry_t lookups[] = {
	{"mpeg2", V4L2_PIX_FMT_MPEG2},
	{"avc", V4L2_PIX_FMT_H264},
	{"h264", V4L2_PIX_FMT_H264},
	{"hevc", V4L2_PIX_FMT_HEVC},
	{"h265", V4L2_PIX_FMT_HEVC},
	{"avs",	V4L2_PIX_FMT_AVS},
	{"avs2", V4L2_PIX_FMT_AVS2},
};

static unsigned long reverseInteger(unsigned long value)
{
    unsigned long b0 = value & 0x000000ff;
    unsigned long b1 = (value & 0x0000ff00) >> 8;
    unsigned long b2 = (value & 0x00ff0000) >> 16;
    unsigned long b3 = (value & 0xff000000) >> 24;

    return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
}


static int lookup(const char *name)
{
	int i, cnt;

	cnt = sizeof(lookups)/sizeof(lookup_entry_t);

	for (i=0; i<cnt; i++)
		if (!strcasecmp(lookups[i].name, name))
			return lookups[i].value;

	printf("Error! lookup %s fails\n", name);
	exit(1);
	return -1;
}


static int s_ctrl(int fd, unsigned int id, int val)
{
    struct v4l2_control control_arg;

    control_arg.id = id;
    control_arg.value = val;

    return ioctl(fd, VIDIOC_S_CTRL, &control_arg);
}

static int g_ctrl(int fd, unsigned int id, int *val)
{
    int ret;
    struct v4l2_control control_arg;

    control_arg.id = id;
    control_arg.value = 0;

    ret = ioctl(fd, VIDIOC_G_CTRL, &control_arg);
    *val = control_arg.value;

    return ret;
}

static int s_ext_ctrls_ptr(int fd, int id, void *ptr)
{
	int ret;

	ext_controls_arg.count = 1;
	ext_controls_arg.controls = &ext_control_arg;

	ext_control_arg.id = id;
	ext_control_arg.ptr = ptr;

	ret = ioctl(fd, VIDIOC_S_EXT_CTRLS, &ext_controls_arg);
	if (ret) {
		printf("%s: errno %d %s\n",  __func__, errno, strerror(errno));
		return ret;
	}

	return 0;
}

static int streamon(int fd)
{
    int v4l2_buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    return ioctl(fd, VIDIOC_STREAMON, &v4l2_buf_type);
}

#if 0
static int subscribe(int fd, unsigned int type, unsigned int id)
{
    struct v4l2_event_subscription subscription_arg;

    subscription_arg.type = type;
    subscription_arg.id = id;

    return ioctl(fd, VIDIOC_SUBSCRIBE_EVENT, &subscription_arg);
}
#endif

static int dqevent(int fd)
{
    int ret;
    struct pollfd poll_fd;
    struct v4l2_event event_arg = {0};
    //int nFrmInfo, nPicInfo, nUserData;

    poll_fd.fd = fd;
    poll_fd.events = POLLPRI;
    //nFrmInfo = nPicInfo = nUserData = 0;

    while (1) {

        ret = poll(&poll_fd, 1, 50);
        if (ret < 0) return errno;
        if (ret == 0) continue;

        do {

            ret = ioctl(fd, VIDIOC_DQEVENT, &event_arg);
            if (ret) {
                printf("%s: VIDIOC_DQEVENT ret %d\n", __func__, ret);
                break;
            }

            switch (event_arg.id) {

               /* case V4L2_SUB_EXT_VDEC_FRAME:
                    recvFrmInfo(event_arg.u.data);
                    nFrmInfo++;
                    break;*/

                case V4L2_SUB_EXT_VDEC_PICINFO:
                    //recvPicInfo(fd);
                    //nPicInfo++;
                    //if (nPicInfo == 1) {
                        //getStreamInfo(fd);
                        //getDecoderStatus(fd);
                    //}
                    break;


                default:
                    printf("%s: unknown event %d\n", __func__, event_arg.id);
                    break;
            }

        } while (event_arg.pending);

    }

    return 0;
}

#define _DIN_COPYMODE
static int sinkFile_venc(char *filepath)
{
    int fd = -1;
    void *handle = NULL, *buf = NULL;
    int bufsize=0, size,buf_phyadd=0,buf_phyadd_swap=0;
#ifdef _DIN_COPYMODE
    static int i=0;
#endif	
    if (!filepath)
        return -1;

    fd = open(filepath, O_RDONLY);
    if (fd < 0) return -ENOENT;

#ifndef _DIN_COPYMODE
    bufsize = (720*480*3)>>1;
    buf = malloc(bufsize);;
    if (!buf) return -ENOMEM;
#endif
    ringbuf_open(DIN_BS_HDR_ADDR, &handle);

    printf("%s: start\n", __func__);

    while (handle) {

#ifdef _DIN_COPYMODE
		bufsize = frame[i% ALLOCFRAME_NUM].size;
		buf =(void*)frame[i% ALLOCFRAME_NUM].cached;
		buf_phyadd=frame[i% ALLOCFRAME_NUM].phy;
		buf_phyadd_swap=reverseInteger(buf_phyadd);
#endif
        size = read(fd, buf, bufsize);
		//printf("read size:%d,%x,%x\n",size,(unsigned int)buf,buf_phyadd);
        if (size <= 0 || size != bufsize) break;

#ifndef _DIN_COPYMODE
		while (ringbuf_write(handle, buf, size) < 0)
#else
        while (ringbuf_write(handle, (void *)&buf_phyadd_swap, sizeof(unsigned long)) < 0)
#endif
            usleep(300);
        usleep(16000);
        input_frame++;
#ifdef _DIN_COPYMODE
		i++;
#endif
    }
    //ringbuf_close(handle);

#ifndef _DIN_COPYMODE
    free(buf);
#endif
    close(fd);

    printf("%s: end,num=%d\n", __func__,input_frame);

    return 0;
}

static void* thread_sinkFile_venc(void *arg)
{
    int ret;

    ret = sinkFile_venc((char*)arg);

    printf("%s: sinkFile ret %d\n", __func__, ret);

    return NULL;
}

#if 0
static int reqbuf_test(int fd)
{
    int ret;

	struct v4l2_requestbuffers reqbuf;
	printf("%s:\n", __func__);

	memset((void*)&reqbuf,0,sizeof(reqbuf));
	reqbuf.count = 1;
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	reqbuf.memory = V4L2_MEMORY_MMAP;


    
    ret=ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	if (ret) {
		printf("%s: VIDIOC_REDQBUFS ret %d\n", __func__, ret);
	}
	printf("%s: end\n", __func__);

	return 0;
}
#endif
static int init_userp(int fd,unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;
        int i,ret;
        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

		ret = ioctl(fd, VIDIOC_REQBUFS, &req);
		IF_ERR_EXIT(errno);


        buffers = calloc(4, sizeof(*buffers));

        if (!buffers) {
                printf("Out of memory\\n");
				return -1;
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc(buffer_size);

                if (!buffers[n_buffers].start) {
                        printf("Out of memory\\n");
						return -1;
                }
        }


		for (i = 0; i < n_buffers; ++i) {
				struct v4l2_buffer buf;
		
				CLEAR(buf);
				buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory = V4L2_MEMORY_USERPTR;
				buf.index = i;
				buf.m.userptr = (unsigned long)buffers[i].start;
				buf.length = buffers[i].length;
		
		        ret = ioctl(fd, VIDIOC_QBUF, &buf);
		        IF_ERR_EXIT(errno);
	            printf("qbuf: addr %x\n", (int)buffers[i].start);
		}

		return ret;
		
}

#if 0
static int init_cap_bufs(int fd, int picW, int picH, int bufcnt)
{
	int i, j, ret;
	struct v4l2_requestbuffers reqbuf = {0};
	//struct v4l2_exportbuffer expbuf = {0};

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuf.memory = V4L2_MEMORY_USERPTR;//V4L2_MEMORY_MMAP;
	reqbuf.count = bufcnt;
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuf);
	IF_ERR_EXIT(errno);
/*
	for (i=0; i<bufcnt; i++) {
		for (j=0; j<2; j++) {
			expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
			expbuf.index = i;
			expbuf.plane = j;
			ret = ioctl(fd, VIDIOC_EXPBUF, &expbuf);
			IF_ERR_EXIT(errno);
			cap_bufs[i].expbuf_fd[j] = expbuf.fd;
			printf("expbuf: index %d plane %d fd %d\n", expbuf.index, expbuf.plane, expbuf.fd);
		}
	}
*/
/*
	for (i=0; i<bufcnt; i++) {
		cap_bufs[i].buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		cap_bufs[i].buffer.memory = V4L2_MEMORY_MMAP;
		cap_bufs[i].buffer.index = i;
		cap_bufs[i].buffer.length = VIDEO_MAX_PLANES;
		cap_bufs[i].buffer.m.planes = cap_bufs[i].planes;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &cap_bufs[i].buffer);
		IF_ERR_EXIT(errno);
		for (j=0; j<2; j++)
			printf("querybuf: index %d plane %d\n", i, j);
	}
*/
	for (i=0; i<bufcnt; i++) {
		ret = ioctl(fd, VIDIOC_QBUF, &cap_bufs[i].buffer);
		IF_ERR_EXIT(errno);
		printf("qbuf: index %d\n", cap_bufs[i].buffer.index);
	}

	return ret;
}
#endif

static int dqbuf(int fd,char *filepath)
{
    int file = -1;

	struct v4l2_buffer buf={0};
	//struct v4l2_plane planes[0];
    printf("%s:%d\n", __func__,__LINE__);

    if (!filepath)
    {
        printf("dump filepath error\n");
        return -1;
    }
	
    file = open(filepath,O_RDWR|O_CREAT|O_TRUNC);
    if (file < 0)
    {
		printf("%s: open file fail\n",filepath);
		return -ENOENT;
    }



	while (1) {

		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_USERPTR;
		//buf.length = 1;
		//buf.m.planes = planes;

		ioctl(fd, VIDIOC_DQBUF, &buf);
		IF_ERR_EXIT(errno);
		printf("dqbuf: addr %x, size:%d\n", (int)buf.m.userptr, buf.bytesused);

		usleep(10000);
		//process_image((void *)buf.m.userptr, buf.bytesused);
		
		write(file, (void *)buf.m.userptr, buf.bytesused);

		ioctl(fd, VIDIOC_QBUF, &buf);
		IF_ERR_EXIT(errno);
		printf("qbuf: addr %x size:%d\n", (int)buf.m.userptr,buf.bytesused);
	}

    printf("%s:%d\n", __func__,__LINE__);
    close(file);

	return 0;
}

#if 0
static void* thread_dqbuf(void *arg)
{
    int ret;
    printf("%s: dqbuf \n", __func__);

    ret = dqbuf((int) arg);

    printf("%s: dqbuf ret %d\n", __func__, ret);

    return NULL;
}

#endif

#if 0 
static int dumpFile(char *filepath)
{
    int fd = -1;
    void *handle = NULL, *buf = NULL;
    int bufsize;//,size,total_size;
	unsigned int wrpt,rdpt,read_size=0;

    if (!filepath)
    {
        printf("dump filepath error\n");
        return -1;
    }
	
    fd = open(filepath,O_RDWR|O_CREAT|O_TRUNC);
    if (fd < 0)
    {
		printf("%s: open file fail\n",filepath);
		return -ENOENT;
    }

    ringbuf_open(ENC_BS_HDR_ADDR, &handle);

    printf("%s: start\n", __func__);

    bufsize = 0x100000;//reserved 1M for output stream copy
    buf = malloc(bufsize);;
    if (!buf) 
    {
	printf("%s: malloc fail\n", __func__);
	return -ENOMEM;
    }
    while (handle) 
	{
      //while(1)
      {
        ringbuf_getWr(handle,&wrpt);
        ringbuf_getRd(handle,&rdpt);
		read_size=wrpt-rdpt;

		if(read_size)
		{
    	  printf("dump wrpt:%x,rdpt:%x,size:%d\n",wrpt,rdpt,read_size);
    	  ringbuf_read(handle, buf ,read_size) ;
    	  write(fd, buf, read_size);
          timeout_count=0;
	    }
		else
		{
		    timeout_count++;
			usleep(1000);
			if (timeout_count>1000 && wrpt == rdpt)
			{
			printf("dump wrpt:%x,rdpt:%x timeout\n",wrpt,rdpt);
			break;
			}
		}
      }

    }
    //ringbuf_close(handle);
    free(buf);
    close(fd);

    printf("%s: end\n", __func__);

    return 0;
}


static void* thread_dumpFile(void *arg)
{
    int ret;
    printf("%s: dumpFile \n", __func__);

    ret = dumpFile((char*)arg);

    printf("%s: dumpFile ret %d\n", __func__, ret);

    return NULL;
}
#endif


static int initRingBufHdr(void)
{
    int ret;
    //NEW_SEG new_seg;
    //DECODE decode;
    void *handle;
printf("initRingBufHdr\n");
    ret = ringbuf_initHdr(ENC_BS_HDR_ADDR, RINGBUFFER_STREAM, ENC_BS_BUF_ADDR, ENC_BS_BUF_SIZE);
    IF_ERR_EXIT(ret);

    //ret = ringbuf_initHdr(ENC_MSG_HDR_ADDR, RINGBUFFER_MESSAGE, ENC_MSG_BUF_ADDR, ENC_MSG_BUF_SIZE);
    //IF_ERR_EXIT(ret);

    ret = ringbuf_initHdr(DIN_BS_HDR_ADDR, RINGBUFFER_STREAM, DIN_BS_BUF_ADDR, DIN_BS_BUF_SIZE);
    IF_ERR_EXIT(ret);

    ret = ringbuf_initHdr(DIN_IB_HDR_ADDR, RINGBUFFER_COMMAND, DIN_IB_BUF_ADDR, DIN_IB_BUF_SIZE);
    IF_ERR_EXIT(ret);
#if 1
    ret = ringbuf_open(DIN_IB_HDR_ADDR, &handle);
    IF_ERR_EXIT(ret);

    //DIN_COPY_MODE cmd;
    //cmd.header.type = VIDEO_DIN_INBAND_CMD_TYPE_COPY_MODE;
    //cmd.header.size = sizeof(DIN_COPY_MODE);
    //cmd.copy_mode = 1;
    //cmd.step_size = sizeof(unsigned long);
    //ret = ringbuf_write(handle, &cmd, sizeof(DIN_COPY_MODE));
    //IF_ERR_EXIT(ret);
	
    VIDEO_DIVX3_RESOLUTION cmd1;
    cmd1.header.type = VIDEO_DIVX3_INBAND_CMD_TYPE_RESOLUTION;
    cmd1.header.size = sizeof(cmd1);
    cmd1.width  = 720;
    cmd1.height = 480;

    ret = ringbuf_write(handle, &cmd1, sizeof(VIDEO_DIVX3_RESOLUTION));
    IF_ERR_EXIT(ret);

    ringbuf_close(handle);
#endif
    return 0;
}

static int emulateSource(void)
{
    int ret;

    ret = initRingBufHdr();
    IF_ERR_EXIT(ret);

    return 0;
}

static int venc_allocMemory(int fd, int type, int size, int flags, unsigned int *pAddr, unsigned int **cached)
{
	int ret;
	struct v4l2_ext_vdec_mem_alloc args = {0};

	args.type = type;
	args.size = size;
	args.flags = flags;

	ret = s_ext_ctrls_ptr(fd, V4L2_CID_EXT_VDEC_MEM_ALLOC, &args);
	IF_ERR_EXIT(ret);

	*pAddr = args.phyaddr;

	*cached=syswrap_mmap(args.phyaddr, size) ;

	//printf("\n%s: type %d size %d flags %d addr 0x%08x ,0x%08x\n", __func__,
		//type, size, flags, args.phyaddr, (int)*cached);

	return 0;
}


static int test_01(void)
{
    int ret, fd_enc, val;
    pthread_t thread;//,thread_1;
    struct v4l2_format fmt;
    int i;
    int raw_width  = 720 ;
    int raw_height = 480  ;

    if (!opts.input) {
        printf("%s: no input\n", __func__);
        return -1;
    }

    ret = emulateSource();
    IF_ERR_EXIT(ret);

    fd_enc = open(V4L2_EXT_DEV_PATH_VENC, O_RDWR);
    IF_ERR_EXIT(errno);

    //Alloc ALLOCFRAME_NUM frame buffer for yuv420p input raw data
    for(i = 0; i < ALLOCFRAME_NUM ;i++)
    {
        frame[i].size   = (raw_width * raw_height * 1.5);
		ret = venc_allocMemory(fd_enc, V4L2_EXT_VDEC_MEMTYPE_GENERIC,frame[i].size, 0, &(frame[i].phy),&(frame[i].cached));
		printf("frame[%d].cached=%x,phy=%x\n",i,(int)frame[i].cached,frame[i].phy);
		IF_ERR_EXIT(ret);
    }


    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ret = ioctl(fd_enc, VIDIOC_G_FMT, &fmt);
    IF_ERR_EXIT(ret);

    ret = g_ctrl(fd_enc, V4L2_CID_EXT_VDEC_DECODER_VERSION, &val);
    IF_ERR_EXIT(ret);

    printf("decoder version: %d\n", val);

	memset((void*)&fmt,0,sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix_mp.width = 720;
    fmt.fmt.pix_mp.height = 480;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
	fmt.fmt.pix_mp.plane_fmt[0].sizeimage = MAX_STREAM_SIZE;
    ret = ioctl(fd_enc, VIDIOC_S_FMT, &fmt);
    IF_ERR_EXIT(ret);

    ret=init_userp(fd_enc,0x50000);
	//ret = init_cap_bufs(fd_enc, 720, 480, 2);
	//ret=reqbuf_test(fd_enc);
    IF_ERR_EXIT(ret);

	ret = s_ctrl(fd_enc, V4L2_CID_EXT_VDEC_VTP_PORT, lookup(opts.source));
	IF_ERR_EXIT(ret);


    //ret = subscribe(fd_dec, V4L2_EVENT_PRIVATE_EXT_VDEC_EVENT, V4L2_SUB_EXT_VDEC_FRAME);
    //IF_ERR_EXIT(ret);



    ret = streamon(fd_enc);
    IF_ERR_EXIT(ret);
    printf("streamon done\n");

   // ret = pthread_create(&thread_1, NULL, thread_dumpFile, opts.output);
   //IF_ERR_EXIT(ret);

    ret = pthread_create(&thread, NULL, thread_sinkFile_venc, opts.input);
    IF_ERR_EXIT(ret);


    //ret = pthread_create(&thread_1, NULL, thread_dqbuf, (void *)&fd_enc);
    //IF_ERR_EXIT(ret);

	ret = dqbuf(fd_enc,opts.output);
	IF_ERR_EXIT(ret);


    ret = dqevent(fd_enc);
    IF_ERR_EXIT(ret);

    pthread_join(thread, NULL);
    //pthread_join(thread_1, NULL);
    close(fd_enc);
    printf("close fd_enc done\n");

    return 0;
}

static test_entry_t tests[] = {
    {1, test_01, "emulate source"},
};

static void getRange(char *str, int *start, int *end)
{
    char *p;

    *start = strtoll(str, NULL, 0);
    p = strstr(str, "-");
    *end = p? strtoll(p+1, NULL, 0) : *start;

    return;
}

static void runTest(int start, int end)
{
    int ret = 0, id, x, cnt, step;

    cnt = sizeof(tests) / sizeof(test_entry_t);
    step = (start > end)? -1 : 1;
    id = start;

    while (1) {

        for (x=0; x<cnt; x++) {
            if (tests[x].id == id) {
                printf("\n------------------------------------------------\n");
                printf("test_%02d running ...\n", id);
                ret = tests[x].fun();
                printf("test_%02d ret %d\n", id, ret);
                printf("------------------------------------------------\n\n");
                break;
            }
        }

        if (x == cnt)
            printf("no test_%02d\n", id);

        if (id == end) break;
        id += step;
    }

    return;
}

int main(int argc, char *argv[])
{
    char *p;
    int i, start, end, cnt;

    parseArgs(&opts, argc, argv);

    if (opts.list_tests) {
        cnt = sizeof(tests) / sizeof(test_entry_t);
        for (i=0; i<cnt; i++)
            printf("test_%02d: %s\n", tests[i].id, tests[i].desc);
        return 0;
    }

    p = strtok(opts.test, " ");
    while (p) {
        getRange(p, &start, &end);
        runTest(start, end);
        p = strtok(NULL, " ");
    }

    return 0;
}

