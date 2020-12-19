/*
 *  Mars soundcard
 *  Copyright (c) by Jaroslav Kysela <perex@suse.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <linux/mutex.h>
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/syscalls.h> /* needed for the _IOW etc stuff used later */
#include <linux/mpage.h>
#include <linux/dcache.h>
#include <linux/pageremap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/dma-mapping.h>
#include <sound/asound.h>
#include "AudioInbandAPI.h"
#include "rpc_common.h"
#include <sound/rtk_snd.h>
#include <asm/cacheflush.h>
#include <linux/pageremap.h>
#include <linux/string.h>
#include <linux/delay.h>
#if defined(CONFIG_REALTEK_RPC)
#include <mach/RPCDriver.h>
#endif
#include <linux/platform_device.h>

#if defined(CONFIG_RTK_KDRV_RPC)
#include <rtk_kdriver/RPCDriver.h>
#endif
#include "rtk_kcontrol.h"
#define ALSA_ERROR pr_err


MODULE_AUTHOR("EJ Hsu <ejhsu@realtek.com.tw>");
MODULE_DESCRIPTION("Mars soundcard");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("{{ALSA,Mars soundcard}}");

int snd_open_count;		/* playback open count */
int snd_open_ai_count;	/* capture open count */
int have_global_ai;		/* check whether FW use global AI or not */
int get_cap;
int alsa_agent;
int ao_cap_agent = -1;
int ao_cap_pin = -1;

/* #define USE_DECODER */
#define MAX_PCM_DEVICES 8
#define MAX_PCM_SUBSTREAMS 1
#define MAX_MIDI_DEVICES 2
/* for capture feature */
#define MAX_AI_DEVICES 2
#define ENDIAN_CHANGE(x)	\
                ((((x)&0xff000000)>>24)|\
				(((x)&0x00ff0000)>>8)|\
				(((x)&0x0000ff00)<<8)|\
				(((x)&0x000000ff)<<24))
#define ENDIAN_CHANGE_16BITS(x)	 ((x & 0xff00)>>8 |(x & 0x00ff)<<8)

/* defaults */
#define MIN_BUFFER_SIZE (0x8000)
#define MAX_BUFFER_SIZE (256*1024)
/* period size in bytes */
#define MIN_PERIOD_SIZE (1680)
#define MAX_PERIOD_SIZE (0x40000)

#define CONSTRAINT_MIN (35000)
#define CONSTRAINT_MAX (1000000)

#define USE_FORMATS SNDRV_PCM_FMTBIT_S16_LE

#ifdef CONFIG_CUSTOMER_TV006
#define USE_RATE (SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000)
#define USE_RATE_MIN 44100
#else
#define USE_RATE (SNDRV_PCM_RATE_16000|SNDRV_PCM_RATE_32000| \
	SNDRV_PCM_RATE_44100|SNDRV_PCM_RATE_48000)
#define USE_RATE_MIN 16000
#endif

#define USE_RATE_MAX 48000

#ifndef USE_CHANNELS_MIN
#define USE_CHANNELS_MIN 1
#endif
#ifndef USE_CHANNELS_MAX
  #ifndef CONFIG_WISA
  #define USE_CHANNELS_MAX 2
  #else
  #define USE_CHANNELS_MAX 8
  #endif
#endif
#ifndef USE_PERIODS_MIN
#define USE_PERIODS_MIN 4
#endif
#ifndef USE_PERIODS_MAX
#define USE_PERIODS_MAX 1024
#endif
#ifndef add_playback_constraints
#define add_playback_constraints(x) 0
#endif
#ifndef add_capture_constraints
#define add_capture_constraints(x) 0
#endif

#ifdef CONFIG_CUSTOMER_TV006
#define USE_FIXED_AO_PINID
#endif

#ifdef CONFIG_ARM64
#define ALIGN4 (0xFFFFFFFFFFFFFFFCLL)
#else
#define ALIGN4 (0xFFFFFFFC)
#endif
/* #define ALSA_DEBUG_LOG */

#ifdef CONFIG_CUSTOMER_TV001
#ifndef RPC_ACPU_NOT_READY
#define RPC_ACPU_NOT_READY -3
#endif
#endif

#define WISA_RESAMPLER_MAX  (48187)
#define WISA_RESAMPLER_MIN  (47812)

/* EJ: device configuration */
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;	/* Index 0-MAX */
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;	/* ID for this card */
#ifndef CONFIG_CUSTOMER_TV006
static int enable[SNDRV_CARDS] = {1, 1, [2 ... (SNDRV_CARDS - 1)] = 0};
#else
static int enable[SNDRV_CARDS] = {1, [1 ... (SNDRV_CARDS - 1)] = 0};
#endif
static int pcm_devs[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = SNDRV_CARDS};
static int pcm_substreams[SNDRV_CARDS] = {[0 ... (SNDRV_CARDS - 1)] = MAX_PCM_SUBSTREAMS};
static int min_period_size = MIN_PERIOD_SIZE;

#ifdef USE_FIXED_AO_PINID
static int used_ao_pin[MAX_PCM_DEVICES];
static int flush_error[MAX_PCM_DEVICES];
static int pause_error[MAX_PCM_DEVICES];
static int close_error[MAX_PCM_DEVICES];
static int release_error[MAX_PCM_DEVICES];
#endif

static int es_codec[2];
static int es_fs[2];
static int es_nch[2];
static int es_bps[2];

//For capture devices
static enum AUDIO_CONFIG_DEVICE g_capture_dev;
static HAL_AUDIO_VOLUME_T g_capture_dev_vol[4];//0:default, 1:BT, 2:WISA, 3:SE_BT
static BOOLEAN g_capture_dev_mute[4];//0:default, 1:BT, 2:WISA, 3:SE_BT
static int g_capture_wisa_resampler;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for mars soundcard.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for mars soundcard.");
module_param_array(enable, int, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable this mars soundcard.");
module_param_array(pcm_devs, int, NULL, 0444);
MODULE_PARM_DESC(pcm_devs, "PCM devices # (0-7) for mars driver.");
module_param_array(pcm_substreams, int, NULL, 0444);
MODULE_PARM_DESC(pcm_substreams, "PCM substreams # (1-16) for mars driver.");
/* module_param_array(midi_devs, int, NULL, 0444); */
/* MODULE_PARM_DESC(midi_devs, "MIDI devices # (0-2) for mars driver."); */
module_param(min_period_size, int, 0444);

/*struct snd_card_mars {*/
	/*struct snd_card *card;*/
	/*spinlock_t mixer_lock;*/
	/*struct work_struct work_volume;*/
	/*struct mutex rpc_lock;*/
	/*int ao_pin_id[MIXER_ADDR_MAX];*/
	/*int ao_flash_volume[MIXER_ADDR_MAX];*/
	/*int ao_flash_change[MIXER_ADDR_MAX];*/
/*};*/

enum AUDIO_PATH {
	AUDIO_PATH_NONE = 0,
	AUDIO_PATH_DECODER_AO,
	AUDIO_PATH_AO,
	AUDIO_PATH_AO_BYPASS
};

enum RTK_SND_FLUSH_STATE {
	RTK_SND_FLUSH_STATE_NONE = 0,
	RTK_SND_FLUSH_STATE_WAIT,
	RTK_SND_FLUSH_STATE_FINISH
};

enum SND_CARD0_DEVICE {
    PLAY_MIXER0 = 0,
    PLAY_MIXER1,
    PLAY_MIXER2,
    PLAY_MIXER3,
    PLAY_MIXER4,
    PLAY_MIXER5,
    PLAY_MIXER6,
    PLAY_MIXER7,
    PLAY_ES0,
    PLAY_ES1,
    CAPTURE_PCM,
    CAPTURE_ES,
    CAPTURE_MIC,
    DEVICE_MAX,
};

#define PLAY_ES_DEVICE(dev) (dev == PLAY_ES0 || dev == PLAY_ES1)

const char *SND_DEVICE_NAME[] = {
  [PLAY_MIXER0] = "MARS PCM",
  [PLAY_MIXER1] = "MARS PCM",
  [PLAY_MIXER2] = "MARS PCM",
  [PLAY_MIXER3] = "MARS PCM",
  [PLAY_MIXER4] = "MARS PCM",
  [PLAY_MIXER5] = "MARS PCM",
  [PLAY_MIXER6] = "MARS PCM",
  [PLAY_MIXER7] = "MARS PCM",
  [PLAY_ES0   ] = "MARS ES",
  [PLAY_ES1   ] = "MARS ES",
  [CAPTURE_PCM] = "MARS PCM",
  [CAPTURE_ES ] = "MARS ES",
  [CAPTURE_MIC] = "MARS MIC",
};

struct rtk_snd_pcm {
	struct snd_card_mars *card;
	spinlock_t lock;
	spinlock_t *pcm_lock;
	struct timer_list *timer;
	struct tasklet_struct *elapsed_tasklet;
	int elapsed_tasklet_finish;
	int elapsed_underflow_debouce;
	struct tasklet_struct trigger_tasklet;
	struct work_struct work_resume;
	struct work_struct work_pause;
	struct work_struct work_flush;
	uint32_t empty_timeout;
	int running;

	enum RTK_SND_FLUSH_STATE flush_state;
	unsigned int pcm_buffer_head;
	unsigned int pcm_size;		/* buffer sizze */
	unsigned int pcm_count;		/* period length */
	unsigned int pcm_bps;		/* bytes per second */
	unsigned int pcm_jiffie;	/* bytes per one jiffie */
	int pcm_irq_pos;			/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	unsigned int remain_sample;

	snd_pcm_access_t access;	/* access mode */
	snd_pcm_format_t format;	/* SNDRV_PCM_FORMAT_* */
	unsigned int rate;		/* rate in Hz */
	unsigned int channels;		/* channels */
	snd_pcm_uframes_t period_size;	/* period size */
	unsigned int periods;		/* periods */
	snd_pcm_uframes_t buffer_size;	/* buffer size */
	unsigned int sample_bits;

	snd_pcm_uframes_t buffer_byte_size;	/* buffer size */

	unsigned int sample_jiffies;
	unsigned int period_jiffies;
	unsigned int wp;
	unsigned int rp_real;
	unsigned int out_sample_width;
	int output_frame_bytes;
	int input_frame_bytes;
	int output_sample_bytes;
	int input_sample_bytes;

	struct RBUF_HEADER_ARM ring_bak[8];
	/* ALSA buffer control */
	snd_pcm_uframes_t appl_ptr;
	/* buffer control */
	int freerun;
	unsigned int ring_init;
	unsigned int extend_to_32be_ratio;
	/* realtek hw control */
	int ao_agent;
	int ao_pin_id;
	int dec_agent;
	int dec_pin_id;
	int agent_id;
	int pin_id;
	int volume;
	int volume_change;
	enum AUDIO_PATH audiopath;
	struct page *page;
	RINGBUFFER_HEADER *ring; /* realtek hw ring buffer control */
	RINGBUFFER_HEADER *inband; /* realtek hw ring buffer control */

	RINGBUFFER_HEADER hw_ring[8];
	RINGBUFFER_HEADER hw_inband_ring[8];
	unsigned int hw_inband_data[256];

	RINGBUFFER_HEADER *ao_in_ring;
	RINGBUFFER_HEADER ao_in_ring_instance[8];

	unsigned int *ao_in_ring_p[8];
	struct snd_pcm_substream *substream;

	dma_addr_t phy_addr; /* for UNCAC_BASE */

	long total_data_wb;	/* total data write in byte */
	long pre_time_ms; /* last time that updata wp in millisecond */
	long current_time_ms; /* current time in millisecond */
	unsigned long pre_wr_ptr;

	long pre_no_datatime_ms; /* last time that no data time in millisecond */
	long current_no_datatime_time_ms; /* current no data time in millisecond */

	long max_level; /* max data in ring buffer */
	long min_level; /* min data in ring buffer */

	int hwptr_error_times; /* error handle for ptr */
};

/* pcm capture stucture */
struct rtk_snd_cap_pcm {
	struct snd_card_mars *card;
	spinlock_t *pcm_lock;
	struct timer_list *timer;
	struct tasklet_struct *elapsed_tasklet;
	int elapsed_tasklet_finish;
	int elapsed_underflow_debouce;
	struct tasklet_struct trigger_tasklet;
	struct work_struct work_resume;
	struct work_struct work_pause;
	struct work_struct work_flush;
	uint32_t empty_timeout;
	int running;

	enum RTK_SND_FLUSH_STATE flush_state;
	unsigned int pcm_buffer_head;
	unsigned int pcm_size;		/* buffer sizze */
	unsigned int pcm_count;		/* period length */
	unsigned int pcm_bps;		/* bytes per second */
	unsigned int pcm_jiffie;	/* bytes per one jiffie */
	int pcm_irq_pos;			/* IRQ position */
	unsigned int pcm_buf_pos;	/* position in buffer */
	unsigned int remain_sample;

	snd_pcm_access_t access;	/* access mode */
	snd_pcm_format_t format;	/* SNDRV_PCM_FORMAT_* */
	unsigned int rate;		/* rate in Hz */
	unsigned int channels;		/* channels */
	snd_pcm_uframes_t period_size;	/* period size */
	unsigned int periods;		/* periods */
	snd_pcm_uframes_t buffer_size;	/* buffer size */
	unsigned int sample_bits;

	snd_pcm_uframes_t buffer_byte_size;	/* buffer size */

	unsigned int sample_jiffies;
	unsigned int period_jiffies;
	unsigned int out_sample_width;
	int output_frame_bytes;
	int input_frame_bytes;
	int output_sample_bytes;
	int input_sample_bytes;
	int interleave;

	struct RBUF_HEADER_ARM ring_bak[8];
	/* ALSA buffer control */
	snd_pcm_uframes_t appl_ptr;

	/* for AIO_CONFIG */
	enum ENUM_AUDIO_IPT_SRC path_src;
	enum ENUM_AUDIO_BBADC_SRC bbadc_mux_in;
	enum ENUM_AUDIO_I2SI_SRC i2si_mux_in;
	enum ENUM_AUDIO_SPDIFI_SRC spdifi_mux_in;

	unsigned int ring_init;
	unsigned int extend_to_32be_ratio;
	/* realtek hw control */
	int ao_cap_agent;
	int ao_cap_pin;
	int ai_cap_agent;
	int ai_cap_pin;
	int volume;
	int volume_change;

	enum AUDIO_FORMAT_OF_CAPTURED_SEND_TO_ALSA cap_format;
	struct page *page;
	RINGBUFFER_HEADER *ring; /* realtek hw ring buffer control */
	RINGBUFFER_HEADER *inband; /* realtek hw ring buffer control */


	RINGBUFFER_HEADER hw_ring[8];
	RINGBUFFER_HEADER hw_inband_ring[8];
	unsigned int hw_inband_data[256];

	RINGBUFFER_HEADER *hw_ai_ring;
	RINGBUFFER_HEADER hw_ai_ring_instance[8];

	unsigned int *hw_ai_ring_data[8];
	struct snd_pcm_substream *substream;

	void *ring_p;	/* for capture address */
	dma_addr_t ring_phy_addr;	/* for capture address */

	dma_addr_t phy_addr; /* add for UNCAC_BASE */

	/* last time that updata wp in millisecond */
	long last_time_ms;
	/* current time in millisecond */
	long current_time_ms;

};

static char *snd_access_mode[SNDRV_PCM_ACCESS_LAST+1] = {
	"SND_PCM_ACCESS_MMAP_INTERLEAVED",
	/** mmap access with simple non interleaved channels */
	"SND_PCM_ACCESS_MMAP_NONINTERLEAVED",
	/** mmap access with complex placement */
	"SND_PCM_ACCESS_MMAP_COMPLEX",
	/** snd_pcm_readi/snd_pcm_writei access */
	"SND_PCM_ACCESS_RW_INTERLEAVED",
	/** snd_pcm_readn/snd_pcm_writen access */
	"SND_PCM_ACCESS_RW_NONINTERLEAVED",
};

static ALSA_DELAY_CMD_INFO dly_cmd_info;
static ALSA_GAIN_CMD_INFO gain_cmd_info;
static ALSA_SNDOUT_CMD_INFO sndout_cmd_info={0};
static ALSA_ADEC_CMD_INFO adec_cmd_info;
static ALSA_AENC_CMD_INFO aenc_cmd_info;
static ALSA_ADC_CMD_INFO  adc_cmd_info;
static BOOLEAN b_gain_get_init  = FALSE;
static BOOLEAN b_delay_get_init = FALSE;

static struct platform_device *devices[SNDRV_CARDS];

static int rtk_snd_flush(struct snd_pcm_substream *substream);
static int rtk_snd_pause(struct snd_pcm_substream *substream);
static int rtk_snd_close(struct snd_pcm_substream *substream);
static int rtk_snd_playback_close(struct snd_pcm_substream *substream);
static int rtk_snd_create_decoder_agent(struct snd_pcm_substream *substream);
static int rtk_snd_init_connect_decoder_ao(struct snd_pcm_substream *substream);
static void rtk_snd_init_decoder_ring(struct snd_pcm_substream *substream);
static int rtk_snd_init_decoder_info(struct snd_pcm_substream *substream);
static int rtk_snd_set_ao_flashpin_volume(struct snd_pcm_substream *substream);
static void rtk_snd_resume(struct snd_pcm_substream *substream);
static int rtk_snd_init_ao_ring(struct snd_pcm_substream *substream);
static int rtk_snd_ao_info(struct snd_pcm_substream *substream);
static int rtk_snd_init(struct snd_card *card);
static int rtk_snd_open(struct snd_pcm_substream *substream);
static void rtk_snd_resume_work(struct work_struct *work);
static void rtk_snd_pause_work(struct work_struct *work);
static void rtk_snd_eos_work(struct work_struct *work);
static void rtk_snd_playback_volume_work(struct work_struct *work);
static void rtk_snd_pcm_freerun_timer_function(unsigned long data);
static void rtk_snd_pcm_capture_timer_function(unsigned long data);
static void rtk_snd_pcm_capture_es_timer_function(unsigned long data);
static int rtk_snd_check_audio_fw_capability(struct snd_card *card);
static void rtk_snd_fmt_convert_to_S16LE(struct snd_pcm_substream *substream,
	snd_pcm_uframes_t wp_next, snd_pcm_uframes_t wp, unsigned int adv_min);

extern long rtkaudio_send_audio_config(AUDIO_CONFIG_COMMAND_RTKAUDIO * cmd);
extern SINT32 Volume_to_DSPGain(HAL_AUDIO_VOLUME_T volume);

/* not used now
void getpts(void)
{
	int64_t ret;
	unsigned int ptrlo, ptrhi;

	ptrlo = (unsigned int) rtd_inl(0xB801B690);
	ptrhi = (unsigned int) rtd_inl(0xB801B694);

	ret = ptrlo;
	ret = ret|(((int64_t)ptrhi) << 32);

	ALSA_DEBUG_INFO("ALSA PTS: %08x\n", ret);
}
*/

#ifndef CONFIG_CUSTOMER_TV006
static void update_hw_delay(struct snd_pcm_substream *substream)
{
	unsigned long delay_90k;
	struct snd_pcm_runtime *runtime = substream->runtime;

	delay_90k = (unsigned long) rtd_inl(0xB800607C);

	runtime->delay = ((delay_90k / 90) * runtime->rate) / 1000;

	/*
	if (runtime->rate != 0) {
		pr_info("ALSA hw delay: %08x(%d samples, %d ms)\n", delay_90k, runtime->delay, runtime->delay*1000/runtime->rate);
	}
	*/
}
#endif

static int audio_send_rpc_command(int opt,
	unsigned long command, unsigned long param1,
	unsigned long param2, unsigned long param2_LE,
	unsigned long *retvalue)
{
	int ret, count;
	RPCRES_LONG *audio_ret;
	ret = 0;
	count = 0;

#ifdef CONFIG_CUSTOMER_TV001
	do {
		ret = send_rpc_command(opt, command, param1, param2, retvalue);

		if (ret == RPC_FAIL) {
			ALSA_ERROR("[ALSA] RPC to ACPU fail!!\n");
			return -1;
		}

		if (ret == RPC_OK)
			break;

		// RPC_ACPU_NOT_READY
		msleep(100);
		count++;
	} while (count <= 100);

	if (ret == RPC_ACPU_NOT_READY) {
		ALSA_ERROR("[ALSA] wait ACPU ready timeout!!!\n");
		return -1;
	}
#else
	if (send_rpc_command(opt, command, param1, param2, retvalue))
		ret = -1;
#endif

	audio_ret = (RPCRES_LONG *)param2_LE;

	if (command == ENUM_KERNEL_RPC_CHECK_READY ||
	command == ENUM_KERNEL_RPC_PRIVATEINFO ||
	command == ENUM_KERNEL_RPC_GET_MUTE_N_VOLUME) {
		if (*retvalue != S_OK) {
			ALSA_ERROR("[ALSA] RPC S_OK fail\n");
			ALSA_ERROR("[ALSA] retvalue %lx\n", *retvalue);
			ret = -1;
		}
	} else {
		if (*retvalue != S_OK || ntohl(audio_ret->result) != S_OK) {
			ALSA_ERROR("[ALSA] RPC S_OK fail\n");
			ALSA_ERROR("[ALSA] retvalue %lx, result %x, command %lx\n",
				*retvalue, ntohl(audio_ret->result), command);
			ret = -1;
		}
	}

	return ret;
}

#if 0
static int snd_card_std_copy(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos,
	void __user *buf, snd_pcm_uframes_t frames)
{
	int channels;
	char *hwbuf;
	dma_addr_t phy_addr;
	size_t dma_csize;
	struct rtk_snd_pcm *dpcm;
	struct snd_pcm_runtime *rt = substream->runtime;
	ALSA_DEBUG_INFO("[ALSA] [%s, %d]\n", __func__, __LINE__);

	channels = rt->channels;
	dma_csize = rt->dma_bytes / channels;
	dpcm = rt->private_data;

	/* default transfer behaviour */
	if (channel == -1) {
		hwbuf = rt->dma_area + frames_to_bytes(rt, pos);
		if (copy_from_user(hwbuf, buf, frames_to_bytes(rt, frames)))
			return -EFAULT;
		phy_addr = dma_map_single(dpcm->card->card->dev, hwbuf,
				frames_to_bytes(rt, frames), DMA_TO_DEVICE);
		dma_unmap_single(dpcm->card->card->dev, phy_addr,
			frames_to_bytes(rt, frames), DMA_TO_DEVICE);
	} else {
		hwbuf = rt->dma_area +
			(channel * dma_csize) +
			samples_to_bytes(rt, pos);
		if (copy_from_user(hwbuf, buf, samples_to_bytes(rt, frames)))
			return -EFAULT;
	}

	return 0;
}

static int rtk_snd_silence(struct snd_pcm_substream *substream,
	int channel, snd_pcm_uframes_t pos, snd_pcm_uframes_t frames)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	int	*i32outbuf[8] = {NULL};
	int i, frame_bytes, channel_count;

	if (channel == -1) {
		frame_bytes = dpcm->input_frame_bytes;
		channel_count = runtime->channels;
		for (i = 0; i < channel_count; i++) {
			i32outbuf[i] =
				(int *)(dpcm->ring_bak[i].beginAddr +
				dpcm->output_sample_bytes *
				pos);
		}
	} else {
		frame_bytes = dpcm->input_sample_bytes;
		i32outbuf[0] =
			(int *)(dpcm->ring_bak[channel].beginAddr +
			dpcm->output_sample_bytes * pos);
		channel_count = 1;
	}

	for (i = 0; i < channel_count; i++)
		memset(i32outbuf[i], 0, dpcm->output_sample_bytes * frames);

	return frames;
}
#endif

#if 1
static int rtk_snd_create_decoder_agent(struct snd_pcm_substream *substream)
{
	struct AUDIO_RPC_INSTANCE *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	int result;
	dma_addr_t dat;
	void *p;

	struct snd_pcm_runtime *runtime;
	struct rtk_snd_pcm *dpcm;

	if (substream == NULL)
		return -1;

	runtime = substream->runtime;
	dpcm = runtime->private_data;

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_INSTANCE *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_INSTANCE) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_INSTANCE) + 8) &
			ALIGN4));

	/* create decoder */
	info->type = htonl(AUDIO_DECODER);
	info->instanceID = htonl(-1);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dpcm->dec_agent = dpcm->agent_id = ntohl(res->data);
	dpcm->dec_pin_id = dpcm->pin_id = BASE_BS_IN;
	result = dpcm->agent_id;
	ALSA_DEBUG_INFO("[ALSA] Create decode instance %d\n", dpcm->agent_id);

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return result;
}

static int RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(struct snd_card *card,
	struct RPC_RBUF_HEADER *header)
{
	struct RPC_RBUF_HEADER *hd, *hd_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	int i;
	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] [%d] alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	hd = p;
	hd_audio = (void *)(struct RPC_RBUF_HEADER *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct RPC_RBUF_HEADER) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct RPC_RBUF_HEADER) + 8) &
			ALIGN4));

	hd->instanceID = htonl(header->instanceID);
	hd->pin_id = htonl(header->pin_id);
	hd->rd_idx = htonl(header->rd_idx);
	hd->listsize = htonl(header->listsize);

	for (i = 0; i < 8; i++) {
		hd->rbuf_list[i] =
			htonl(0xa0000000|(((ulong) header->rbuf_list[i])&0x1FFFFFFF));
	}

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_INIT_RINGBUF,
		(unsigned long) hd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int RPC_TOAGENT_CONNECT_SVC(struct snd_card *card,
	struct AUDIO_RPC_CONNECTION *pconnection)
{
	struct AUDIO_RPC_CONNECTION *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] [%d]alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_CONNECTION *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_CONNECTION) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_CONNECTION) + 8) &
			ALIGN4));

	info->srcInstanceID = htonl(pconnection->srcInstanceID);
	info->srcPinID = htonl(pconnection->srcPinID);
	info->desInstanceID = htonl(pconnection->desInstanceID);
	info->desPinID = htonl(pconnection->desPinID);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CONNECT,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		dma_free_coherent(card->dev, 4096, p, dat);
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int RPC_TOAGENT_PAUSE_SVC(struct snd_card *card, int *inst_id)
{
	int *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(int *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(int) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(int) + 8) &
			ALIGN4));

	*info = htonl(*inst_id);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PAUSE,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int RPC_TOAGENT_RUN_SVC(struct snd_card *card, int *inst_id)
{
	int *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(int *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(int) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(int) + 8) &
			ALIGN4));

	*info = htonl(*inst_id);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_RUN,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int RPC_TOAGENT_FLUSH_SVC(struct snd_card *card,
	struct AUDIO_RPC_SENDIO *sendio)
{
	struct AUDIO_RPC_SENDIO *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_SENDIO *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_SENDIO) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_SENDIO) + 8) &
			ALIGN4));

	ALSA_DEBUG_INFO("[ALSA] %s %ld %d\n", __func__, sendio->instanceID, sendio->pinID);

	info->instanceID = htonl(sendio->instanceID);
	info->pinID = htonl(sendio->pinID);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_FLUSH,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

int RPC_TOAGENT_STOP_SVC(struct snd_card *card, int *instanceID)
{
	int *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(int *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(int) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(int) + 8) &
			ALIGN4));

	*info = htonl(*instanceID);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_STOP,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

int RPC_TOAGENT_DESTROY_SVC(struct snd_card *card, int *instanceID)
{
	int *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] [%s %d]alloc memory fail\n", __func__, __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(int *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(int) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(int) + 8) &
			ALIGN4));

	*info = htonl(*instanceID);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_DESTROY,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

/* not used now
static int hw_ring_avail(RINGBUFFER_HEADER *ring)
{
	RINGBUFFER_HEADER tmp;
	int avail;

	tmp.beginAddr = ntohl(ring->beginAddr) & 0x1FFFFFFF;
	tmp.writePtr = ntohl(ring->writePtr) & 0x1FFFFFFF;
	tmp.readPtr[0] = ntohl(ring->readPtr[0]) & 0x1FFFFFFF;
	tmp.size = ntohl(ring->size);

	if (tmp.readPtr[0] <= tmp.writePtr)
		avail = tmp.writePtr - tmp.readPtr[0];
	else
		avail = tmp.writePtr + tmp.size - tmp.readPtr[0];

	avail = tmp.size - avail;

	return avail;
}
*/

static int hw_ring_write(RINGBUFFER_HEADER *ring, void *data, int len)
{
	RINGBUFFER_HEADER tmp;
	unsigned int temp;
	unsigned char *ptr = data;

	ALSA_DEBUG_INFO("[ALSA] %s Enter\n", __func__);

	tmp.beginAddr = ntohl(ring->beginAddr) & 0x1FFFFFFF;
	tmp.writePtr = ntohl(ring->writePtr) & 0x1FFFFFFF;
	tmp.readPtr[0] = ntohl(ring->readPtr[0]) & 0x1FFFFFFF;
	tmp.size = ntohl(ring->size);

	if (tmp.beginAddr + tmp.size <= tmp.writePtr + len) {
		memcpy((unsigned char *)
			(phys_to_virt(ntohl(ring->writePtr)&0x1FFFFFFF)),
			ptr, (tmp.beginAddr + tmp.size - tmp.writePtr));
		ptr += (tmp.beginAddr + tmp.size - tmp.writePtr);
		len -= (tmp.beginAddr + tmp.size - tmp.writePtr);
		if (len != 0) {
			memcpy((unsigned char *)
				(phys_to_virt(ntohl(ring->beginAddr)&0x1FFFFFFF)), ptr, len);
			tmp.writePtr = tmp.beginAddr + len;
		} else {
			tmp.writePtr = tmp.beginAddr;
		}
	} else {
		memcpy((unsigned char *)
			(phys_to_virt(ntohl(ring->writePtr)&0x1FFFFFFF)),
			ptr, len);
		ALSA_DEBUG_INFO("[ALSA] ntohl(ring->writePtr) = %lx\n",
			(unsigned long)(phys_to_virt(ntohl(ring->writePtr)&0x1FFFFFFF)));
		tmp.writePtr = tmp.writePtr + len;
	}

	temp = 0xa0000000|tmp.writePtr;
	ring->writePtr = htonl(temp);
	ALSA_DEBUG_INFO("[ALSA] %s Exit\n", __func__);
	return len;
}

int RPC_TOAGENT_INBAND_EOS_SVC(struct snd_pcm_substream *substream)
{
	AUDIO_DEC_EOS cmd;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	cmd.header.type = htonl(AUDIO_DEC_INBAND_CMD_TYPE_EOS);
	cmd.header.size = htonl(sizeof(AUDIO_DEC_EOS));
	cmd.EOSID = 0;
	cmd.wPtr = htonl((ulong) dpcm->ring_bak[0].writePtr);

	hw_ring_write(dpcm->inband, &cmd, sizeof(AUDIO_DEC_EOS));
	return 0;
}

int RPC_TOAGENT_EOS_SVC(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	struct AUDIO_RPC_SENDPIN_LONG *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_SENDPIN_LONG *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_SENDPIN_LONG) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_SENDPIN_LONG) + 8) &
			ALIGN4));

	info->instanceID = htonl(dpcm->ao_agent);
	info->pinID = htonl(dpcm->ao_pin_id);
	info->data = htonl((ulong) dpcm->ring_bak[0].writePtr);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_EOS,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return 0;
}

static int dev2adec_index(int device)
{
	int adec_id;
	switch(device)
	{
	case PLAY_ES1:
		adec_id = HAL_AUDIO_ADEC1;
		break;
	case PLAY_ES0:
	default:
		adec_id = HAL_AUDIO_ADEC0;
		break;
	}
	return adec_id;
}

static HAL_AUDIO_SRC_TYPE_T direct2hal_codec(direct_src_codec_ext_type_t es_codec)
{
	int adec_codec;

	switch(es_codec)
	{
	case DIRECT_CODEC_PCM:
		adec_codec = HAL_AUDIO_SRC_TYPE_PCM; break;
	case DIRECT_CODEC_AC3:
		adec_codec = HAL_AUDIO_SRC_TYPE_AC3; break;
	case DIRECT_CODEC_AAC:
		adec_codec = HAL_AUDIO_SRC_TYPE_AAC; break;
	case DIRECT_CODEC_DRA:
		adec_codec = HAL_AUDIO_SRC_TYPE_DRA; break;
	case DIRECT_CODEC_MP3:
		adec_codec = HAL_AUDIO_SRC_TYPE_MPEG; break;
	case DIRECT_CODEC_DTS:
		adec_codec = HAL_AUDIO_SRC_TYPE_DTS; break;
	case DIRECT_CODEC_WMA_PRO:
		adec_codec = HAL_AUDIO_SRC_TYPE_WMA_PRO; break;
	case DIRECT_CODEC_VORBIS:
        /* Need to indicate BS embed info or not */
		/*adec_codec = HAL_AUDIO_SRC_TYPE_VORBIS; break;*/
        adec_codec = HAL_AUDIO_SRC_TYPE_UNKNOWN; break;
	case DIRECT_CODEC_AMR_WB:
        /* Need to indicate 3gp or pure AMR */
		/*adec_codec = HAL_AUDIO_SRC_TYPE_AMR_WB; break;*/
        adec_codec = HAL_AUDIO_SRC_TYPE_UNKNOWN; break;
	case DIRECT_CODEC_AMR_NB:
        /* Need to indicate 3gp or pure AMR */
		/*adec_codec = HAL_AUDIO_SRC_TYPE_AMR_NB; break;*/
        adec_codec = HAL_AUDIO_SRC_TYPE_UNKNOWN; break;
	case DIRECT_CODEC_ADPCM:
        /* Need wFormatTag, block align, dynamic range or bitpersample  */
		/*adec_codec = HAL_AUDIO_SRC_TYPE_ADPCM; break;*/
        adec_codec = HAL_AUDIO_SRC_TYPE_UNKNOWN; break;
	case DIRECT_CODEC_RA8:
		adec_codec = HAL_AUDIO_SRC_TYPE_RA8; break;
	case DIRECT_CODEC_FLAC:
        /* Need to indicate FLAC to skip 1st sync word */
		adec_codec = HAL_AUDIO_SRC_TYPE_FLAC; break;
	case DIRECT_CODEC_UNKNOWN:
	default:
		adec_codec = ADEC_SRC_CODEC_UNKNOWN; break;
		break;
	}
	return adec_codec;
}

static int rtk_snd_es_init_dec_ring(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct rtk_snd_pcm *dpcm_audio = (struct rtk_snd_pcm *) (dpcm->phy_addr|0xa0000000);
	int i, ret = 0;
	unsigned long count, phy_addr_ring;
	int adec_id = dev2adec_index(substream->pcm->device);

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	dpcm->pcm_buf_pos = 0;
	dpcm->ring = (RINGBUFFER_HEADER *)((unsigned long) dpcm->hw_ring);
	dpcm->ao_in_ring = dpcm->ring;
	dpcm->buffer_byte_size = runtime->buffer_size * frames_to_bytes(runtime, 1);

	ALSA_DEBUG_INFO("[ALSA] buffer_byte_size %ld\n", dpcm->buffer_byte_size);

	for (i = 0; i < 1; i++) {
		dpcm->ring[i].beginAddr = htonl(0xa0000000|(((unsigned long) runtime->dma_addr) & 0x1FFFFFFF));
		dpcm->ring_bak[i].beginAddr = (unsigned long) runtime->dma_addr;

		dpcm->ring[i].bufferID = htonl(RINGBUFFER_STREAM);
		dpcm->ring[i].size = htonl(dpcm->buffer_byte_size);
		dpcm->ring[i].writePtr = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[0] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[1] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[2] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[3] = dpcm->ring[i].beginAddr;

		dpcm->ring_bak[i].size = dpcm->buffer_byte_size;
		dpcm->ring_bak[i].writePtr = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[1] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[2] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[3] = dpcm->ring_bak[i].beginAddr;
	}

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	count = (unsigned long)&dpcm->ring[0] - (unsigned long)dpcm;
	phy_addr_ring = (unsigned long)dpcm_audio + count;
	if(HAL_AUDIO_SetEsPushMode(adec_id, HAL_AUDIO_ES_PLAY_ALSA, phy_addr_ring) == NOT_OK)
		ret = -1;

	ALSA_DEBUG_INFO("[ALSA] RING INITIALIZED\n");
	ALSA_DEBUG_INFO("[ALSA] channel = %d\n", runtime->channels);
	ALSA_DEBUG_INFO("[ALSA] frames_to_bytes = %zd\n", frames_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] samples_to_bytes = %zd\n", samples_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] buffer size = %d\n", (int) runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] ACCESS MODE=%s", snd_access_mode[runtime->access]);
	for (i = 0; i < runtime->channels; i++) {
		ALSA_DEBUG_INFO("[ALSA] buf_header[%d].magic = %x\n",
			i, ntohl(dpcm->ring[i].magic));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].beginAddr = %x\n",
			i, ntohl(dpcm->ring[i].beginAddr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].size = %x\n",
			i, ntohl(dpcm->ring[i].size));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[0]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[1] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[1]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %x\n",
			i, ntohl(dpcm->ring[i].writePtr));
	}

	return ret;
}

static int rtk_snd_es_resume(int device, int wptr)
{
	int adec_id = dev2adec_index(device);
	HAL_AUDIO_ADEC_Open(adec_id);
	HAL_AUDIO_DIRECT_SYSTEM_Open();
	HAL_AUDIO_DIRECT_SYSTEM_Connect();
	HAL_AUDIO_DIRECT_ADEC_Connect(adec_id);
	HAL_AUDIO_SetEsPushWptr(adec_id, (wptr));
	HAL_AUDIO_SetEsPush_PCMFormat(adec_id, es_fs[adec_id], es_nch[adec_id], es_bps[adec_id]);
	if(HAL_AUDIO_DIRECT_StartDecoding(adec_id, direct2hal_codec(es_codec[adec_id])) == NOT_OK)
		return -1;
	return 0;
}

static int rtk_snd_es_stop(int device)
{
	int adec_id = dev2adec_index(device);
	if(HAL_AUDIO_StopDecoding(adec_id) == NOT_OK)
		return -1;
	return 0;
}

static int rtk_snd_es_close(int device)
{
	int adec_id = dev2adec_index(device);
	HAL_AUDIO_SetEsPushMode(adec_id, HAL_AUDIO_ES_NORMAL, 0);
	HAL_AUDIO_DIRECT_SYSTEM_Disconnect();
	HAL_AUDIO_DIRECT_ADEC_Disconnect(adec_id);
	HAL_AUDIO_DIRECT_SYSTEM_Close();
	HAL_AUDIO_ADEC_Close(adec_id);
	return 0;
}

static int rtk_snd_init_decoder_info(struct snd_pcm_substream *substream)
{
	AUDIO_DEC_NEW_FORMAT cmd;
	struct AUDIO_RPC_SENDIO sendio;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	int temp = dpcm->ao_agent|dpcm->ao_pin_id;
	RPC_TOAGENT_PAUSE_SVC(dpcm->card->card, &temp); /* AO pause */

	temp = dpcm->dec_agent;
	RPC_TOAGENT_PAUSE_SVC(dpcm->card->card, &temp); /* Decoder pause */

	sendio.instanceID = dpcm->dec_agent;
	sendio.pinID = dpcm->dec_pin_id;
	RPC_TOAGENT_FLUSH_SVC(dpcm->card->card, &sendio);

	cmd.audioType = htonl(AUDIO_LPCM_DECODER_TYPE);
	cmd.header.type = htonl(AUDIO_DEC_INBAMD_CMD_TYPE_NEW_FORMAT);
	cmd.header.size = htonl(sizeof(AUDIO_DEC_NEW_FORMAT));
	cmd.privateInfo[0] = htonl(runtime->channels);
	cmd.privateInfo[1] = htonl(runtime->sample_bits);
	cmd.privateInfo[2] = htonl(runtime->rate);
	cmd.privateInfo[3] = htonl(0);
	cmd.privateInfo[4] = htonl(0);
	cmd.privateInfo[5] = htonl(0);
	cmd.privateInfo[6] = htonl(0);
	cmd.privateInfo[7] = htonl(0);

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_BE:
	case SNDRV_PCM_FORMAT_U16_BE:
	case SNDRV_PCM_FORMAT_S24_BE:
	case SNDRV_PCM_FORMAT_U24_BE:
	case SNDRV_PCM_FORMAT_S32_BE:
	case SNDRV_PCM_FORMAT_U32_BE:
	case SNDRV_PCM_FORMAT_S24_3BE:
	case SNDRV_PCM_FORMAT_U24_3BE:
	case SNDRV_PCM_FORMAT_S20_3BE:
	case SNDRV_PCM_FORMAT_U20_3BE:
	case SNDRV_PCM_FORMAT_S18_3BE:
	case SNDRV_PCM_FORMAT_U18_3BE:
	case SNDRV_PCM_FORMAT_FLOAT_BE:
	case SNDRV_PCM_FORMAT_FLOAT64_BE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_BE:
		cmd.privateInfo[7] = htonl(AUDIO_BIG_ENDIAN);
		break;
	case SNDRV_PCM_FORMAT_S8:
	case SNDRV_PCM_FORMAT_U8:
	case SNDRV_PCM_FORMAT_S16_LE:
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S24_LE:
	case SNDRV_PCM_FORMAT_U24_LE:
	case SNDRV_PCM_FORMAT_S32_LE:
	case SNDRV_PCM_FORMAT_U32_LE:
	case SNDRV_PCM_FORMAT_S24_3LE:
	case SNDRV_PCM_FORMAT_U24_3LE:
	case SNDRV_PCM_FORMAT_S20_3LE:
	case SNDRV_PCM_FORMAT_U20_3LE:
	case SNDRV_PCM_FORMAT_S18_3LE:
	case SNDRV_PCM_FORMAT_U18_3LE:
	case SNDRV_PCM_FORMAT_FLOAT_LE:
	case SNDRV_PCM_FORMAT_FLOAT64_LE:
	case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE:
	case SNDRV_PCM_FORMAT_MU_LAW:
	case SNDRV_PCM_FORMAT_A_LAW:
	case SNDRV_PCM_FORMAT_IMA_ADPCM:
	case SNDRV_PCM_FORMAT_MPEG:
	case SNDRV_PCM_FORMAT_GSM:
	case SNDRV_PCM_FORMAT_SPECIAL:
	default:
		cmd.privateInfo[7] = htonl(AUDIO_LITTLE_ENDIAN);
		break;
	}
	cmd.wPtr = dpcm->ring[0].beginAddr;

	hw_ring_write(dpcm->inband, &cmd, sizeof(AUDIO_DEC_NEW_FORMAT));
	return 0;
}

static int rtk_snd_decoder_run(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	int result = 0;

	int temp = dpcm->ao_agent|dpcm->ao_pin_id;

	if (dpcm->audiopath == AUDIO_PATH_DECODER_AO)
		result = RPC_TOAGENT_RUN_SVC(dpcm->card->card, &dpcm->agent_id);

	if (result < 0)
		return result;

	result = RPC_TOAGENT_RUN_SVC(dpcm->card->card, &temp);

	return result;
}

static void rtk_snd_init_decoder_ring(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct RPC_RBUF_HEADER ringbuf_header;

	dpcm->pcm_buf_pos = 0;

	/* scprioren add for UNCAC_BASE */
	dpcm->ring = dpcm->hw_ring;
	dpcm->inband = dpcm->hw_inband_ring;

	/* set bitstream ring buffer */
	dpcm->buffer_byte_size =
		frames_to_bytes(runtime, runtime->buffer_size);

	dpcm->ring[0].beginAddr =	htonl(0xa0000000|(((ulong) runtime->dma_addr) & 0x1FFFFFFF));
	dpcm->ring[0].bufferID = htonl(RINGBUFFER_STREAM);
	dpcm->ring[0].size = htonl((ulong) dpcm->buffer_byte_size);
	dpcm->ring[0].writePtr = dpcm->ring[0].beginAddr;
	dpcm->ring[0].readPtr[0] = dpcm->ring[0].beginAddr;
	dpcm->ring[0].readPtr[1] = dpcm->ring[0].beginAddr;
	dpcm->ring[0].readPtr[2] = dpcm->ring[0].beginAddr;
	dpcm->ring[0].readPtr[3] = dpcm->ring[0].beginAddr;
	dpcm->ring[0].numOfReadPtr = htonl(1);

	dpcm->ring_bak[0].beginAddr = (unsigned long) runtime->dma_addr;
	dpcm->ring_bak[0].size = dpcm->buffer_byte_size;
	dpcm->ring_bak[0].writePtr = dpcm->ring_bak[0].beginAddr;
	dpcm->ring_bak[0].readPtr[0] = dpcm->ring_bak[0].beginAddr;
	dpcm->ring_bak[0].readPtr[1] = dpcm->ring_bak[0].beginAddr;
	dpcm->ring_bak[0].readPtr[2] = dpcm->ring_bak[0].beginAddr;
	dpcm->ring_bak[0].readPtr[3] = dpcm->ring_bak[0].beginAddr;
	ALSA_DEBUG_INFO("[ALSA] ring[0].readPtr[0] = %lx, ring_bak[0].writePtr = %lx\n",
		dpcm->ring[0].readPtr[0], dpcm->ring_bak[0].writePtr);
	ALSA_DEBUG_INFO("[ALSA] decoder input ring buffer %lx %lx\n",
		dpcm->ring_bak[0].size,
		dpcm->ring_bak[0].beginAddr);

	ringbuf_header.instanceID = dpcm->agent_id;
	ringbuf_header.pin_id = BASE_BS_IN;
	ringbuf_header.rbuf_list[0] = (ulong) (&dpcm->ring[0]);
	ringbuf_header.rd_idx = 0;
	ringbuf_header.listsize = 1;

	RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &ringbuf_header);

	/* setup inband ring buffer */
	ALSA_DEBUG_INFO("[ALSA] hw_inband_data = %lx\n",
		(unsigned long) dpcm->hw_inband_data);
	dpcm->inband[0].beginAddr =
		htonl(0xa0000000|
		((unsigned long)dpcm->hw_inband_data &
		0x1FFFFFFF));
	ALSA_DEBUG_INFO("[ALSA] inband[0].beginAddr = %lx\n", dpcm->inband[0].beginAddr);
	dpcm->inband[0].size = htonl(sizeof(dpcm->hw_inband_data));
	dpcm->inband[0].readPtr[0] = dpcm->inband[0].beginAddr;
	dpcm->inband[0].writePtr = dpcm->inband[0].beginAddr;
	dpcm->inband[0].numOfReadPtr = htonl(1);

	ringbuf_header.instanceID = dpcm->agent_id;
	ringbuf_header.pin_id = INBAND_QUEUE;
	ringbuf_header.rbuf_list[0] = (ulong) (&dpcm->inband[0]);
	ringbuf_header.rd_idx = 0;
	ringbuf_header.listsize = 1;

	RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &ringbuf_header);
	ALSA_DEBUG_INFO("[ALSA] ring[0].readPtr[0] = %lx\n", dpcm->ring[0].readPtr[0]);
}

static int rtk_snd_init_connect_decoder_ao(
	struct snd_pcm_substream *substream)
{
	int i;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	int ring_size;
	struct RPC_RBUF_HEADER ringbuf_header;
	struct AUDIO_RPC_CONNECTION connection;

	dpcm->ao_in_ring = (void *)(RINGBUFFER_HEADER *)
		((unsigned long) dpcm->ao_in_ring_instance);

	ALSA_DEBUG_INFO("[ALSA] ao_in_ring = %lx\n", (unsigned long) dpcm->ao_in_ring);

	ring_size = 8 * 1024;
	ALSA_DEBUG_INFO("[ALSA] internal ring size per channel : %d bytes\n", ring_size);

	for (i = 0; i < runtime->channels; i++) {
		dpcm->ao_in_ring_p[i] =
			dvr_malloc_specific(8 * 1024, GFP_DCU1);
		if (dpcm->ao_in_ring_p[i] == NULL)
			return -ENOMEM;
		ALSA_DEBUG_INFO("[ALSA] ao_in_ring_p[%d] = %lx\n",
			i, (unsigned long)dpcm->ao_in_ring_p[i]);

		dpcm->ao_in_ring[i].beginAddr =
			htonl(0xa0000000|
			(((unsigned long) dpcm->ao_in_ring_p[i])&
			0x1FFFFFFF));

		dpcm->ao_in_ring[i].size = htonl((ring_size));
		dpcm->ao_in_ring[i].readPtr[0] = dpcm->ao_in_ring[i].beginAddr;
		dpcm->ao_in_ring[i].readPtr[1] = dpcm->ao_in_ring[i].beginAddr;
		dpcm->ao_in_ring[i].readPtr[2] = dpcm->ao_in_ring[i].beginAddr;
		dpcm->ao_in_ring[i].readPtr[3] = dpcm->ao_in_ring[i].beginAddr;
		dpcm->ao_in_ring[i].writePtr = dpcm->ao_in_ring[i].beginAddr;
		dpcm->ao_in_ring[i].numOfReadPtr = htonl(1);
	}

	ringbuf_header.instanceID = dpcm->agent_id;
	ringbuf_header.pin_id = PCM_OUT_RTK;
	ringbuf_header.rbuf_list[0] = (unsigned long) &dpcm->ao_in_ring[0];
	ringbuf_header.rbuf_list[1] = (unsigned long) &dpcm->ao_in_ring[1];
	ringbuf_header.rd_idx = -1;
	ringbuf_header.listsize = runtime->channels;

	RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &ringbuf_header);

	ringbuf_header.instanceID = dpcm->ao_agent;
	ringbuf_header.pin_id = dpcm->ao_pin_id;
	ringbuf_header.rd_idx = 0;
	ringbuf_header.rbuf_list[0] = (unsigned long) &dpcm->ao_in_ring[0];
	ringbuf_header.rbuf_list[1] = (unsigned long) &dpcm->ao_in_ring[1];
	ringbuf_header.listsize = runtime->channels;

	RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &ringbuf_header);

	connection.desInstanceID = dpcm->ao_agent;
	connection.srcInstanceID = dpcm->agent_id;

	connection.srcPinID = PCM_OUT_RTK;
	connection.desPinID = dpcm->ao_pin_id;
	RPC_TOAGENT_CONNECT_SVC(dpcm->card->card, &connection);

	return 0;
}
#endif

static void rtk_snd_eos(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;
	struct rtk_snd_pcm *dpcm;
	runtime = substream->runtime;
	dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] %d\n", __LINE__);
	ALSA_DEBUG_INFO("[ALSA] ring[0].writePtr = %x\n", htonl(dpcm->ring[0].writePtr));
	ALSA_DEBUG_INFO("[ALSA] ring[0].readPtr[0] = %x\n", htonl(dpcm->ring[0].readPtr[0]));
	ALSA_DEBUG_INFO("[ALSA] ring[0].beginAddr = %x\n", htonl(dpcm->ring[0].beginAddr));
	ALSA_DEBUG_INFO("[ALSA] ring[0].size = %x\n", htonl(dpcm->ring[0].size));
	ALSA_DEBUG_INFO("[ALSA] ring[0] = %p\n", &dpcm->ring[0]);
	if (dpcm->audiopath == AUDIO_PATH_DECODER_AO)
		RPC_TOAGENT_INBAND_EOS_SVC(substream);
	else
		RPC_TOAGENT_EOS_SVC(substream);

	/* end of hw buffer flushing */
	dpcm->flush_state = RTK_SND_FLUSH_STATE_FINISH;
}

static int rtk_snd_flush(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_SENDIO sendio;
	int ret;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	sendio.instanceID = dpcm->agent_id;
	sendio.pinID = dpcm->ao_pin_id;

	ret = RPC_TOAGENT_FLUSH_SVC(dpcm->card->card, &sendio);
	/* end of hw buffer flushing */

	dpcm->flush_state = RTK_SND_FLUSH_STATE_FINISH;

	if (ret < 0)
		return -1;

	return 0;
}

static int rtk_snd_pause(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;
	struct rtk_snd_pcm *dpcm;
	int temp, ret;

	runtime = substream->runtime;
	dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	temp = dpcm->ao_agent|dpcm->ao_pin_id;
	ret = RPC_TOAGENT_PAUSE_SVC(dpcm->card->card, &temp);

	if (ret < 0)
		return -1;

	return 0;
}

static int rtk_snd_init_ao_ring(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct rtk_snd_pcm *dpcm_audio = (struct rtk_snd_pcm *) (dpcm->phy_addr|0xa0000000);

	struct RPC_RBUF_HEADER *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long retval;

	int i, ret = 0;
	dma_addr_t dat;
	void *p;
	unsigned long count, phy_addr_ring;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	dpcm->pcm_buf_pos = 0;
	dpcm->ring = (RINGBUFFER_HEADER *)((unsigned long) dpcm->hw_ring);
	dpcm->ao_in_ring = dpcm->ring;
	dpcm->buffer_byte_size =
		runtime->buffer_size * frames_to_bytes(runtime, 1);

	ALSA_DEBUG_INFO("[ALSA] buffer_byte_size %ld\n", dpcm->buffer_byte_size);

	for (i = 0; i < 1; i++) {
		dpcm->ring[i].beginAddr = htonl(0xa0000000|(((unsigned long) runtime->dma_addr) & 0x1FFFFFFF));
		dpcm->ring_bak[i].beginAddr = (unsigned long) runtime->dma_addr;

		dpcm->ring[i].bufferID = htonl(RINGBUFFER_STREAM);
		dpcm->ring[i].size = htonl(dpcm->buffer_byte_size);
		dpcm->ring[i].writePtr = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[0] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[1] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[2] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[3] = dpcm->ring[i].beginAddr;

		dpcm->ring_bak[i].size = dpcm->buffer_byte_size;
		dpcm->ring_bak[i].writePtr = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[1] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[2] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[3] = dpcm->ring_bak[i].beginAddr;
	}

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_KERNEL | __GFP_DMA);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct RPC_RBUF_HEADER *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct RPC_RBUF_HEADER) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct RPC_RBUF_HEADER) + 8) &
			ALIGN4));

	info->instanceID = htonl(dpcm->ao_agent);
	info->pin_id = htonl(dpcm->ao_pin_id);
	info->rd_idx = htonl(0);
	info->listsize = htonl(1);

	for (i = 0; i < 1; i++) {
		count = (unsigned long)&dpcm->ring[i] - (unsigned long)dpcm;
		phy_addr_ring = (unsigned long)dpcm_audio + count;

		info->rbuf_list[i] =
			htonl(0xa0000000|
			((((unsigned long)phy_addr_ring))&0x1FFFFFFF));
	}

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_INIT_RINGBUF,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &retval)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ALSA_DEBUG_INFO("[ALSA] RING INITIALIZED\n");
	ALSA_DEBUG_INFO("[ALSA] channel = %d\n", runtime->channels);
	ALSA_DEBUG_INFO("[ALSA] frames_to_bytes = %zd\n", frames_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] samples_to_bytes = %zd\n", samples_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] buffer size = %d\n", (int) runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] ACCESS MODE=%s", snd_access_mode[runtime->access]);
	for (i = 0; i < runtime->channels; i++) {
		ALSA_DEBUG_INFO("[ALSA] buf_header[%d].magic = %x\n",
			i, ntohl(dpcm->ring[i].magic));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].beginAddr = %x\n",
			i, ntohl(dpcm->ring[i].beginAddr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].size = %x\n",
			i, ntohl(dpcm->ring[i].size));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[0]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[1] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[1]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %x\n",
			i, ntohl(dpcm->ring[i].writePtr));
	}

#if 0
    ret = KHAL_AUDIO_ALSA_SNDOUT_Connect(sndout_cmd_info.opened_device, dpcm->ao_pin_id);
    if(ret != OK)
        pr_err("[%s:%d] fail!\n",__FUNCTION__,__LINE__);
#endif

exit:
	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return ret;
}

static int rtk_snd_init_ai_ring(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	struct RPC_RBUF_HEADER *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long retval;

	int ret = 0;
	dma_addr_t dat;
	void *p;
	int i;

	/* allocate ring buffer */
	dpcm->ring = (RINGBUFFER_HEADER *)((unsigned long) dpcm->hw_ring);
	/* frames per buffer * sample_bytes */
	dpcm->buffer_byte_size = runtime->buffer_size * dpcm->input_sample_bytes;

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	dpcm->ring_p = dma_alloc_coherent(dpcm->card->card->dev,runtime->channels * dpcm->buffer_byte_size,&dpcm->ring_phy_addr, GFP_ATOMIC);

	for (i = 0; i < runtime->channels; i++) {
		dpcm->hw_ai_ring_data[i] =
			dpcm->ring_p + i * dpcm->buffer_byte_size;

		if (dpcm->hw_ai_ring_data[i] == NULL)
			return -ENOMEM;

		/* uncache address for Audio FW */
		dpcm->ring[i].beginAddr =
			htonl(0xa0000000|
			(((unsigned long) dpcm->hw_ai_ring_data[i]) & 0x1FFFFFFF));
		dpcm->ring_bak[i].beginAddr =
			((unsigned long) dpcm->hw_ai_ring_data[i]);

		dpcm->ring[i].bufferID = htonl(RINGBUFFER_STREAM);
		dpcm->ring[i].size = htonl((ulong) dpcm->buffer_byte_size);
		dpcm->ring[i].numOfReadPtr = htonl(1);
		dpcm->ring[i].writePtr = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[0] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[1] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[2] = dpcm->ring[i].beginAddr;
		dpcm->ring[i].readPtr[3] = dpcm->ring[i].beginAddr;

		dpcm->ring_bak[i].size = dpcm->buffer_byte_size;
		dpcm->ring_bak[i].numOfReadPtr = 1;
		dpcm->ring_bak[i].writePtr = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[1] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[2] = dpcm->ring_bak[i].beginAddr;
		dpcm->ring_bak[i].readPtr[3] = dpcm->ring_bak[i].beginAddr;
	}

	/* send init ring rpc */

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct RPC_RBUF_HEADER *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct RPC_RBUF_HEADER) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct RPC_RBUF_HEADER) + 8) &
			ALIGN4));

	info->instanceID = htonl(dpcm->ai_cap_agent);
	info->pin_id = htonl(dpcm->ai_cap_pin);
	info->rd_idx = htonl(-1);
	info->listsize = htonl(runtime->channels);

	for (i = 0; i < runtime->channels; i++) {
		/* uncache address for Audio FW */
		info->rbuf_list[i] =
			htonl(0xa0000000|(((unsigned long) &dpcm->ring[i])&0x1FFFFFFF));
	}

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_INIT_RINGBUF,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &retval)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

	ALSA_DEBUG_INFO("[ALSA] ALSA RING INITIALIZED\n");
	ALSA_DEBUG_INFO("[ALSA] channel = %d\n", runtime->channels);
	ALSA_DEBUG_INFO("[ALSA] frames_to_bytes = %zd\n", frames_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] samples_to_bytes = %zd\n", samples_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] buffer size = %d\n", (int) runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] ACCESS MODE=%s", snd_access_mode[runtime->access]);
	for (i = 0; i < runtime->channels; i++) {
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].beginAddr = %x\n",
			i, ntohl(dpcm->ring[i].beginAddr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].size = %x\n",
			i, ntohl(dpcm->ring[i].size));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[0]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %x\n",
			i, ntohl(dpcm->ring[i].writePtr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %lx\n",
			i, dpcm->ring_bak[i].readPtr[0]);
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %lx\n",
			i, dpcm->ring_bak[i].writePtr);
	}

exit:
	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return ret;
}

/* set AO config */
static int rtk_snd_set_ao_config(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
    long ret = S_OK;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_AO_OUTPUT_CONFIG;
    audioConfig.value[0] = runtime->channels;
    audioConfig.value[1] = runtime->rate;
    audioConfig.value[2] = dpcm->ao_cap_pin;
    audioConfig.value[3] = dpcm->interleave;
    audioConfig.value[4] = g_capture_dev;

    pr_info("[ALSA] %s, %d ch(%d) reate(%d) pin(%d) interl(%d) cap_dev(%d)\n", __FUNCTION__, __LINE__, runtime->channels, runtime->rate, dpcm->ao_cap_pin, dpcm->interleave, g_capture_dev);

    ret = rtkaudio_send_audio_config(&audioConfig);

    return ret;
}

/* set AO config: wisa resamplser */
static int rtk_snd_set_wisa_resampler(void)
{
    long ret = S_OK;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_WISA_RESAMPLER;
    audioConfig.value[0] = g_capture_wisa_resampler;

    pr_info("[ALSA] %s, %d resampler(%d)\n", __FUNCTION__, __LINE__, audioConfig.value[0]);

    ret = rtkaudio_send_audio_config(&audioConfig);

    return ret;
}

static int rtk_snd_get_clock_count(int *clock_count, int *clock_rate)
{
    long ret = S_OK;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_GET_INPUT_TIME_COUNT;
    ret = rtkaudio_send_audio_config(&audioConfig);

    if(ret == S_OK) {
        *clock_count = audioConfig.value[0];
        *clock_rate  = audioConfig.value[1];
    } else {
        *clock_count = 0;
        *clock_rate  = 0;
    }

    return ret;
}

/* set AO capture device mute */
static int rtk_snd_set_ao_capture_mute(int mute)
{
    int i;
    long ret = S_OK;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;

    if((ao_cap_agent <= -1) || (ao_cap_pin <= -1))
    {
        pr_info("[ALSA]%s, %d ao_cap_agent(%d), ao_cap_pin(%d) not created!\n", __FUNCTION__, __LINE__, ao_cap_agent, ao_cap_pin);
        return -1;
    }

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_MUTE;
    audioConfig.value[0] = ao_cap_agent;//instance id
    audioConfig.value[1] = ENUM_DEVICE_PCM_CAPTURE;//device id
    audioConfig.value[2] = ao_cap_pin;//pin id
    audioConfig.value[3] = 0xff;//8 channels

    for(i = 0; i < 8; i++)
        audioConfig.value[4+i] = mute;

    pr_info("[ALSA] %s, %d cap_dev(%d) mute(%d)\n", __FUNCTION__, __LINE__, g_capture_dev, mute);

    ret = rtkaudio_send_audio_config(&audioConfig);

    return ret;
}

/* set AO capture device volume */
static int rtk_snd_set_ao_capture_volume(HAL_AUDIO_VOLUME_T volume)
{
    long ret = S_OK;
    AUDIO_CONFIG_COMMAND_RTKAUDIO audioConfig;
    int i;
    SINT32 dsp_gain = 0;

    if((ao_cap_agent <= -1) || (ao_cap_pin <= -1))
    {
        ALSA_ERROR("[ALSA][%s:%d] ao_cap_agent(%d), ao_cap_pin(%d) not created, save setting!\n", __FUNCTION__, __LINE__, ao_cap_agent, ao_cap_pin);
        return S_OK;
    }

    dsp_gain = Volume_to_DSPGain(volume);

    memset(&audioConfig, 0, sizeof(AUDIO_CONFIG_COMMAND_RTKAUDIO));

    audioConfig.msg_id = AUDIO_CONFIG_CMD_SET_VOLUME;
    audioConfig.value[0] = ao_cap_agent;//instance id
    audioConfig.value[1] = ENUM_DEVICE_PCM_CAPTURE;//device id
    audioConfig.value[2] = ao_cap_pin;//pin id
    audioConfig.value[3] = 0xff;//8 channels

    for(i = 0; i < 8; i++)
        audioConfig.value[4+i] = dsp_gain;

    audioConfig.value[12] = volume.fineVol % 4;  // 0~3

    ALSA_INFO("[ALSA] %s cap_dev(%d) volume(%d, %d)\n", __FUNCTION__, g_capture_dev,  volume.mainVol,  volume.fineVol);

    ret = rtkaudio_send_audio_config(&audioConfig);

    return ret;
}

/* init AO capture ring */
static int rtk_snd_init_ao_capture_ring(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	struct RPC_RBUF_HEADER *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long retval;

	int ret = 0;
	dma_addr_t dat;
	void *p;
	int i;

	/* allocate ring buffer */
	dpcm->ring = (RINGBUFFER_HEADER *)((unsigned long) dpcm->hw_ring);
	dpcm->buffer_byte_size =
		runtime->buffer_size * frames_to_bytes(runtime, 1);

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	dpcm->ring_p = dma_alloc_coherent(dpcm->card->card->dev,dpcm->buffer_byte_size,&dpcm->ring_phy_addr, GFP_KERNEL);
	ALSA_INFO("[ALSA] %s ring_p(%x) size %d*%d=%d\n", __FUNCTION__, dpcm->ring_p, runtime->buffer_size, frames_to_bytes(runtime, 1),dpcm->buffer_byte_size);

    if(dpcm->ring_p == NULL)
        ALSA_ERROR("[ALSA] %s, %d Can not allocate ring buffer for capture! \n", __FUNCTION__, __LINE__);

    if(dpcm ->interleave== 1)//interleave
    {
        //Only use 1 ch for audio fw interleave output
        for (i = 0; i < 1; i++) {
            dpcm->hw_ai_ring_data[i] = dpcm->ring_p + i * dpcm->buffer_byte_size;
            if (dpcm->hw_ai_ring_data[i] == NULL)
                return -ENOMEM;

            /* uncache address for Audio FW */
            dpcm->ring[i].beginAddr = htonl(0xa0000000 |(((unsigned long) dpcm->hw_ai_ring_data[i]) & 0x1FFFFFFF));
            dpcm->ring_bak[i].beginAddr = ((unsigned long) dpcm->hw_ai_ring_data[i]);

            dpcm->ring[i].bufferID = htonl(RINGBUFFER_STREAM);
            dpcm->ring[i].size = htonl((ulong) ((dpcm->buffer_byte_size)));
            dpcm->ring[i].numOfReadPtr = htonl(1);
            dpcm->ring[i].writePtr = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[0] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[1] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[2] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[3] = dpcm->ring[i].beginAddr;

            dpcm->ring_bak[i].size = (dpcm->buffer_byte_size);
            dpcm->ring_bak[i].numOfReadPtr = 1;
            dpcm->ring_bak[i].writePtr = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[1] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[2] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[3] = dpcm->ring_bak[i].beginAddr;
            //pr_info("[ALSA]%s, %d (%d, %x, %d)\n", __FUNCTION__, __LINE__, i, dpcm->ring_bak[i].beginAddr, dpcm->ring_bak[i].size);
        }
    }
    else//non-interleave
    {
        for (i = 0; i < runtime->channels; i++) {
            dpcm->hw_ai_ring_data[i] = dpcm->ring_p + i * (dpcm->buffer_byte_size/runtime->channels);
            if (dpcm->hw_ai_ring_data[i] == NULL)
                return -ENOMEM;

            /* uncache address for Audio FW */
            dpcm->ring[i].beginAddr = htonl(0xa0000000 |(((unsigned long) dpcm->hw_ai_ring_data[i]) & 0x1FFFFFFF));
            dpcm->ring_bak[i].beginAddr = ((unsigned long) dpcm->hw_ai_ring_data[i]);

            dpcm->ring[i].bufferID = htonl(RINGBUFFER_STREAM);
            dpcm->ring[i].size = htonl((ulong) ((dpcm->buffer_byte_size)/runtime->channels));
            dpcm->ring[i].numOfReadPtr = htonl(1);
            dpcm->ring[i].writePtr = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[0] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[1] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[2] = dpcm->ring[i].beginAddr;
            dpcm->ring[i].readPtr[3] = dpcm->ring[i].beginAddr;

            dpcm->ring_bak[i].size = (dpcm->buffer_byte_size/runtime->channels);
            dpcm->ring_bak[i].numOfReadPtr = 1;
            dpcm->ring_bak[i].writePtr = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[1] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[2] = dpcm->ring_bak[i].beginAddr;
            dpcm->ring_bak[i].readPtr[3] = dpcm->ring_bak[i].beginAddr;
            //pr_info("[ALSA] %s, %d (%d, %x, %d)\n", __FUNCTION__, __LINE__, i, dpcm->ring_bak[i].beginAddr, dpcm->ring_bak[i].size);
        }
    }

	/* send init ring rpc */
	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct RPC_RBUF_HEADER *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct RPC_RBUF_HEADER) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct RPC_RBUF_HEADER) + 8) &
			ALIGN4));

    info->instanceID = htonl(dpcm->ao_cap_agent);
    info->pin_id = htonl(dpcm->ao_cap_pin);
    info->rd_idx = htonl(-1);
    if(dpcm ->interleave== 1)//interleave
        info->listsize = htonl(1);//fix at 1 ch
    else
        info->listsize = htonl(runtime->channels);//for runtime->channels

    if(dpcm ->interleave== 1)//interleave
    {
        //only 1 ch for audio fw interleave output
        for (i = 0; i < 1; i++) {
            /* uncache address for Audio FW */
            info->rbuf_list[i] = htonl(0xa0000000|(((unsigned long) &dpcm->ring[i])&0x1FFFFFFF));
        }
    }
    else//non-interleave
    {
        for (i = 0; i < runtime->channels; i++) {
            /* uncache address for Audio FW */
            info->rbuf_list[i] = htonl(0xa0000000|(((unsigned long) &dpcm->ring[i])&0x1FFFFFFF));
        }
    }

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_INIT_RINGBUF,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &retval)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		ret = -1;
		goto exit;
	}

#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] ALSA RING INITIALIZED\n");
	ALSA_DEBUG_INFO("[ALSA] channel = %d\n", runtime->channels);
	ALSA_DEBUG_INFO("[ALSA] frames_to_bytes = %d\n", frames_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] samples_to_bytes = %d\n", samples_to_bytes(runtime, 1));
	ALSA_DEBUG_INFO("[ALSA] buffer size = %d\n", (int) runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] ACCESS MODE=%s", snd_access_mode[runtime->access]);
	for (i = 0; i < runtime->channels; i++) {
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].beginAddr = %x\n",
			i, ntohl(dpcm->ring[i].beginAddr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].size = %x\n",
			i, ntohl(dpcm->ring[i].size));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %x\n",
			i, ntohl(dpcm->ring[i].readPtr[0]));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %x\n",
			i, ntohl(dpcm->ring[i].writePtr));
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].readPtr[0] = %x\n",
			i, dpcm->ring_bak[i].readPtr[0]);
		ALSA_DEBUG_INFO("[ALSA] snd_rtk_buffer_header[%d].writePtr = %x\n",
			i, dpcm->ring_bak[i].writePtr);
	}
#endif

exit:
	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return ret;
}

static void rtk_snd_resume(struct snd_pcm_substream *substream)
{
	unsigned int temp;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	if (dpcm->audiopath == AUDIO_PATH_DECODER_AO) {
		rtk_snd_decoder_run(substream);
	} else {
		ALSA_DEBUG_INFO("[ALSA] Flash Pin =%d\n", dpcm->ao_pin_id);
		temp = (dpcm->ao_agent | dpcm->ao_pin_id);
		RPC_TOAGENT_RUN_SVC(dpcm->card->card, &temp);
	}
}

static int rtk_snd_set_volume(struct snd_card_mars *mars)
{
	dma_addr_t dat;
	void *p;
	unsigned long ret;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID, pin_id, i;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	mars->card->dev->dma_mask = &mars->card->dev->coherent_dma_mask;
	if(dma_set_mask(mars->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(mars->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(mars->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *)((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS)+8) & ALIGN4));

	for (i = 0; i < MIXER_ADDR_MAX; i++) {
		if ((mars->ao_pin_id[i] != 0) && (alsa_agent != -1) &&
			(mars->ao_flash_change[i] != 0)) {

			memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
			instanceID = alsa_agent;
			cmd->instanceID = htonl(instanceID);
			cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME);
			pin_id = mars->ao_pin_id[i];
			cmd->privateInfo[0] = htonl(pin_id);
			cmd->privateInfo[1] = htonl(mars->ao_flash_volume[i]);

			ALSA_DEBUG_INFO("[ALSA] volume = %d, pin_id = %d\n",
				mars->ao_flash_volume[i], pin_id);

			if (audio_send_rpc_command(RPC_AUDIO,
				ENUM_KERNEL_RPC_PRIVATEINFO,
				(unsigned long) cmd_audio,
				(unsigned long) res_audio,
				(unsigned long) res, &ret)) {
				ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
				dma_free_coherent(mars->card->dev, 4096, p, dat);
				return -1;
			}

			mars->ao_flash_change[i] = 0;
		} else {
			ALSA_DEBUG_INFO("[ALSA] no running AO flash pin_id, don't set volume\n");
		}
	}

	dma_free_coherent(mars->card->dev, 4096, p, dat);
	return 0;
}

static void rtk_snd_pause_work(struct work_struct *work)
{
	struct rtk_snd_pcm *dpcm =
		container_of(work, struct rtk_snd_pcm, work_pause);
	if(PLAY_ES_DEVICE(dpcm->substream->pcm->device)) {
		rtk_snd_es_stop(dpcm->substream->pcm->device);
	} else {
		rtk_snd_pause(dpcm->substream);
	}
}

static void rtk_snd_resume_work(struct work_struct *work)
{
	struct rtk_snd_pcm *dpcm =
		container_of(work, struct rtk_snd_pcm, work_resume);
	if(PLAY_ES_DEVICE(dpcm->substream->pcm->device)) {
		rtk_snd_es_resume(dpcm->substream->pcm->device, (dpcm->ring_bak[0].writePtr));
	} else {
		rtk_snd_resume(dpcm->substream);
	}
}

static void rtk_snd_playback_volume_work(struct work_struct *work)
{
	struct snd_card_mars *mars = container_of(work,
		struct snd_card_mars, work_volume);
	rtk_snd_set_volume(mars);
}

static void rtk_snd_eos_work(struct work_struct *work)
{
	struct rtk_snd_pcm *dpcm =
		container_of(work, struct rtk_snd_pcm, work_flush);
	rtk_snd_eos(dpcm->substream);
}

/* not use now
static void rtk_snd_flush_work(struct work_struct *work)
{
	struct rtk_snd_pcm *dpcm =
		container_of(work, struct rtk_snd_pcm, work_flush);
	rtk_snd_flush(dpcm->substream);
}
*/

static int rtk_snd_set_ao_flashpin_volume(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd , *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID, pin_id;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *)((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = alsa_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_CONTROL_FLASH_VOLUME);
	pin_id = dpcm->ao_pin_id;
	cmd->privateInfo[0] = htonl(pin_id);
	cmd->privateInfo[1] = htonl(dpcm->volume);

	ALSA_DEBUG_INFO("[ALSA] dpcm->volume= %d\n", dpcm->volume);
	dpcm->volume_change = 0;

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long) cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return pin_id;
}

#ifdef USE_FIXED_AO_PINID
static int rtk_snd_get_used_pin_id(struct snd_card *card)
{
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID, pin_map;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *)((ulong) dat | 0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat | 0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = alsa_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_QUERY_FLASH_PIN);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long)cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	pin_map = ntohl(cmd->privateInfo[0]);

	dma_free_coherent(card->dev, 4096, p, dat);

	return pin_map;
}

static int rtk_snd_set_flashpin_id(struct snd_card *card, int pin)
{
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *) ((ulong) dat | 0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat | 0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS)+8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = alsa_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_SET_FLASH_PIN);
	cmd->privateInfo[0] = htonl(pin);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long) cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}
#else
static int rtk_snd_query_flashpin_id(struct snd_card *card)
{
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID, pin_id;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *)((ulong) dat | 0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
			((((ulong)(dat | 0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS)+8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = alsa_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_GET_FLASH_PIN);
	cmd->privateInfo[0] = 0xFF;
	cmd->privateInfo[1] = 0xFF;
	cmd->privateInfo[2] = 0xFF;
	cmd->privateInfo[3] = 0xFF;
	cmd->privateInfo[4] = 0xFF;
	cmd->privateInfo[5] = 0xFF;

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long)cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	pin_id = ntohl(cmd->privateInfo[0]);

	dma_free_coherent(card->dev, 4096, p, dat);

	if (pin_id < FLASH_AUDIO_PIN_1 || pin_id > FLASH_AUDIO_PIN_8)
		return -1;

	return pin_id;
}
#endif

static int rtk_snd_release_flashpin_id(struct snd_card *card, int pin_id)
{
	unsigned long ret;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd , *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID;
	dma_addr_t dat;
	void *p;

	if (pin_id < FLASH_AUDIO_PIN_1 || pin_id > FLASH_AUDIO_PIN_8)
			return -1;

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
			((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *) ((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = alsa_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_RELEASE_FLASH_PIN);
	cmd->privateInfo[0] = htonl(pin_id);
	cmd->privateInfo[1] = 0xFF;
	cmd->privateInfo[2] = 0xFF;
	cmd->privateInfo[3] = 0xFF;
	cmd->privateInfo[4] = 0xFF;
	cmd->privateInfo[5] = 0xFF;

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long) cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int rtk_snd_ao_info(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID;

	int temp = dpcm->ao_agent | dpcm->ao_pin_id;
	RPC_TOAGENT_PAUSE_SVC(dpcm->card->card, &temp); /* AO pause */

	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *) ((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = dpcm->ao_agent | dpcm->ao_pin_id;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_CONFIG_CMD_BS_INFO);
	cmd->privateInfo[0] = htonl(runtime->channels); /* channel number */

	ALSA_DEBUG_INFO("[ALSA] format %d\n", runtime->format);
	ALSA_DEBUG_INFO("[ALSA] channels %d\n", runtime->channels);

	if ((dpcm->audiopath == AUDIO_PATH_AO_BYPASS) ||
		(dpcm->audiopath == AUDIO_PATH_AO)) {
		/*
		0	32 bit big endian
		1	24 bit big endian
		2	16 bit big endian
		3	24 bit little endian
		4	16 bit little endian
		5	8 bit
		*/
		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S32_BE:
			cmd->privateInfo[1] = htonl(0);
			break;
		case SNDRV_PCM_FORMAT_S24_BE:
			cmd->privateInfo[1] = htonl(1);
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			cmd->privateInfo[1] = htonl(3);
			break;
		case SNDRV_PCM_FORMAT_S16_BE:
		case SNDRV_PCM_FORMAT_S16_LE:
			cmd->privateInfo[1] = htonl(0x84);
			break;
		case SNDRV_PCM_FORMAT_S8:
			cmd->privateInfo[1] = htonl(5);
			break;
		default:
			cmd->privateInfo[1] = htonl(0);
			break;
		}
	} else {
		cmd->privateInfo[1] = htonl(0);
	}

	ALSA_DEBUG_INFO("[ALSA] privateInfo[1] %d\n", htonl(cmd->privateInfo[1]));
	ALSA_DEBUG_INFO("[ALSA] rate %d\n", runtime->rate);

	cmd->privateInfo[2] = htonl(runtime->rate);
	cmd->privateInfo[4] = htonl(dpcm->ao_pin_id);

	/* channel mapping,
	ALSA driver only supports case 1 and case 2 now */
	switch (runtime->channels) {
	case 1:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(0);
		cmd->privateInfo[7] = htonl(0);
		cmd->privateInfo[8] = htonl(0);
		cmd->privateInfo[9] = htonl(0);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 2:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(0);
		cmd->privateInfo[8] = htonl(0);
		cmd->privateInfo[9] = htonl(0);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 3:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LFE_INDEX);
		cmd->privateInfo[8] = htonl(0);
		cmd->privateInfo[9] = htonl(0);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 4:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LEFT_SURROUND_REAR_INDEX);
		cmd->privateInfo[8] = htonl(AUDIO_RIGHT_SURROUND_REAR_INDEX);
		cmd->privateInfo[9] = htonl(0);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 5:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LEFT_SURROUND_REAR_INDEX);
		cmd->privateInfo[8] = htonl(AUDIO_RIGHT_SURROUND_REAR_INDEX);
		cmd->privateInfo[9] = htonl(AUDIO_CENTER_FRONT_INDEX);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 6:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LEFT_SURROUND_REAR_INDEX);
		cmd->privateInfo[8] = htonl(AUDIO_RIGHT_SURROUND_REAR_INDEX);
		cmd->privateInfo[9] = htonl(AUDIO_CENTER_FRONT_INDEX);
		cmd->privateInfo[10] = htonl(AUDIO_LFE_INDEX);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 7:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LEFT_SURROUND_REAR_INDEX);
		cmd->privateInfo[8] = htonl(AUDIO_RIGHT_SURROUND_REAR_INDEX);
		cmd->privateInfo[9] = htonl(AUDIO_CENTER_FRONT_INDEX);
		cmd->privateInfo[10] = htonl(AUDIO_LFE_INDEX);
		cmd->privateInfo[11] = htonl(AUDIO_CENTER_SURROUND_REAR_INDEX);
		cmd->privateInfo[12] = htonl(0);
		break;
	case 8:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(AUDIO_LEFT_SURROUND_REAR_INDEX);
		cmd->privateInfo[8] = htonl(AUDIO_RIGHT_SURROUND_REAR_INDEX);
		cmd->privateInfo[9] = htonl(AUDIO_CENTER_FRONT_INDEX);
		cmd->privateInfo[10] = htonl(AUDIO_LFE_INDEX);
		cmd->privateInfo[11] = htonl(AUDIO_LEFT_OUTSIDE_FRONT_INDEX);
		cmd->privateInfo[12] = htonl(AUDIO_RIGHT_OUTSIDE_FRONT_INDEX);
		break;
	default:
		cmd->privateInfo[5] = htonl(AUDIO_LEFT_FRONT_INDEX);
		cmd->privateInfo[6] = htonl(AUDIO_RIGHT_FRONT_INDEX);
		cmd->privateInfo[7] = htonl(0);
		cmd->privateInfo[8] = htonl(0);
		cmd->privateInfo[9] = htonl(0);
		cmd->privateInfo[10] = htonl(0);
		cmd->privateInfo[11] = htonl(0);
		cmd->privateInfo[12] = htonl(0);
		break;
	}

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long) cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return 0;
}

int RPC_TOAGENT_CHECK_AO_READY(struct snd_card *card)
{
	unsigned int *info, *info_audio;
	RPCRES_LONG *res, *res_audio;

	unsigned long ret;
	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(unsigned long *)((unsigned long) dat | 0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(RPCRES_LONG) + 8) &
			ALIGN4));

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CHECK_READY,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	if (info == 0) {
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(card->dev, 4096, p, dat);
	return 0;
}

static int rtk_snd_check_ao_ready(struct snd_card *card)
{
	DEFINE_WAIT(wait);
	wait_queue_head_t q;	/* for blocking read */
	long remain_time;
	ALSA_DEBUG_INFO("[ALSA] %s %d enter\n", __func__, __LINE__);

	/* Initialize wait queue... */
	init_waitqueue_head(&q);
	do {
		if (RPC_TOAGENT_CHECK_AO_READY(card) != 0) {
			ALSA_DEBUG_INFO("[ALSA] wait AO READY\n");
		} else {
			ALSA_DEBUG_INFO("[ALSA] AO ready .....\n");
			return 0;
		}

		prepare_to_wait(&q, &wait, TASK_INTERRUPTIBLE);
		remain_time = schedule_timeout(HZ);
		finish_wait(&q, &wait);
	} while (remain_time == 0);

	return 1;
}

static int rtk_snd_create_ao_agent(struct snd_card *card)
{
	struct AUDIO_RPC_INSTANCE *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;

	dma_addr_t dat;
	void *p;

	card->dev->dma_mask = &card->dev->coherent_dma_mask;
	if(dma_set_mask(card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_INSTANCE *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_INSTANCE) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_INSTANCE) + 8) &
			ALIGN4));

	info->instanceID = htonl(0);
	info->type = htonl(AUDIO_OUT);
	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(card->dev, 4096, p, dat);
		return -1;
	}

	alsa_agent = ntohl(res->data);

	dma_free_coherent(card->dev, 4096, p, dat);
	ALSA_DEBUG_INFO("[ALSA] AO agent ID = %d\n", alsa_agent);
	return 0;
}

/* create AI agent to get Instance ID for capture */
static int rtk_snd_create_ai_agent(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_INSTANCE *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;
	dma_addr_t dat;
	void *p;

	if (snd_open_ai_count >= MAX_AI_DEVICES) {
		ALSA_ERROR("[ALSA] too more capture open %d\n", __LINE__);
		return -1;
	}

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);
	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_INSTANCE *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_INSTANCE) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_INSTANCE) + 8) &
			ALIGN4));

	info->instanceID = htonl(-1);
	info->type = htonl(AUDIO_IN);

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dpcm->ai_cap_agent = ntohl(res->data);

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	ALSA_DEBUG_INFO("[ALSA] AI agent ID = %d\n", dpcm->ai_cap_agent);
	snd_open_ai_count++;
	return 0;
}

/* create AO agent to get Instance ID for capture */
static int rtk_snd_create_ao_capture_agent(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	struct AUDIO_RPC_INSTANCE *info, *info_audio;
	RPCRES_LONG *res, *res_audio;
	unsigned long ret;
	dma_addr_t dat;
	void *p;

	if (snd_open_ai_count >= MAX_AI_DEVICES) {
		ALSA_ERROR("[ALSA] capture busy %d\n", __LINE__);
		return -1;
	}

	if (rtk_snd_check_ao_ready(dpcm->card->card)) {
		ALSA_ERROR("[ALSA] ao not ready\n");
		return -1;
	}

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	info = p;
	info_audio = (void *)(struct AUDIO_RPC_INSTANCE *)((ulong) dat|0xa0000000);

	res = (void *)(RPCRES_LONG *)
		((((unsigned long)p + sizeof(struct AUDIO_RPC_INSTANCE) + 8) & ALIGN4));
	res_audio = (void *)(RPCRES_LONG *)
		(((((unsigned long) dat|0xa0000000) + sizeof(struct AUDIO_RPC_INSTANCE) + 8) &
			ALIGN4));

	info->instanceID = htonl(0);
	info->type = htonl(AUDIO_OUT);

	ALSA_DEBUG_INFO("[ALSA] [%s %d]\n", __func__, __LINE__);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_CREATE_AGENT,
		(unsigned long) info_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %s %d RPC fail\n", __func__, __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dpcm->ao_cap_agent = ntohl(res->data);
	ao_cap_agent = dpcm->ao_cap_agent;

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	ALSA_DEBUG_INFO("[ALSA] AO capture agent ID = %d\n", dpcm->ao_cap_agent);
	snd_open_ai_count++;
	return 0;
}

static int rtk_snd_init(struct snd_card *card)
{
	static int init = 1;
	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	if (init == 0)
		return 0;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	if (rtk_snd_check_ao_ready(card))
		return -1;

	rtk_snd_create_ao_agent(card);
	init = 0;
	return 0;
}

static int rtk_snd_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	int pin, ret;
#ifdef USE_FIXED_AO_PINID
	int pinmap[8], i, error_handle, dev_num;

	error_handle = 0;
	dev_num = substream->pcm->device;
#endif

	pin = 0;
	ret = 0;

	dpcm->ao_pin_id = 0;

	if (snd_open_count >= SNDRV_CARDS) {
		ALSA_ERROR("[ALSA] too more open %d\n", SNDRV_CARDS);
		ret = -1;
		goto exit;
	}

	if (PLAY_ES_DEVICE(substream->pcm->device)) {
		dpcm->ao_pin_id = BASE_BS_IN;
	} else {
#ifdef USE_FIXED_AO_PINID
		pin = rtk_snd_get_used_pin_id(dpcm->card->card);
		ALSA_DEBUG_INFO("[ALSA] FW used pinmap %x\n", pin);

		for(i = 0; i < 8; i++) {
			pinmap[i] = pin % 2;
			pin = pin / 2;
		}

		if(used_ao_pin[dev_num] == 0 && pinmap[dev_num] == 1) {
			dpcm->ao_pin_id = FLASH_AUDIO_PIN_1 + dev_num;
			used_ao_pin[dev_num] = 1;
			if (rtk_snd_set_flashpin_id(dpcm->card->card, dpcm->ao_pin_id)) {
				ALSA_ERROR("[ALSA] set flashpin fail\n");
				ret = -1;
				goto exit;
			}
			ALSA_DEBUG_INFO("[ALSA] use fixed pin %d\n", dev_num);
		} else {
			ALSA_ERROR("[ALSA] device %d already opened\n", dev_num);
			error_handle = 1;
		}

		if (error_handle == 1) {
			if (used_ao_pin[dev_num] == 0) {
				ALSA_ERROR("[ALSA] error handle for pin release\n");
				dpcm->ao_pin_id = FLASH_AUDIO_PIN_1 + dev_num;
				used_ao_pin[dev_num] = 1;

				/* error handle for preclose */
				if (flush_error[dev_num] > 0) {
					rtk_snd_flush(substream);
					flush_error[dev_num] = 0;
				}
				if (pause_error[dev_num] > 0) {
					rtk_snd_pause(substream);
					pause_error[dev_num] = 0;
				}
				if (close_error[dev_num] > 0) {
					rtk_snd_close(substream);
					close_error[dev_num] = 0;
				}
				if (release_error[dev_num] > 0) {
					rtk_snd_release_flashpin_id(dpcm->card->card, dpcm->ao_pin_id);
					release_error[dev_num] = 0;
				}

				if (rtk_snd_set_flashpin_id(dpcm->card->card, dpcm->ao_pin_id)) {
					ALSA_ERROR("[ALSA] set flashpin fail\n");
					ret = -1;
					goto exit;
				}
				ALSA_DEBUG_INFO("[ALSA] use fixed pin\n");
			} else {
				ALSA_ERROR("[ALSA] need check status\n");

				for (i = 0; i < 8; i++) {
					ALSA_ERROR("[ALSA] pinmap[%d] = %d, used_ao_pin[%d] = %d\n",
						i, pinmap[i], i, used_ao_pin[i]);
				}

				ret = -1;
				goto exit;
			}
		}
#else
		dpcm->ao_pin_id = rtk_snd_query_flashpin_id(dpcm->card->card);
		ALSA_DEBUG_INFO("[ALSA] use query pin\n");
		if (dpcm->ao_pin_id < 0) {
			ALSA_ERROR("[ALSA] can't get flash pin");
			ret = -1;
			goto exit;
		}
#endif
    }

	dpcm->ring_init = 0;
	dpcm->volume_change = 0;
	snd_open_count++;

	ALSA_DEBUG_INFO("[ALSA] Audio AgentID = %d pin_id = %d\n",
		dpcm->ao_agent, dpcm->ao_pin_id);

exit:
	return ret;
}

static int rtk_snd_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime;
	struct rtk_snd_pcm *dpcm;
	struct RPC_RBUF_HEADER buf;
	int ret = 0;
	int result = 0;
	int i;
	int temp;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);
	runtime = substream->runtime;
	dpcm = runtime->private_data;

	/* re-initialize ring buffer with null ring */
	buf.instanceID = dpcm->ao_agent;
	buf.pin_id = dpcm->ao_pin_id;
	buf.rd_idx = 0;
	buf.listsize = 0;

	for (i = 0; i < 8; i++)
		buf.rbuf_list[i] = 0;

	/*stop AO agent*/
	temp = dpcm->ao_agent | dpcm->ao_pin_id;
	result = RPC_TOAGENT_STOP_SVC(dpcm->card->card, &temp);

	/* stop decoder */
	if (dpcm->audiopath == AUDIO_PATH_DECODER_AO)
		RPC_TOAGENT_STOP_SVC(dpcm->card->card, &dpcm->agent_id);

	/* initial ring buffer NULL */
	ret = RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &buf);

#if 0
    KHAL_AUDIO_ALSA_SNDOUT_Disconnect(sndout_cmd_info.opened_device, dpcm->ao_pin_id);
    if(ret != OK)
        pr_err("[%s:%d] fail!\n",__FUNCTION__,__LINE__);
#endif

	/* destroy decoder instance if exist */
	if (dpcm->audiopath == AUDIO_PATH_DECODER_AO)
		ret = RPC_TOAGENT_DESTROY_SVC(dpcm->card->card, &dpcm->agent_id);

	return ret;
}

static int snd_realtek_ai_hw_close(struct snd_pcm_substream *substream)
{
	unsigned int instanceID;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	struct RPC_RBUF_HEADER buf;
	int ret = 0;
	int i;

	instanceID = ((dpcm->ai_cap_agent&0xFFFFFFF0)|(dpcm->ai_cap_pin&0xF));

	/* stop AI */
	RPC_TOAGENT_STOP_SVC(dpcm->card->card, &instanceID);
	ALSA_DEBUG_INFO("[ALSA] AI stop success\n");

	/* re-initialize ring buffer with null ring */
	buf.instanceID = dpcm->ai_cap_agent;
	buf.pin_id = dpcm->ai_cap_pin;
	buf.rd_idx = 0;
	buf.listsize = 0;

	for (i = 0; i < 8; i++)
		buf.rbuf_list[i] = 0;

	/* initial ring buffer NULL */
	ret = RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &buf);
	ALSA_DEBUG_INFO("[ALSA] AI init ring success\n");

	return 0;
}

static int snd_realtek_ao_capture_hw_close(struct snd_pcm_substream *substream)
{
	unsigned int instanceID;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	struct RPC_RBUF_HEADER buf;
	int ret = 0;
	int i;

	instanceID = ((dpcm->ao_cap_agent&0xFFFFFFF0)|(dpcm->ao_cap_pin&0xF));

	/* re-initialize ring buffer with null ring */
	buf.instanceID = dpcm->ao_cap_agent;
	buf.pin_id = dpcm->ao_cap_pin;
	buf.rd_idx = 0;
	buf.listsize = 0;

	for (i = 0; i < 8; i++)
		buf.rbuf_list[i] = 0;

	/* initial ring buffer NULL */
	ret = RPC_TOAGENT_INITRINGBUFFER_HEADER_SVC(dpcm->card->card, &buf);
	ALSA_DEBUG_INFO("[ALSA] AO capture init ring success\n");

	return 0;
}

static void rtk_snd_pcm_timer_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct timeval t;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] period_size = %d\n", runtime->period_size);
	ALSA_DEBUG_INFO("[ALSA] period_jiffies = %d, rate = %d\n",
		dpcm->period_jiffies, runtime->rate);
#endif
	dpcm->running = 0;
	dpcm->empty_timeout = jiffies;
	dpcm->elapsed_tasklet_finish = 1;
	dpcm->period_jiffies = 1;

	do_gettimeofday(&t);
	dpcm->pre_time_ms = t.tv_sec*1000 + t.tv_usec/1000;
	dpcm->pre_no_datatime_ms = 0;
	dpcm->pre_wr_ptr = (ntohl(dpcm->ring[0].beginAddr) & 0x1FFFFFFF);
	dpcm->max_level = 0;
	dpcm->min_level = dpcm->buffer_byte_size;
	dpcm->total_data_wb = 0;

	rtk_snd_pcm_freerun_timer_function((unsigned long) dpcm);
	dpcm->running = 1;
}

static void rtk_snd_pcm_timer_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);

	del_timer_sync(dpcm->timer);
}

static void rtk_snd_pcm_capture_timer_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	struct timeval t;

	dpcm->empty_timeout = jiffies;

	dpcm->pcm_buf_pos = 0;
	dpcm->period_jiffies = 1;
	dpcm->elapsed_tasklet_finish = 1;

	do_gettimeofday(&t);
	dpcm->last_time_ms = t.tv_sec*1000 + t.tv_usec/1000;

	ALSA_INFO("[ALSA] %s\n", __func__);

	if(substream->pcm->device == CAPTURE_ES) {
		rtk_snd_pcm_capture_es_timer_function((unsigned long) dpcm);
	} else {
		rtk_snd_pcm_capture_timer_function((unsigned long) dpcm);
	}
	dpcm->running = 1;
}

static void rtk_snd_pcm_capture_timer_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	ALSA_INFO("[ALSA] %s %d\n", __func__, __LINE__);
	del_timer_sync(dpcm->timer);
}

static int rtk_snd_playback_trigger(
	struct snd_pcm_substream *substream, int cmd)
{
	if (cmd == SNDRV_PCM_TRIGGER_START)
		rtk_snd_pcm_timer_start(substream);
	else if (cmd == SNDRV_PCM_TRIGGER_STOP)
		rtk_snd_pcm_timer_stop(substream);
	else
		return -EINVAL;

	return 0;
}

static int rtk_snd_capture_trigger(
	struct snd_pcm_substream *substream, int cmd)
{
	if (cmd == SNDRV_PCM_TRIGGER_START)
		rtk_snd_pcm_capture_timer_start(substream);
	else if (cmd == SNDRV_PCM_TRIGGER_STOP)
		rtk_snd_pcm_capture_timer_stop(substream);
	else
		return -EINVAL;

	return 0;
}

static int rtk_snd_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	unsigned int bps;
	struct timeval t;
	long ctms;
	long ptms;
	int dev;

	ALSA_DEBUG_INFO("[ALSA] [%s]\n", __func__);

	if (dpcm->ring_init == 1) {
		if (dpcm->periods != runtime->periods) {
			ALSA_ERROR("[ALSA] periods different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->period_size != runtime->period_size) {
			ALSA_ERROR("[ALSA] period_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->buffer_size != runtime->buffer_size) {
			ALSA_ERROR("[ALSA] buffer_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->access != runtime->access) {
			ALSA_ERROR("[ALSA] access different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->format != runtime->format) {
			ALSA_ERROR("[ALSA] format different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->channels != runtime->channels) {
			ALSA_ERROR("[ALSA] channels different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->ring_bak[0].beginAddr !=
			(unsigned long) runtime->dma_addr) {
			ALSA_ERROR("[ALSA] dma_addr different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->ring_init == 0) {
			if (PLAY_ES_DEVICE(substream->pcm->device)) {
				rtk_snd_es_stop(substream->pcm->device);
				rtk_snd_es_close(substream->pcm->device);
			} else {
				rtk_snd_flush(substream);
				rtk_snd_pause(substream);
				rtk_snd_close(substream);
			}
		}
	}

	bps = runtime->rate * runtime->channels;
	bps *= snd_pcm_format_width(runtime->format);
	bps /= 8;
	if (bps <= 0)
		return -EINVAL;

	dpcm->pcm_bps = bps;
	dpcm->pcm_jiffie = runtime->rate / HZ;
	dpcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	dpcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	dpcm->pcm_buf_pos = 0;
	dpcm->pcm_irq_pos = 0;
	dpcm->appl_ptr = 0;
	dpcm->flush_state = RTK_SND_FLUSH_STATE_NONE;
	dpcm->periods = runtime->periods;
	dpcm->buffer_size = runtime->buffer_size;
	dpcm->access = runtime->access;
	dpcm->channels = runtime->channels;
	dpcm->format = runtime->format;
	dpcm->period_size = runtime->period_size;
	dpcm->sample_bits = runtime->sample_bits;
	dpcm->rate = runtime->rate;
	dpcm->empty_timeout = jiffies;
	dpcm->sample_jiffies = runtime->rate / 10000;
	dpcm->remain_sample = 0;
	if (dpcm->sample_jiffies == 0)
		dpcm->sample_jiffies = 1;

	dpcm->period_jiffies =
		runtime->period_size * 100 / runtime->rate * 1 / 20;
	if (dpcm->period_jiffies == 0)
		dpcm->period_jiffies = 1;

	dpcm->hwptr_error_times = 0;

#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] dpcm->period_jiffies = %d\n", dpcm->period_jiffies);
#endif
	switch (runtime->access) {
		case SNDRV_PCM_ACCESS_MMAP_INTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] MMAP_INTERLEAVED\n");
#ifdef USE_DECODER
			/*old flow ALSA -> Decoder -> AO */
			dpcm->audiopath = AUDIO_PATH_DECODER_AO;
#else
			/* new flow ALSA -> AO */
			dpcm->audiopath = AUDIO_PATH_AO;
#endif
			break;
		case SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] MMAP_NONINTERLEAVED\n");
			dpcm->audiopath = AUDIO_PATH_AO_BYPASS;
			break;
		case SNDRV_PCM_ACCESS_RW_INTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] RW_INTERLEAVED\n");
#ifdef USE_DECODER
			/*old flow ALSA -> Decoder -> AO */
			dpcm->audiopath = AUDIO_PATH_DECODER_AO;
#else
			/* new flow ALSA -> AO */
			dpcm->audiopath = AUDIO_PATH_AO;
#endif
			break;
		case SNDRV_PCM_ACCESS_RW_NONINTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] RW_NONINTERLEAVED\n");
			dpcm->audiopath = AUDIO_PATH_AO_BYPASS;
			break;
		default:
			ALSA_DEBUG_INFO("[ALSA][%d] unsupport mode\n", __LINE__);
			return -1;
			break;
	}

	if (dpcm->audiopath == AUDIO_PATH_AO) {
		switch (runtime->format) {
#ifdef USE_DECODER
		case SNDRV_PCM_FORMAT_S32_BE:
		case SNDRV_PCM_FORMAT_S24_BE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S8:
#endif
		case SNDRV_PCM_FORMAT_S16_BE:
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		default:
			ALSA_DEBUG_INFO("[ALSA] unsupport format\n");
			return -1;
			break;
		}
	}

	if (dpcm->ring_init == 1) {
		if ((dpcm->pcm_buf_pos != dpcm->appl_ptr) ||
			(runtime->status->hw_ptr != runtime->control->appl_ptr)) {
		ALSA_DEBUG_INFO("[ALSA] reprepare!!\n");
		spin_lock(dpcm->pcm_lock);
		dpcm->pcm_buf_pos = dpcm->appl_ptr;
		runtime->status->hw_ptr = runtime->control->appl_ptr;
		dpcm->ring_bak[0].writePtr = dpcm->ring_bak[0].beginAddr +
			(dpcm->appl_ptr % runtime->buffer_size) * dpcm->output_frame_bytes;
		dpcm->ring[0].writePtr = htonl(0xa0000000|
			(dpcm->ring_bak[0].writePtr & 0x1FFFFFFF));
		spin_unlock(dpcm->pcm_lock);
		if(PLAY_ES_DEVICE(substream->pcm->device)) {
			rtk_snd_es_resume(substream->pcm->device, (dpcm->ring_bak[0].writePtr));
		} else {
			rtk_snd_pause(substream);
			rtk_snd_flush(substream);
			rtk_snd_resume(substream);

			do_gettimeofday(&t);
			ptms = t.tv_sec*1000 + t.tv_usec/1000;

			/* wait ADSP finish flush */
			while (1) {
				if ((dpcm->ring_bak[0].writePtr & 0x1FFFFFFF) ==
						((ntohl(dpcm->ring[0].readPtr[0]) & 0x1FFFFFFF))) {
					ALSA_DEBUG_INFO("[ALSA] wp %lx, rp %x\n",
							dpcm->ring_bak[0].writePtr, ntohl(dpcm->ring[0].readPtr[0]));
					break;
				}

				do_gettimeofday(&t);
				ctms = t.tv_sec*1000 + t.tv_usec/1000;
				if ((ctms - ptms) >= 100) {
					/* wait timeout prevent deadlock */
					ALSA_ERROR("[ALSA] wait ADSP flush timeout!!!\n");
					break;
				}
			}
		}
		}
		return 0;
	}

	dpcm->ring_init = 1;
	dev = substream->pcm->device;

	ALSA_DEBUG_INFO("[ALSA] freerun = %d\n", dpcm->freerun);
	ALSA_DEBUG_INFO("[ALSA] start_threshold = %lx\n", runtime->start_threshold);
	ALSA_DEBUG_INFO("[ALSA] stop_threshold = %lx\n", runtime->stop_threshold);
	ALSA_DEBUG_INFO("[ALSA] avail_min = %lx\n", runtime->control->avail_min);
	ALSA_DEBUG_INFO("[ALSA] buffer_size = %lx\n", runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] period_size = %lx\n", runtime->period_size);
	ALSA_DEBUG_INFO("[ALSA] sample rate = %d\n", runtime->rate);

	if (PLAY_ES_DEVICE(dev)) {
		ALSA_DEBUG_INFO("[ALSA] ES play\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id   = dpcm->ao_pin_id;

		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		if (rtk_snd_es_init_dec_ring(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}
		rtk_snd_es_resume(dev, (dpcm->ring_bak[0].writePtr));
	} else if (dpcm->audiopath == AUDIO_PATH_DECODER_AO) {
		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		rtk_snd_create_decoder_agent(substream);

		ALSA_DEBUG_INFO("[ALSA] dec_agent = %d, dec_pin_id = %d\n",
			dpcm->dec_agent, dpcm->dec_pin_id);

		rtk_snd_init_decoder_ring(substream);
		if (rtk_snd_init_connect_decoder_ao(substream)) {
			ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);
			return -ENOMEM;
		}

		/*
		*	AO Pause
		*	Decoder Pause
		*	Decoder Flush
		*	Write Inband data
		*/
		rtk_snd_init_decoder_info(substream);
		rtk_snd_set_ao_flashpin_volume(substream);
		rtk_snd_resume(substream);

		/*
		substream->ops->silence = NULL;
		substream->ops->copy = snd_card_std_copy;
		*/

	} else if (dpcm->audiopath == AUDIO_PATH_AO_BYPASS) {

		ALSA_DEBUG_INFO("[ALSA] AO BYPASS\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id = dpcm->ao_pin_id;
		dpcm->extend_to_32be_ratio = 0;
		dpcm->output_frame_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = samples_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		/*
		substream->ops->copy = NULL;
		substream->ops->silence = NULL;
		*/

		if (rtk_snd_init_ao_ring(substream))
			return -ENOMEM;

		if (rtk_snd_ao_info(substream)) {
			ALSA_ERROR("[ALSA] %s %d\n", __func__, __LINE__);
			return -ENOMEM;
		}
		rtk_snd_resume(substream);
	} else if (dpcm->audiopath == AUDIO_PATH_AO) {
		ALSA_DEBUG_INFO("[ALSA] AO interleaved\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id = dpcm->ao_pin_id;
		dpcm->card->ao_pin_id[dev] = dpcm->ao_pin_id;
		dpcm->volume = dpcm->card->ao_flash_volume[dev];

		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		/*
		substream->ops->copy = NULL;
		substream->ops->silence = NULL;
		*/

		if (rtk_snd_init_ao_ring(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}

		if (rtk_snd_ao_info(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}

		rtk_snd_set_ao_flashpin_volume(substream);
		rtk_snd_resume(substream);
	}

	return 0;
}

static int rtk_snd_playback_prepare(struct snd_pcm_substream *substream)
{
	return rtk_snd_pcm_prepare(substream);
}

#ifndef CONFIG_CUSTOMER_TV006
static void get_mux_in(char *mux_in, struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	switch (dpcm->path_src) {
	case AUDIO_IPT_SRC_BBADC:
		if (!strcmp(mux_in, "MUTE_ALL"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_MUTE_ALL;
		else if (!strcmp(mux_in, "MIC1"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_MIC1;
		else if (!strcmp(mux_in, "MIC2"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_MIC2;
		else if (!strcmp(mux_in, "AIN1"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_AIN1;
		else if (!strcmp(mux_in, "AIN2"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_AIN2;
		else if (!strcmp(mux_in, "AIN3"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_AIN3;
		else if (!strcmp(mux_in, "AIO1"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_AIO1;
		else if (!strcmp(mux_in, "AIO2"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_AIO2;
		else if (!strcmp(mux_in, "DMIC"))
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_DMIC;
		else
			dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_UNKNOWN;
		break;
	case AUDIO_IPT_SRC_SPDIF:
		if (!strcmp(mux_in, "IN"))
			dpcm->spdifi_mux_in = AUDIO_SPDIFI_SRC_IN;
		else if (!strcmp(mux_in, "HDMI"))
			dpcm->spdifi_mux_in = AUDIO_SPDIFI_SRC_HDMI;
		else if (!strcmp(mux_in, "LOOPBACK"))
			dpcm->spdifi_mux_in = AUDIO_SPDIFI_SRC_LOOPBACK;
		else
			dpcm->spdifi_mux_in = AUDIO_SPDIFI_SRC_DISABLE;
		break;
	case AUDIO_IPT_SRC_I2S_PRI_CH12:
	case AUDIO_IPT_SRC_I2S_PRI_CH34:
	case AUDIO_IPT_SRC_I2S_PRI_CH56:
	case AUDIO_IPT_SRC_I2S_PRI_CH78:
		if (!strcmp(mux_in, "IN"))
			dpcm->i2si_mux_in = AUDIO_I2SI_SRC_IN;
		else if (!strcmp(mux_in, "HDMI"))
			dpcm->i2si_mux_in = AUDIO_I2SI_SRC_HDMI;
		else if (!strcmp(mux_in, "LOOPBACK"))
			dpcm->i2si_mux_in = AUDIO_I2SI_SRC_LOOPBACK;
		else
			dpcm->i2si_mux_in = AUDIO_I2SI_SRC_DISABLE;
		break;
	default:
		ALSA_DEBUG_INFO("[ALSA] unknown mux_in\n");
		break;
	}
}
#endif

static void get_aio_config_src(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

#ifndef CONFIG_CUSTOMER_TV006
	char *token, *path_src;

	ALSA_DEBUG_INFO("[ALSA] path_src : %s\n", runtime->path_src);

    if(substream->pcm->device == 0){
	strcpy(runtime->path_src, "I2S_PRI_CH12,IN");
    }
	else if(substream->pcm->device == 1){
	    strcpy(runtime->path_src, "I2S_PRI_CH12,LOOPBACK");
    }
	path_src = (char *) runtime->path_src;

	token = strsep(&path_src, ",");
	ALSA_DEBUG_INFO("[ALSA] token[0] : %s\n", token);

	if (!strcmp(token, "BBADC"))
		dpcm->path_src = AUDIO_IPT_SRC_BBADC;
	else if (!strcmp(token, "ATV"))
		dpcm->path_src = AUDIO_IPT_SRC_ATV;
	else if (!strcmp(token, "SPDIFI"))
		dpcm->path_src = AUDIO_IPT_SRC_SPDIF;
	else if (!strcmp(token, "I2S_PRI_CH12"))
		dpcm->path_src = AUDIO_IPT_SRC_I2S_PRI_CH12;
	else if (!strcmp(token, "I2S_PRI_CH34"))
		dpcm->path_src = AUDIO_IPT_SRC_I2S_PRI_CH34;
	else if (!strcmp(token, "I2S_PRI_CH56"))
		dpcm->path_src = AUDIO_IPT_SRC_I2S_PRI_CH56;
	else if (!strcmp(token, "I2S_PRI_CH78"))
		dpcm->path_src = AUDIO_IPT_SRC_I2S_PRI_CH78;
	else
		dpcm->path_src = AUDIO_IPT_SRC_UNKNOWN;

	token = strsep(&path_src, ",");
	ALSA_DEBUG_INFO("[ALSA] token[1] : %s\n", token);
	get_mux_in(token, substream);

#else
	dpcm->path_src = AUDIO_IPT_SRC_BBADC;
	/*dpcm->bbadc_mux_in = AUDIO_BBADC_SRC_DMIC;*/
#endif
}

static int rtk_snd_ai_switch_focus(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res, *res_audio;
	int instanceID;

	get_aio_config_src(substream);

	ALSA_DEBUG_INFO("[ALSA] path_src: %x\n", dpcm->path_src);

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] [%d] alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *) ((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((ulong) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));

	instanceID = ((dpcm->ai_cap_agent&0xFFFFFFF0)|(dpcm->ai_cap_pin&0xF));
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_AI_SWITCH_FOCUS);
	cmd->privateInfo[0] = htonl(dpcm->path_src);
	cmd->privateInfo[1] = htonl(AUDIO_IPT_SRC_UNKNOWN);
	cmd->privateInfo[2] = htonl(AUDIO_IPT_SRC_UNKNOWN);
	cmd->privateInfo[3] = htonl(AUDIO_IPT_SRC_UNKNOWN);

	switch (dpcm->path_src) {
	case AUDIO_IPT_SRC_BBADC:
		ALSA_DEBUG_INFO("[ALSA] bbadc_mux_in: %x\n", dpcm->bbadc_mux_in);
		cmd->privateInfo[4] = htonl(dpcm->bbadc_mux_in);
	break;
	case AUDIO_IPT_SRC_SPDIF:
		ALSA_DEBUG_INFO("[ALSA] spdifi_mux_in: %x\n", dpcm->spdifi_mux_in);
		cmd->privateInfo[4] = htonl(dpcm->spdifi_mux_in);
		break;
	case AUDIO_IPT_SRC_I2S_PRI_CH12:
	case AUDIO_IPT_SRC_I2S_PRI_CH34:
	case AUDIO_IPT_SRC_I2S_PRI_CH56:
	case AUDIO_IPT_SRC_I2S_PRI_CH78:
	case AUDIO_IPT_SRC_ATV:
		cmd->privateInfo[4] = htonl(AUDIO_IPT_SRC_UNKNOWN);
		break;
	default:
		ALSA_DEBUG_INFO("[ALSA] unknown mux_in\n");
		cmd->privateInfo[4] = htonl(AUDIO_IPT_SRC_UNKNOWN);
		break;
	}

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long) cmd_audio,
		(unsigned long) res_audio,
		(unsigned long) res, &ret)) {
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return 0;
}

static int snd_realtek_ai_hw_resume(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	unsigned int temp;

	temp = ((dpcm->ai_cap_agent&0xFFFFFFF0)|(dpcm->ai_cap_pin&0xF));

	if (RPC_TOAGENT_RUN_SVC(dpcm->card->card, &temp) < 0)
		return -1;

	return 0;
}

static int rtk_snd_check_audio_fw_capability(struct snd_card *card)
{
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res , *res_audio;

	ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);


	card->card_dev.dma_mask = &card->card_dev.coherent_dma_mask;
	if(dma_set_mask(&card->card_dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(&card->card_dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(&card->card_dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}
	else
	{
		ALSA_DEBUG_INFO("[ALSA]%d OK to alloc memory\n", __LINE__);
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *) ((unsigned long) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		(((((unsigned long) dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) &
			ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	/********************************************
	*   privateInfo[0]: Magic Number               *
	*   privateInfo[1]: for AI                     *
	*   [2] ~ [15]	  : TBD			    *
	********************************************/
	/********************************************
	*	privateInfo[1]:                        *
	*		bit 0 -> check global AI    *
	********************************************/
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_FIRMWARE_CAPABILITY);

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long)cmd_audio,
		(unsigned long)res_audio,
		(unsigned long)res, &ret)) {
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		dma_free_coherent(&card->card_dev, 4096, p, dat);
		return -1;
	}

	ALSA_DEBUG_INFO("[ALSA] privateInfo[0] 0x%x, privateInfo[1] 0x%x\n",
		ntohl(res->privateInfo[0]),
		ntohl(res->privateInfo[1]));

	if (ntohl(res->privateInfo[0]) == 0x23792379) {
		have_global_ai = ntohl(res->privateInfo[1]) & 0x00000001;
		ALSA_DEBUG_INFO("[ALSA] support capability RPC!!\n");
	} else {
		have_global_ai = 0;
		ALSA_DEBUG_INFO("[ALSA] doesn't support capability RPC\n");
	}

	dma_free_coherent(&card->card_dev, 4096, p, dat);
	return 0;
}

/* AO capture get available AO out pin */
static int rpc_toagent_get_ao_outpin(struct snd_pcm_substream *substream)
{
	unsigned long ret;
	dma_addr_t dat;
	void *p;
	AUDIO_RPC_PRIVATEINFO_PARAMETERS *cmd, *cmd_audio;
	AUDIO_RPC_PRIVATEINFO_RETURNVAL *res , *res_audio;
	int instanceID, pin_id;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	dpcm->card->card->dev->dma_mask = &dpcm->card->card->dev->coherent_dma_mask;
	if(dma_set_mask(dpcm->card->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(dpcm->card->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(dpcm->card->card->dev, 4096, &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] [%d]alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, 4096);

	cmd = p;
	res = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((unsigned long)p + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	cmd_audio = (void *)(AUDIO_RPC_PRIVATEINFO_PARAMETERS *)((ulong) dat|0xa0000000);
	res_audio = (void *)(AUDIO_RPC_PRIVATEINFO_RETURNVAL *)
		((((ulong)(dat|0xa0000000) + sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS) + 8) & ALIGN4));

	memset(cmd, 0, sizeof(AUDIO_RPC_PRIVATEINFO_PARAMETERS));
	instanceID = dpcm->ao_cap_agent;
	cmd->instanceID = htonl(instanceID);
	cmd->type = htonl(ENUM_PRIVATEINFO_AUDIO_GET_AVAILABLE_AO_OUTPUT_PIN);
	cmd->privateInfo[0] = 0xFF;
	cmd->privateInfo[1] = 0xFF;
	cmd->privateInfo[2] = 0xFF;
	cmd->privateInfo[3] = 0xFF;
	cmd->privateInfo[4] = 0xFF;
	cmd->privateInfo[5] = 0xFF;

	if (audio_send_rpc_command(RPC_AUDIO,
		ENUM_KERNEL_RPC_PRIVATEINFO,
		(unsigned long)cmd_audio,
		(unsigned long)res_audio,
		(unsigned long)res, &ret)) {
		ALSA_ERROR("[ALSA] %d RPC fail\n", __LINE__);
		dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
		return -1;
	}

	pin_id = ntohl(res->privateInfo[0]);
	dpcm->ao_cap_pin = pin_id;
	ao_cap_pin = dpcm->ao_cap_pin;
	ALSA_ERROR("[ALSA] %d dpcm->ao_cap_pin(%x)\n", __LINE__, dpcm->ao_cap_pin);
	dma_free_coherent(dpcm->card->card->dev, 4096, p, dat);
	return 0;
}

static int rtk_snd_capture_prepare_BE(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;


	if (dpcm->cap_format == AUDIO_ALSA_FORMAT_24BITS_BE_PCM) {
        dpcm->input_frame_bytes = 4 * runtime->channels;
        dpcm->input_sample_bytes = 4;
	} else if (dpcm->cap_format == AUDIO_ALSA_FORMAT_16BITS_BE_PCM) {
		dpcm->input_frame_bytes  = frames_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);
	}

    if((!strcmp(dpcm->card->card->longname, "Mars 1")))
    {
        if(substream->pcm->device == CAPTURE_PCM)
        {
            //To Do: AI using only?
            //dpcm->ai_cap_pin = ASLA_OUTPIN;
            //pr_info("[ALSA] prepare open AO capture\n");
            /* get available ao outpin */
            rpc_toagent_get_ao_outpin(substream);

            switch(g_capture_dev)
            {
                case ENUM_DEVICE_BLUETOOTH:
                    rtk_snd_set_ao_capture_volume(g_capture_dev_vol[1]);
                    break;
                case ENUM_DEVICE_WISA:
                    rtk_snd_set_ao_capture_volume(g_capture_dev_vol[2]);
                    break;
                case ENUM_DEVICE_SE_BT:
                    rtk_snd_set_ao_capture_volume(g_capture_dev_vol[3]);
                    break;
                default:
                    rtk_snd_set_ao_capture_volume(g_capture_dev_vol[0]);
                    rtk_snd_set_ao_capture_mute(g_capture_dev_mute[0]);
                    break;
            }

            //pr_info("[ALSA] %s, %d prepare open AO capture\n", __FUNCTION__, __LINE__);
            /* send rpc init ao capture ring */
            if (rtk_snd_init_ao_capture_ring(substream)){
                ALSA_ERROR("[ALSA] %d rtk_snd_init_ao_capture_ring fail\n", __LINE__);
                goto prepare_fail;
            }

            if(rtk_snd_set_ao_config(substream) != S_OK)
                ALSA_ERROR("[ALSA] %d rtk_snd_set_ao_config fail\n", __LINE__);

            ALSA_INFO("[ALSA] %s prepare open AO capture done!\n", __FUNCTION__);
            return 0;
        }
        else if(substream->pcm->device == CAPTURE_ES)
        {
        	//do nothing
        	return 0;
        }
        else
        {
            dpcm->ai_cap_pin = TS_OUTPIN;

			pr_info("[ALSA] prepare open AI capture\n");
			/* send rpc init ai ring */
			if (rtk_snd_init_ai_ring(substream)){
				ALSA_ERROR("[ALSA] %d fail\n", __LINE__);
				goto prepare_fail;
			}
			/* send rpc switch focus to AI capture */
			if (rtk_snd_ai_switch_focus(substream)){
				ALSA_ERROR("[ALSA] %d fail\n", __LINE__);
				goto prepare_fail;
			}
			/* send rpc run AI */
			if (snd_realtek_ai_hw_resume(substream) < 0){
				ALSA_ERROR("[ALSA] %d fail\n", __LINE__);
				goto prepare_fail;
			}

			return 0;
        }
    }

prepare_fail:
	return -ENOMEM;
}

static int rtk_snd_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	unsigned int bps;

	if (dpcm->ring_init == 1) {
		if (dpcm->periods != runtime->periods) {
			ALSA_ERROR("[ALSA] periods different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->period_size != runtime->period_size) {
			ALSA_ERROR("[ALSA] period_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->buffer_size != runtime->buffer_size) {
			ALSA_ERROR("[ALSA] buffer_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->access != runtime->access) {
			ALSA_ERROR("[ALSA] access different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->format != runtime->format) {
			ALSA_ERROR("[ALSA] format different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->channels != runtime->channels) {
			ALSA_ERROR("[ALSA] channels different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->ring_init == 0){
            if(!strcmp(dpcm->card->card->longname, "Mars 1"))
            {
                if(substream->pcm->device == CAPTURE_PCM)
                {
                    snd_realtek_ao_capture_hw_close(substream);
                }
                else
                {
                    snd_realtek_ai_hw_close(substream);
                }
             }
		}
	}

	/* set ALSA capture parameters */
	bps = runtime->rate * runtime->channels;
	bps *= snd_pcm_format_width(runtime->format);
	bps /= 8;
	if (bps <= 0)
		return -EINVAL;

	dpcm->pcm_bps = bps;
	dpcm->pcm_jiffie = runtime->rate / HZ;
	dpcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	dpcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	dpcm->pcm_irq_pos = 0;
	dpcm->appl_ptr = 0;
	dpcm->flush_state = RTK_SND_FLUSH_STATE_NONE;
	dpcm->periods = runtime->periods;
	dpcm->buffer_size = runtime->buffer_size;
	dpcm->access = runtime->access;
	dpcm->channels = runtime->channels;
	dpcm->format = runtime->format;
	dpcm->period_size = runtime->period_size;
	dpcm->sample_bits = runtime->sample_bits;
	dpcm->rate = runtime->rate;
	dpcm->empty_timeout = jiffies;
	dpcm->sample_jiffies = runtime->rate / 10000;
	dpcm->remain_sample = 0;
	//Currently the interleave/non-interleave control by RTK
	dpcm->interleave = 1;//For seetv using
	//dpcm->interleave = 0;//For cips before k6

	if (dpcm->sample_jiffies == 0)
		dpcm->sample_jiffies = 1;

	dpcm->period_jiffies =
		runtime->period_size * 100 / runtime->rate * 1 / 20;
	if (dpcm->period_jiffies == 0)
		dpcm->period_jiffies = 1;

	ALSA_DEBUG_INFO("[ALSA] period_jiffies = %d\n", dpcm->period_jiffies);

	/* check format */
	switch (runtime->access) {
	case SNDRV_PCM_ACCESS_MMAP_INTERLEAVED:
	case SNDRV_PCM_ACCESS_RW_INTERLEAVED:
		switch (runtime->format) {
		case SNDRV_PCM_FORMAT_S16_LE:
			dpcm->cap_format = AUDIO_ALSA_FORMAT_16BITS_LE_LPCM;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			ALSA_DEBUG_INFO("[ALSA] write .wav header err\n");
			dpcm->cap_format = AUDIO_ALSA_FORMAT_24BITS_LE_LPCM;
			break;
		default:
			ALSA_ERROR("[ALSA] unsupport format %d\n", __LINE__);
			return -1;
		}
		break;
	case SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED:
	case SNDRV_PCM_ACCESS_RW_NONINTERLEAVED:
	default:
		ALSA_ERROR("[ALSA] unsupport access %d\n", __LINE__);
		return -1;
	}

    if(!strcmp(dpcm->card->card->longname, "Mars 1"))//snd card 0
    {
        if(substream->pcm->device == CAPTURE_PCM)//c0d10 for wisa/bt
        {
            /* 24bit BE non-interleave from ASLA_OUTPIN */
            //dpcm->cap_format = AUDIO_ALSA_FORMAT_24BITS_BE_PCM;
            dpcm->cap_format = AUDIO_ALSA_FORMAT_16BITS_BE_PCM;
        }
        else
        {
            /* 16bit BE interleave from TS_OUTPIN */
            dpcm->cap_format = AUDIO_ALSA_FORMAT_16BITS_BE_PCM;
        }
    }

	if (dpcm->ring_init == 1)
		return 0;

	dpcm->ring_init = 1;

	pr_info("[ALSA] start_threshold = %lx\n", runtime->start_threshold);
	pr_info("[ALSA] stop_threshold = %lx\n", runtime->stop_threshold);
	pr_info("[ALSA] avail_min = %lx\n", runtime->control->avail_min);
	pr_info("[ALSA] buffer_size = %lx\n", runtime->buffer_size);
	pr_info("[ALSA] period_size = %lx\n", runtime->period_size);
	pr_info("[ALSA] periods = %lx\n", runtime->periods);
	pr_info("[ALSA] sample rate = %d\n", runtime->rate);
	pr_info("[ALSA] frames_to_bytes = %d\n", frames_to_bytes(runtime, 1));

	/*
	substream->ops->silence = NULL;
	substream->ops->copy = NULL;
	*/

	/* prepare according to format */
	switch (dpcm->cap_format) {
	case AUDIO_ALSA_FORMAT_24BITS_BE_PCM:
	case AUDIO_ALSA_FORMAT_16BITS_BE_PCM:
		if (rtk_snd_capture_prepare_BE(substream)) {
			ALSA_ERROR("[ALSA] %d fail\n", __LINE__);
			return -ENOMEM;
		}
		break;
	case AUDIO_ALSA_FORMAT_24BITS_LE_LPCM:
	case AUDIO_ALSA_FORMAT_16BITS_LE_LPCM:
		ALSA_ERROR("[ALSA] %d not support\n", __LINE__);
		break;
	default:
		ALSA_ERROR("[ALSA] %d unknown format\n", __LINE__);
		break;
	}

	return 0;
}

static void rtk_snd_fmt_convert_to_S16LE(struct snd_pcm_substream *substream,
	snd_pcm_uframes_t wp_next, snd_pcm_uframes_t wp, unsigned int adv_min)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	int i, loop;
	char *p = NULL, temp;

	p = (char *) dpcm->ring_bak[0].writePtr;

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_BE:
		if (wp_next > wp || wp_next == 0) {
			for (i = 0; i < adv_min * runtime->channels; i++) {
				temp = *p;
				*p = *(p+1);
				p++;
				*p = temp;
				p++;
			}
		} else {
			loop = runtime->buffer_size - wp;
			for (i = 0; i < loop * runtime->channels; i++) {
				temp = *p;
				*p = *(p+1);
				p++;
				*p = temp;
				p++;
			}
			p = (char *) dpcm->ring_bak[0].beginAddr;
			for (i = 0; i < wp_next * runtime->channels; i++) {
				temp = *p;
				*p = *(p+1);
				p++;
				*p = temp;
				p++;
			}
		}
	break;
	case SNDRV_PCM_FORMAT_S16_LE:
	break;
	default:
		ALSA_ERROR("[ALSA] not support format convert\n");
	break;
	}
}

static void rtk_snd_pcm_freerun_timer_function(unsigned long data)
{
#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] [%s %d]Enter\n", __func__, __LINE__);
#endif

	struct rtk_snd_pcm *dpcm = (struct rtk_snd_pcm *)data;
	struct snd_pcm_substream *substream = dpcm->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int rp;
	unsigned int rp_base, adv_min, rp_real, avail;
	snd_pcm_uframes_t wp, wp_next;

	/* for ALSA data throughput */
	struct timeval t;
	long per_write_count;
	long valid;
	long time_diff, nodata_time_diff;
	int hwptr_diff;  /* for error handle */

	spin_lock(dpcm->pcm_lock);

	rp_base = dpcm->ring_bak[0].beginAddr & 0x1FFFFFFF;
	rp_real = ntohl(dpcm->ring[0].readPtr[0]) & 0x1FFFFFFF;
#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] rp_real = %lx, rp_base = %lx\n", rp_real, rp_base);
#endif
	if ((dpcm->audiopath == AUDIO_PATH_DECODER_AO) ||
	(dpcm->audiopath == AUDIO_PATH_AO)) {
		rp = (rp_real - rp_base) / dpcm->output_frame_bytes;
#ifdef ALSA_DEBUG_LOG
		ALSA_DEBUG_INFO("[ALSA] rp = %ld\n", rp);
#endif
		if (runtime->buffer_size !=
			(dpcm->ring_bak[0].size / dpcm->output_frame_bytes)) {
			ALSA_ERROR("[ALSA] buffer_size error\n");
		}
	} else {
		rp = (rp_real - rp_base) / dpcm->output_sample_bytes;
		if (runtime->buffer_size !=
			(dpcm->ring_bak[0].size / dpcm->output_sample_bytes)) {
			ALSA_ERROR("[ALSA] buffer_size error\n");
		}
	}

	if (rp < (dpcm->pcm_buf_pos % runtime->buffer_size)) {
		adv_min = rp + runtime->buffer_size -
				(dpcm->pcm_buf_pos%runtime->buffer_size);
	} else {
		adv_min = rp - (dpcm->pcm_buf_pos%runtime->buffer_size);
	}

	if (dpcm->pcm_buf_pos > runtime->control->appl_ptr)
		avail = runtime->control->appl_ptr +
			runtime->boundary -
			dpcm->pcm_buf_pos;
	else
		avail = runtime->control->appl_ptr - dpcm->pcm_buf_pos;

#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] avail = %d, adv_min = %d\n", avail, adv_min);
#endif

	/* prevent underflow condition occur */
	if (runtime->status->state != SNDRV_PCM_STATE_DRAINING ||
		dpcm->freerun) {
		if (avail == adv_min && adv_min >= 16)
			adv_min = avail - 16;
	}

	dpcm->pcm_buf_pos += adv_min;
	dpcm->pcm_buf_pos %= runtime->boundary;
	if (dpcm->remain_sample < adv_min)
		dpcm->remain_sample = 0;
	else
		dpcm->remain_sample = dpcm->remain_sample - adv_min;

	/* calculate the avail size */
	if (dpcm->freerun == 0) {
		if (dpcm->appl_ptr == runtime->control->appl_ptr)
			goto check_status;

		wp = dpcm->appl_ptr % runtime->buffer_size;

		if (dpcm->pcm_buf_pos > runtime->control->appl_ptr) {
			avail = runtime->control->appl_ptr +
				runtime->boundary -
				dpcm->pcm_buf_pos;
		} else {
			avail = runtime->control->appl_ptr -
			dpcm->pcm_buf_pos;
		}

#ifdef ALSA_DEBUG_LOG
		ALSA_DEBUG_INFO("[ALSA] avail = %d\n", avail);
#endif

		/* buffer full */
		if (avail == runtime->buffer_size) {
			if (runtime->control->appl_ptr < 16) {
				dpcm->appl_ptr =
					runtime->control->appl_ptr +
					runtime->boundary - 16;
			} else {
				dpcm->appl_ptr =
					runtime->control->appl_ptr - 16;
			}
		} else {
			dpcm->appl_ptr = runtime->control->appl_ptr;
		}

		dpcm->appl_ptr %= runtime->boundary;
		wp_next = dpcm->appl_ptr % runtime->buffer_size;

		if (wp_next != wp) {
			if (wp_next > wp) {
				dpcm->remain_sample += (wp_next - wp);
				adv_min = (wp_next - wp);
			} else {
				dpcm->remain_sample +=
					(wp_next + runtime->buffer_size - wp);
				adv_min =
					(wp_next + runtime->buffer_size - wp);
			}
			dpcm->empty_timeout =
				dpcm->remain_sample * 100 /
				runtime->rate + jiffies;
		} else {
			adv_min = 0;
			goto check_status;
		}

		rtk_snd_fmt_convert_to_S16LE(substream, wp_next, wp, adv_min);

		if ((dpcm->audiopath == AUDIO_PATH_DECODER_AO) ||
			(dpcm->audiopath == AUDIO_PATH_AO)) {
			if (wp_next > wp || wp_next == 0) {
				dma_addr_t phy_addr =
					dma_map_single(dpcm->card->card->dev,
					(phys_to_virt(dpcm->ring_bak[0].writePtr&
					0x1FFFFFFF)),
					adv_min * dpcm->output_frame_bytes,
					DMA_TO_DEVICE);
				dma_unmap_single(dpcm->card->card->dev, phy_addr,
				(long) (phys_to_virt(dpcm->ring_bak[0].writePtr&
				0x1FFFFFFF)),
				DMA_TO_DEVICE);
			} else {
				dma_addr_t phy_addr =
					dma_map_single(dpcm->card->card->dev,
					(phys_to_virt(dpcm->ring_bak[0].writePtr&
					0x1FFFFFFF)),
					(runtime->buffer_size - wp) *
					dpcm->output_frame_bytes,
					DMA_TO_DEVICE);
				dma_unmap_single(dpcm->card->card->dev, phy_addr,
				(long) (phys_to_virt(dpcm->ring_bak[0].writePtr&
				0x1FFFFFFF)),
				DMA_TO_DEVICE);

				if (wp_next) {
					dma_addr_t phy_addr1 =
						dma_map_single(dpcm->card->card->dev,
						(phys_to_virt(
						dpcm->ring_bak[0].beginAddr&
						0x1FFFFFFF)),
						(wp_next) *
						dpcm->output_frame_bytes,
						DMA_TO_DEVICE);
					dma_unmap_single(dpcm->card->card->dev, phy_addr1,
					(long) (phys_to_virt(
					dpcm->ring_bak[0].beginAddr&
					0x1FFFFFFF)),
					DMA_TO_DEVICE);
				}
			}
			dpcm->ring_bak[0].writePtr =
				dpcm->ring_bak[0].beginAddr +
				(dpcm->appl_ptr % runtime->buffer_size) *
				dpcm->output_frame_bytes;
			dpcm->ring[0].writePtr =
				htonl(0xa0000000|
				(dpcm->ring_bak[0].writePtr&
				0x1FFFFFFF));
		} else {
			/* not support AUDIO_PATH_AO_BYPASS */
		}
	} else {
		/* not support freerun */
	}

check_status:
	/* throughput compute */
	per_write_count = (long)((ntohl(dpcm->ring[0].writePtr) & 0x1FFFFFFF) -
						dpcm->pre_wr_ptr);

	if (per_write_count < 0)
		per_write_count += (long)dpcm->buffer_byte_size;

	dpcm->total_data_wb = dpcm->total_data_wb + per_write_count;

	valid = (long)(ntohl(dpcm->ring[0].writePtr) & 0x1FFFFFFF) -
			((ntohl(dpcm->ring[0].readPtr[0]) & 0x1FFFFFFF));
	if (valid < 0)
		valid += dpcm->buffer_byte_size;

	if (dpcm->max_level < valid)
		dpcm->max_level = valid;

	if (dpcm->min_level > valid)
		dpcm->min_level = valid;

	dpcm->pre_wr_ptr = ntohl(dpcm->ring[0].writePtr) & 0x1FFFFFFF;

#ifndef CONFIG_CUSTOMER_TV006
	update_hw_delay(substream);
#endif

	do_gettimeofday(&t);
	dpcm->current_time_ms = t.tv_sec*1000 + t.tv_usec/1000;

	time_diff = dpcm->current_time_ms - dpcm->pre_time_ms;

	if (valid == 0) {
		if (dpcm->pre_no_datatime_ms == 0) {
			dpcm->pre_no_datatime_ms = t.tv_sec*1000 + t.tv_usec/1000;
		}
	} else {
		if (dpcm->pre_no_datatime_ms != 0) {
			dpcm->current_no_datatime_time_ms = t.tv_sec*1000 + t.tv_usec/1000;
			nodata_time_diff = dpcm->current_no_datatime_time_ms - dpcm->pre_no_datatime_ms;

			if (nodata_time_diff > 30) {
				ALSA_DEBUG_INFO("[ALSA] shared buffer no data over than %ld ms\n", nodata_time_diff);
			}

			if (nodata_time_diff > 50) {
				ALSA_DEBUG_INFO("[ALSA] appl_ptr = %ld, appl_ptr = %ld\n",
					dpcm->appl_ptr,
					runtime->control->appl_ptr);
				ALSA_DEBUG_INFO("[ALSA] pcm_buf_pos %d, hw_ptr %ld\n",
					dpcm->pcm_buf_pos,
					runtime->status->hw_ptr);
			}
			dpcm->pre_no_datatime_ms = 0;
		}
	}

	if (time_diff >= 3000) {
		if (dpcm->max_level != 0) {
			ALSA_DEBUG_INFO("[ALSA] total_data_wb %ld, %ld~%ld bytes, time %ldms\n",
				dpcm->total_data_wb,
				dpcm->min_level,
				dpcm->max_level,
				time_diff);
		}

		ALSA_DEBUG_INFO("[ALSA] writePtr = %x, readPtr = %x\n",
			ntohl(dpcm->ring[0].writePtr),
			ntohl(dpcm->ring[0].readPtr[0]));
		ALSA_DEBUG_INFO("[ALSA] pcm_buf_pos %d, hw_ptr %ld, dma_addr %ld\n",
			dpcm->pcm_buf_pos,
			runtime->status->hw_ptr,
			(unsigned long) runtime->dma_addr);
		ALSA_DEBUG_INFO("[ALSA] appl_ptr = %ld, appl_ptr = %ld, tasklet_finish %d\n",
			dpcm->appl_ptr,
			runtime->control->appl_ptr,
			dpcm->elapsed_tasklet_finish);

		if (runtime->rate != 0) {
			ALSA_DEBUG_INFO("[ALSA] hw delay: %lu ms\n", runtime->delay * 1000 / runtime->rate);
		}

		do_gettimeofday(&t);
		dpcm->pre_time_ms = t.tv_sec*1000 + t.tv_usec/1000;
		dpcm->max_level = 0;
		dpcm->min_level = dpcm->buffer_byte_size;
		dpcm->total_data_wb = 0;
	}
	/* End throughput compute */

	hwptr_diff = dpcm->pcm_buf_pos - runtime->status->hw_ptr;
	if(hwptr_diff < 0) {
		hwptr_diff += runtime->boundary;
	}

	if(hwptr_diff >= runtime->buffer_size) {
		ALSA_DEBUG_INFO("[ALSA] reset hwptr from %d to hw_ptr %ld\n",
			dpcm->pcm_buf_pos, runtime->status->hw_ptr);

		dpcm->hwptr_error_times++;

		if(dpcm->hwptr_error_times >= 10) {
			runtime->status->hw_ptr = dpcm->pcm_buf_pos;
			dpcm->hwptr_error_times = 0;
		}
	}

	spin_unlock(dpcm->pcm_lock);
	if (dpcm->running == 0) {
		#ifdef CONFIG_SMP
			dpcm->timer->expires = dpcm->period_jiffies + jiffies;
			add_timer(dpcm->timer);
		#else
			dpcm->timer->expires = jiffies + 1;
			add_timer(dpcm->timer);
		#endif
		return;
	}

	if (runtime->status->state != SNDRV_PCM_STATE_DRAINING ||
		dpcm->freerun) {
		dpcm->pcm_irq_pos = dpcm->pcm_buf_pos - runtime->status->hw_ptr;
		if (dpcm->pcm_irq_pos < 0) {
			dpcm->pcm_irq_pos =
				dpcm->pcm_irq_pos + runtime->boundary;
		}

		if (dpcm->pcm_irq_pos >= runtime->period_size &&
			dpcm->elapsed_tasklet_finish != 0) {
			dpcm->elapsed_tasklet_finish = 0;
			tasklet_schedule(dpcm->elapsed_tasklet);
		}
	} else {
		switch (dpcm->flush_state) {
		case RTK_SND_FLUSH_STATE_NONE:
			ALSA_DEBUG_INFO("[ALSA] draining data start\n");
			dpcm->flush_state = RTK_SND_FLUSH_STATE_FINISH;
		case RTK_SND_FLUSH_STATE_WAIT:
			break;
		case RTK_SND_FLUSH_STATE_FINISH:
			if (dpcm->appl_ptr >= dpcm->pcm_buf_pos) {
				if ((dpcm->appl_ptr - dpcm->pcm_buf_pos) <= 256)
					dpcm->pcm_buf_pos = dpcm->appl_ptr;
			} else {
				if ((dpcm->appl_ptr - dpcm->pcm_buf_pos +
					runtime->boundary) <= 256)
					dpcm->pcm_buf_pos = dpcm->appl_ptr;
			}
			if (dpcm->pcm_buf_pos == dpcm->appl_ptr) {
				ALSA_DEBUG_INFO("[ALSA] draining data done\n");
				tasklet_schedule(dpcm->elapsed_tasklet);
			}
			break;
		}
	}

	dpcm->timer->expires = dpcm->period_jiffies + jiffies;
	add_timer(dpcm->timer);
}

//#ifdef USE_ASLA_OUTPIN
/* tranfer 24 bit BE to 16 bit LE */
/*#define COPY_FUNC(n_frame, p, ring_rp) \
{\
	int i, temp;\
	for (i = 0; i < n_frame; ++i) {\
		temp = (int)(*ring_rp[0]);\
		temp = ENDIAN_CHANGE(temp);\
		*p = (short)(temp >> 9);\
		p++;\
		temp = (int)(*ring_rp[1]);\
		temp = ENDIAN_CHANGE(temp);\
		*p = (short)(temp >> 9);\
		p++;\
		ring_rp[0]++;\
		ring_rp[1]++;\
	} \
}*/
//#else
/* tranfer 24 bit BE to 16 bit LE */
//void COPY_FUNC_24to16(int n_frame, short* p, long **ring_rp, int channels)
void COPY_FUNC_24to16(int n_frame, short* p, long **ring_rp, int channels)
{
    int i, j;
    long temp;
    for (i = 0; i < n_frame; i++) {
        for (j=0; j< channels; j++)
        {
            temp = (long)(*ring_rp[j]);
            temp = ENDIAN_CHANGE(temp);
            if(channels <= 2)
                *p = (short)(temp >> 9);//ch2: from data[]
            else
                *p = (short)(temp>>16);//ch3...8: from unrender pcm data
            p++;
            ring_rp[j]++;
        }
    }
}


/* tranfer 16 bit BE to 16 bit LE */
void COPY_FUNC_16BETo16LE(int n_sample, short* p, long *ring_rp, int channels)
{
	int i;
    //pr_info("[ALSA] %s, %d n_frame(%d) ring_rp=%x\n", __FUNCTION__, __LINE__, n_sample, ring_rp);
    short *tmp = (short *)(ring_rp);
    for (i = 0; i < (n_sample); i++) {
        *p = ENDIAN_CHANGE_16BITS(*tmp);
        tmp++;
        p++;
    }
}


#if 0
/* tranfer 16 bit BE to 16 bit LE */
#define COPY_FUNC(n_frame, p, ring_rp) \
{\
	int i, temp;\
	for (i = 0; i < n_frame; ++i) {\
		temp = (int)(*ring_rp[0]);\
		temp = ENDIAN_CHANGE(temp);\
		*p = (short)(temp&0x0000ffff);\
		p++;\
		*p = (short)((temp&0xffff0000)>>16);\
		p++;\
		ring_rp[0]++;\
	} \
}
#endif

//#endif

/* write zero to dma_area */
#define COPY_ZERO_FUNC(n_frame, p)\
{\
	int i;\
	for (i = 0; i < n_frame; ++i) {\
		*p = (short)(0);\
		p++;\
	} \
}

/*
copy ring buf to dma buf
substream: struct of the pcm stream
nPeriodCound: number of periods that write to runtime->dma_area
write_zero:
	if write_zero = 1,
		ALSA would write zero to dma_area.
	if write_zero = 0,
		ALSA would write n periods to dma_area from AI or AO.
*/
static void rtk_snd_capture_copy(struct snd_pcm_substream *substream,
	long n_period, int write_zero)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	snd_pcm_uframes_t n_frame = n_period * runtime->period_size;
	snd_pcm_uframes_t dma_wp = dpcm->pcm_buf_pos % runtime->buffer_size;
	snd_pcm_uframes_t wrap_size = runtime->buffer_size - dma_wp;
    long *ring_rp, *ring_rp_[8];
    long ring_limit;
	int i, loop0, loop1;
	short *p = NULL;

    if(dpcm->interleave)
    {
        if (write_zero == 0) {
            ring_rp = (long *)dpcm->ring_bak[0].readPtr[0];
            ring_limit = (long)(dpcm->ring_bak[0].beginAddr + dpcm->ring_bak[0].size);
        }

        if (n_frame > wrap_size)
        {
            p = (short *)(runtime->dma_area + dma_wp * frames_to_bytes(runtime, 1));
            if (write_zero == 0)
            {
                if ((long)(ring_rp + wrap_size * frames_to_bytes(runtime, 1)) > ring_limit)
                {
                    loop0 = (ring_limit - (long) ring_rp) / frames_to_bytes(runtime, 1);
                    loop1 = wrap_size - loop0;
                    COPY_FUNC_16BETo16LE(loop0 * runtime->channels, p, ring_rp, runtime->channels);

                    ring_rp = (long *)dpcm->ring_bak[0].beginAddr;
                    COPY_FUNC_16BETo16LE(loop1 * runtime->channels, p, ring_rp, runtime->channels);
                }
                else
                {
                    COPY_FUNC_16BETo16LE(wrap_size * runtime->channels, p, ring_rp, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }

            p = (short *)(runtime->dma_area);
            n_frame -= wrap_size;
            if (write_zero == 0)
            {
                if ((long)(ring_rp + n_frame * frames_to_bytes(runtime, 1)) > ring_limit)
                {
                    loop0 = (ring_limit - (long) ring_rp) / frames_to_bytes(runtime, 1);
                    loop1 = n_frame - loop0;
                    COPY_FUNC_16BETo16LE(loop0 * runtime->channels, p, ring_rp, runtime->channels);

                    ring_rp = (long *)dpcm->ring_bak[0].beginAddr;
                    COPY_FUNC_16BETo16LE(loop1 * runtime->channels, p, ring_rp, runtime->channels);
                }
                else
                {
                    COPY_FUNC_16BETo16LE(n_frame * runtime->channels, p, ring_rp, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }
        }
        else
        {
            p = (short *)(runtime->dma_area + dma_wp*frames_to_bytes(runtime, 1));
            if (write_zero == 0)
            {
                if ((long)(ring_rp + n_frame*frames_to_bytes(runtime, 1)) > ring_limit)
                {
                    loop0 = (ring_limit - (long) ring_rp) / (frames_to_bytes(runtime, 1));
                    loop1 = n_frame - loop0;
                    COPY_FUNC_16BETo16LE(loop0 * runtime->channels, p, ring_rp, runtime->channels);

                    ring_rp = (long *)dpcm->ring_bak[0].beginAddr;
                    COPY_FUNC_16BETo16LE(loop1 * runtime->channels, p, ring_rp, runtime->channels);
                }
                else
                {
                    COPY_FUNC_16BETo16LE(n_frame * runtime->channels, p, ring_rp, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }
        }
    }
    else//non-interleave
    {
        if (write_zero == 0) {
            for(i=0; i<runtime->channels; i++)
            {
                ring_rp_[i] = (long *)dpcm->ring_bak[i].readPtr[0];
            }
            //common limit
            ring_limit = (long)(dpcm->ring_bak[0].beginAddr + dpcm->ring_bak[0].size);
        }

        if (n_frame > wrap_size)
        {
            //p = (short *)(runtime->dma_area + dma_wp * runtime->channels);
            p = (short *)(runtime->dma_area + dma_wp*frames_to_bytes(runtime, 1));
            if (write_zero == 0)
            {
                if ((long)(ring_rp_[0] + wrap_size) > ring_limit)
                {
                    loop0 = (ring_limit - (long) ring_rp_[0])>>2;
                    loop1 = wrap_size - loop0;
                    COPY_FUNC_24to16(loop0, p, ring_rp_, runtime->channels);

                    p = (short *)(runtime->dma_area + (dma_wp+loop0)*frames_to_bytes(runtime, 1));
                    for(i=0; i<runtime->channels; i++)
                        ring_rp_[i] = (long *)dpcm->ring_bak[i].beginAddr;

                    COPY_FUNC_24to16(loop1, p, ring_rp_, runtime->channels);
                }
                else
                {
                    COPY_FUNC_24to16(wrap_size, p, ring_rp_, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }

            p = (short *)(runtime->dma_area);
            n_frame -= wrap_size;
            if (write_zero == 0)
            {
                if ((long)(ring_rp_[0] + n_frame) > ring_limit)
                {
                    loop0 = (ring_limit - (long) ring_rp_[0])>>2;
                    loop1 = n_frame - loop0;
                    COPY_FUNC_24to16(loop0, p, ring_rp_, runtime->channels);

                    p = (short *)(runtime->dma_area + loop0*frames_to_bytes(runtime, 1));
                    for(i=0; i<runtime->channels; i++)
                        ring_rp_[i] = (long *)dpcm->ring_bak[i].beginAddr;

                    COPY_FUNC_24to16(loop1, p, ring_rp_, runtime->channels);
                }
                else
                {
                    COPY_FUNC_24to16(n_frame, p, ring_rp_, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }
        }
        else
        {
            p = (short *)(runtime->dma_area + dma_wp*frames_to_bytes(runtime, 1));
            if (write_zero == 0)
            {
                if ((long)(ring_rp_[0] + n_frame) > ring_limit)
                {
                    loop0 = (ring_limit - (long)ring_rp_[0]) >> 2;
                    loop1 = n_frame - loop0;
                    COPY_FUNC_24to16(loop0, p, ring_rp_, runtime->channels);

                    p = (short *)(runtime->dma_area + (dma_wp+loop0)*frames_to_bytes(runtime, 1));
                    for(i=0; i<runtime->channels; i++)
                        ring_rp_[i] = (long *)dpcm->ring_bak[i].beginAddr;

                    COPY_FUNC_24to16(loop1, p, ring_rp_, runtime->channels);
                }
                else
                {
                    COPY_FUNC_24to16(n_frame, p, ring_rp_, runtime->channels);
                }
            }
            else
            {
                COPY_ZERO_FUNC(n_frame * runtime->channels, p);
            }
        }
    }
}

static long rtk_snd_capture_copy_es(struct snd_pcm_substream *substream,
	HAL_AUDIO_AENC_DATA_T *Msg)
{
	/*
	 * typedef struct {
	 *     unsigned long long pts;
	 *     unsigned char pData[AENC_BUF_SIZE_MAX(10 * 1024)];
	 *     unsigned int dataLen;
	 * } aenc_data_t;
	 */
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	int frame_format = frames_to_bytes(runtime, 1);
	snd_pcm_uframes_t dma_wp = (dpcm->pcm_buf_pos % runtime->buffer_size) * frame_format;
	snd_pcm_uframes_t wrap_size = (runtime->buffer_size* frame_format) - dma_wp;
	snd_pcm_uframes_t n_frame = sizeof(aenc_data_t);
	short *p = NULL;

	// if data length > AENC_BUF_SIZE_MAX, copy only AENC_BUF_SIZE_MAX
	if(Msg->dataLen > AENC_BUF_SIZE_MAX) {
		Msg->dataLen = AENC_BUF_SIZE_MAX;
	}

    // get current wp addr
    p = (short *)(runtime->dma_area + dma_wp);
    ALSA_DEBUG_INFO("[AENC] start p %p, n_frame: %lu, dma_wp %lu, wrap_size %lu\n(0)-0000 [AENC] pData %p, pRStart %p, pREnd %p, dataLen %d\nhw_ptr: %p, appl_ptr: %p\n",
    	(void*)p, n_frame, dma_wp, wrap_size,
    	Msg->pData, Msg->pRStart, Msg->pREnd, Msg->dataLen,
    	(void*)runtime->status->hw_ptr, (void*)runtime->control->appl_ptr);

	if (n_frame > wrap_size) {
		// calculate if wrap_size is enough to write all bs data
		if (wrap_size >= (Msg->dataLen+sizeof(unsigned long long))) {
			// if enough, write pts & pData field first
			unsigned int *dataLenPos;
			aenc_data_t* aenc_data = (aenc_data_t*)p;
			aenc_data->pts = Msg->pts;
			if((UINT32)Msg->pData + Msg->dataLen >= (UINT32)Msg->pREnd) {
				UINT32 part_size = ((UINT32)Msg->pREnd - (UINT32)Msg->pData);
				memcpy(aenc_data->pData, Msg->pData, part_size);
				memcpy((aenc_data->pData + part_size), Msg->pRStart, (Msg->dataLen-part_size));
			} else {
				memcpy(aenc_data->pData, Msg->pData, Msg->dataLen);
			}
			// calculate the position of dataLen field
			p = (short *)(runtime->dma_area + (n_frame - wrap_size - sizeof(unsigned int)));
			dataLenPos = (unsigned int *)p;
			*dataLenPos = Msg->dataLen;
		} else if (wrap_size == sizeof(unsigned int)) {
			// need to separate pts field (long long type) to high/low int
			unsigned int *ptshPos   = (unsigned int *)p;
			unsigned int *ptslPos   = (unsigned int *)runtime->dma_area;
			unsigned char *pDataPos = (unsigned char *)((unsigned char *)ptslPos + sizeof(unsigned int));
			unsigned int *dataLenPos   = (unsigned int *)(pDataPos + AENC_BUF_SIZE_MAX);
			*ptshPos = (unsigned int)((Msg->pts & 0xFFFFFFFF00000000LL) >> 32);
			*ptslPos = (unsigned int)(Msg->pts & 0xFFFFFFFFLL);
			*dataLenPos = Msg->dataLen;
			if((UINT32)Msg->pData + Msg->dataLen >= (UINT32)Msg->pREnd) {
				UINT32 part_size = ((UINT32)Msg->pREnd - (UINT32)Msg->pData);
				memcpy(pDataPos, Msg->pData, part_size);
				memcpy((pDataPos + part_size), Msg->pRStart, (Msg->dataLen-part_size));
			} else {
				memcpy(pDataPos, Msg->pData, Msg->dataLen);
			}
		} else if (wrap_size == sizeof(unsigned long long)) {
			unsigned long long *ptsPos = (unsigned long long *)p;
			unsigned char *pDataPos    = (unsigned char *)runtime->dma_area;
			unsigned int *dataLenPos   = (unsigned int *)(pDataPos + AENC_BUF_SIZE_MAX);
			*ptsPos = Msg->pts;
			*dataLenPos = Msg->dataLen;
			if((UINT32)Msg->pData + Msg->dataLen >= (UINT32)Msg->pREnd) {
				UINT32 part_size = ((UINT32)Msg->pREnd - (UINT32)Msg->pData);
				memcpy(pDataPos, Msg->pData, part_size);
				memcpy((pDataPos + part_size), Msg->pRStart, (Msg->dataLen-part_size));
			} else {
				memcpy(pDataPos, Msg->pData, Msg->dataLen);
			}
		} else {
			unsigned long long *ptsPos = (unsigned long long *)p;
			unsigned char *pDataPos1   = (unsigned char *)((unsigned char *)p + sizeof(unsigned long long));
			unsigned char *pDataPos2   = (unsigned char *)runtime->dma_area;
			unsigned int dataLen1      = (wrap_size - sizeof(unsigned long long));
			unsigned int dataLen2      = (AENC_BUF_SIZE_MAX - dataLen1);
			unsigned int *dataLenPos   = (unsigned int *)(pDataPos2 + dataLen2);
			*ptsPos = Msg->pts;
			*dataLenPos = Msg->dataLen;
			if((UINT32)Msg->pData + Msg->dataLen >= (UINT32)Msg->pREnd) {
				UINT32 part_size = ((UINT32)Msg->pREnd - (UINT32)Msg->pData);
				if (dataLen1 == part_size) {
					memcpy(pDataPos1, Msg->pData, part_size);
					memcpy(pDataPos2, Msg->pRStart, (Msg->dataLen-part_size));
				} else if (dataLen1 > part_size) {
					unsigned int buffer_diff = (dataLen1-part_size);
					memcpy(pDataPos1, Msg->pData, part_size);
					memcpy((pDataPos1+part_size), Msg->pRStart, buffer_diff);
					memcpy(pDataPos2, (Msg->pRStart+buffer_diff), (Msg->dataLen-dataLen1));
				} else {
					unsigned int buffer_diff = (part_size-dataLen1);
					memcpy(pDataPos1, Msg->pData, dataLen1);
					memcpy(pDataPos2, (Msg->pData+dataLen1), buffer_diff);
					memcpy((pDataPos2+buffer_diff), Msg->pRStart, (Msg->dataLen-dataLen1));
				}
			} else {
				memcpy(pDataPos1, Msg->pData, dataLen1);
				memcpy(pDataPos2, (Msg->pData+dataLen1), dataLen2);
			}
		}
	} else {
		aenc_data_t* aenc_data = (aenc_data_t*)p;
		aenc_data->pts = Msg->pts;
		aenc_data->dataLen = Msg->dataLen;
		if((UINT32)Msg->pData + Msg->dataLen >= (UINT32)Msg->pREnd) {
			UINT32 part_size = ((UINT32)Msg->pREnd - (UINT32)Msg->pData);
			memcpy(aenc_data->pData, Msg->pData, part_size);
			memcpy((aenc_data->pData + part_size), Msg->pRStart, (Msg->dataLen-part_size));
		} else {
			memcpy(aenc_data->pData, Msg->pData, Msg->dataLen);
		}
	}

	return Msg->dataLen;
}

static long ring_valid_data(long ring_base, long ring_limit,
	long ring_rp, long ring_wp)
{
	if (ring_wp >= ring_rp)
		return ring_wp-ring_rp;
	else
		return (ring_limit-ring_base)-(ring_rp-ring_wp);
}

static long rtk_snd_get_ring_data(RINGBUFFER_HEADER *p_ring_be,
	struct RBUF_HEADER_ARM *p_ring_le)
{
	long base, limit, rp, wp, data_size;
	base = p_ring_le->beginAddr;
	limit = p_ring_le->beginAddr + p_ring_le->size;
	wp = (long)(ntohl(p_ring_be->writePtr) & 0x1FFFFFFF);
	rp = (long)(ntohl(p_ring_be->readPtr[0]) & 0x1FFFFFFF);
	data_size = ring_valid_data(base, limit, rp, wp);
	return data_size;
}

//#define AENC_DUMP_BS
#ifdef AENC_DUMP_BS
static struct file * aenc_dump_file = NULL;
static struct file* aenc_file_open(const char* path, int flags, int rights)
{
	struct file* filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());

	filp = filp_open(path, flags, rights);
	set_fs(oldfs);

	if(IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}

	return filp;
}

static void aenc_file_close(struct file* file)
{
	filp_close(file, NULL);
}

static int aenc_file_write(struct file *file, unsigned char *data, uint size)
{
	mm_segment_t oldfs;
	int ret;
	loff_t pos;

	oldfs = get_fs();
	set_fs(get_ds());
	pos = file->f_pos;
	ret = vfs_write(file, data, size, &pos);
	file->f_pos = pos;
	set_fs(oldfs);

	return ret;
}
#endif

static void rtk_snd_pcm_capture_es_timer_function(unsigned long data)
{
	struct rtk_snd_cap_pcm *dpcm = (struct rtk_snd_cap_pcm *)data;
	struct snd_pcm_substream *substream = dpcm->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	long n_data_size = 0;
	struct timeval t;
	unsigned long avail_size;
	HAL_AUDIO_AENC_DATA_T Msg;

	spin_lock(dpcm->pcm_lock);
	// check if avail size is enough
	avail_size = snd_pcm_capture_hw_avail(runtime) * frames_to_bytes(runtime, 1);
	if (avail_size >= sizeof(aenc_data_t)) {
		// get es data from fw
		n_data_size = RHAL_AUDIO_Encoder_DataDeliver_loop(HAL_AUDIO_AENC0, &Msg);
		if (n_data_size > 0) {
			/* copy data from Msg to dma_area */
			n_data_size = rtk_snd_capture_copy_es(substream, &Msg);

			#ifdef AENC_DUMP_BS
			{
				if((UINT32)Msg.pData + Msg.dataLen >= (UINT32)Msg.pREnd) {
					UINT32 part_size = ((UINT32)Msg.pREnd - (UINT32)Msg.pData);
					aenc_file_write(aenc_dump_file, Msg.pData, part_size);
					aenc_file_write(aenc_dump_file, Msg.pRStart, (Msg.dataLen-part_size));
				} else {
					aenc_file_write(aenc_dump_file, Msg.pData, Msg.dataLen);
				}
			}
			#endif

			HAL_AUDIO_MEMOUT_ReleaseData(n_data_size);

			/* update rp, for AENC, it is always sizeof(aenc_data_t).
			   NOTE: it is FRAME size, not byte */
			dpcm->pcm_buf_pos += (sizeof(aenc_data_t)/frames_to_bytes(runtime, 1));

			dpcm->pcm_irq_pos = dpcm->pcm_buf_pos - runtime->status->hw_ptr;
			if (dpcm->pcm_irq_pos < 0) {
				dpcm->pcm_irq_pos =
					dpcm->pcm_irq_pos + runtime->boundary;
			}

			if (dpcm->pcm_irq_pos >= runtime->period_size &&
				dpcm->elapsed_tasklet_finish != 0) {
				dpcm->elapsed_tasklet_finish = 0;
				/* update ALSA runtime->hw_ptr */
				tasklet_schedule(dpcm->elapsed_tasklet);
			}

			do_gettimeofday(&t);
			dpcm->last_time_ms = t.tv_sec*1000 + t.tv_usec/1000;
		}
	}

	spin_unlock(dpcm->pcm_lock);

	dpcm->timer->expires = dpcm->period_jiffies + jiffies;
	add_timer(dpcm->timer);
}

/* 24BE AI 2ch input */
static void rtk_snd_pcm_capture_timer_function(unsigned long data)
{
	struct rtk_snd_cap_pcm *dpcm = (struct rtk_snd_cap_pcm *)data;
	struct snd_pcm_substream *substream = dpcm->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;

	snd_pcm_uframes_t n_data_frame;
	long n_data_size;
	unsigned int n_period = 0;
	int i;
	struct timeval t;
	unsigned long ring_limit;

	spin_lock(dpcm->pcm_lock);
	/* get AI outring data size */
	n_data_size = rtk_snd_get_ring_data(dpcm->ring, dpcm->ring_bak);
	if(dpcm->interleave == 1)
        n_data_frame = n_data_size / frames_to_bytes(runtime, 1);
    else
        n_data_frame = n_data_size / sizeof(long);

	if (n_data_frame >= runtime->period_size) {
		n_period = n_data_frame / runtime->period_size;
		if (n_period == runtime->periods)
			n_period--;
        //pr_info("[ALSA] %s, %d, (%d, %d, %d) n_period(%d), frames_to_bytes(%d)\n", __FUNCTION__, __LINE__, n_data_size, n_data_frame, runtime->period_size, n_period, frames_to_bytes(runtime, 1));

		/* copy data from ai ring to dma */
		if(sndout_cmd_info.capture_disable) {
			// write zero when capture_disable on
			rtk_snd_capture_copy(substream, n_period, 1);
		} else {
			rtk_snd_capture_copy(substream, n_period, 0);
		}

		/* update rp */
		dpcm->pcm_buf_pos += (n_period * runtime->period_size);

        if(dpcm->interleave == 1)//interleave
        {
            // only use 1 ch for interleave output
            for (i = 0; i < 1; i++) {
                dpcm->ring_bak[i].readPtr[0] =
                    dpcm->ring_bak[i].beginAddr + (dpcm->pcm_buf_pos % runtime->buffer_size) * frames_to_bytes(runtime, 1);
                dpcm->ring[i].readPtr[0] =
                    htonl(0xa0000000| ((ulong) dpcm->ring_bak[i].readPtr[0]&0x1FFFFFFF));
            }
            //pr_info("[ALSA] %s, %d (%d, %d)\n", __FUNCTION__, __LINE__, dpcm->ring_bak[i].readPtr[i], runtime->buffer_size);
        }
        else//non-interleave
        {
            for (i = 0; i < runtime->channels; i++) {

                ring_limit = dpcm->ring_bak[i].beginAddr + dpcm->ring_bak[i].size;
                if((dpcm->ring_bak[i].beginAddr + (dpcm->pcm_buf_pos % runtime->buffer_size)*sizeof(long)) > ring_limit)
                {
                    dpcm->ring_bak[i].readPtr[0] = (dpcm->ring_bak[i].beginAddr) + (dpcm->pcm_buf_pos % runtime->buffer_size)*sizeof(long) - ring_limit + dpcm->ring_bak[i].beginAddr;
                }
                else
                {
                    dpcm->ring_bak[i].readPtr[0] = dpcm->ring_bak[i].beginAddr + (dpcm->pcm_buf_pos % runtime->buffer_size)*sizeof(long);
                }

                dpcm->ring[i].readPtr[0] =
                    htonl(0xa0000000| ((ulong) dpcm->ring_bak[i].readPtr[0]&0x1FFFFFFF));

                //pr_info("[ALSA] %s, %d ring_rp[%d]=%x, ring_wp[%d]=%x, size(%d), (%x), runtime->buffer_size(%d)\n", __FUNCTION__, __LINE__, i, dpcm->ring_bak[i].readPtr[0], i, dpcm->ring_bak[i].writePtr, dpcm->ring_bak[i].size, ring_limit, runtime->buffer_size);
            }
        }

		/* update ALSA runtime->hw_ptr */
		tasklet_schedule(dpcm->elapsed_tasklet);

		do_gettimeofday(&t);
		dpcm->last_time_ms = t.tv_sec*1000 + t.tv_usec/1000;
	} else {
		do_gettimeofday(&t);
		dpcm->current_time_ms = t.tv_sec*1000 + t.tv_usec/1000;

		if ((dpcm->current_time_ms - dpcm->last_time_ms) >= 5000) {
			ALSA_DEBUG_INFO("[ALSA] no data in %ld millisecond\n",
				dpcm->current_time_ms - dpcm->last_time_ms);
			dpcm->last_time_ms = dpcm->current_time_ms;

			/* write zero data to dma_area
			when FW doesn't have data in 5 seconds */
			rtk_snd_capture_copy(substream, 1, 1);

			/* update rp */
			dpcm->pcm_buf_pos += runtime->period_size;
			ALSA_DEBUG_INFO("[ALSA] pcm_buf_pos = %x\n", dpcm->pcm_buf_pos);

			/* update ALSA runtime->status->hw_ptr */
			tasklet_schedule(dpcm->elapsed_tasklet);
		}
	}

	spin_unlock(dpcm->pcm_lock);

	dpcm->timer->expires = dpcm->period_jiffies + jiffies;
	add_timer(dpcm->timer);
}

static void snd_realtek_pcm_elapsed(unsigned long dpcm)
{
	snd_pcm_period_elapsed(((struct rtk_snd_pcm *)dpcm)->substream);
	((struct rtk_snd_pcm *)dpcm)->elapsed_tasklet_finish = 1;
}

static void snd_realtek_pcm_capture_elapsed(unsigned long dpcm)
{
	snd_pcm_period_elapsed(((struct rtk_snd_cap_pcm *)dpcm)->substream);
	((struct rtk_snd_cap_pcm *)dpcm)->elapsed_tasklet_finish = 1;
}

static snd_pcm_uframes_t rtk_snd_playback_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	snd_pcm_uframes_t pos;
	unsigned long flags;
	spin_lock_irqsave(dpcm->pcm_lock, flags);
	pos = (snd_pcm_uframes_t) dpcm->pcm_buf_pos % runtime->buffer_size;
	spin_unlock_irqrestore(dpcm->pcm_lock, flags);
	return pos;
}

static snd_pcm_uframes_t rtk_snd_capture_pointer(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;
	snd_pcm_uframes_t pos;
	unsigned long flags;
	spin_lock_irqsave(dpcm->pcm_lock, flags);
	pos = (snd_pcm_uframes_t) dpcm->pcm_buf_pos % runtime->buffer_size;
	spin_unlock_irqrestore(dpcm->pcm_lock, flags);
	return pos;
}

static struct snd_pcm_hardware rtk_snd_playback = {
	.info =	SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_NONINTERLEAVED |
			SNDRV_PCM_INFO_MMAP_VALID,
	.formats = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE),
	.rates = USE_RATE,
	.rate_min = USE_RATE_MIN,
	.rate_max =	USE_RATE_MAX,
	.channels_min =	1,
	.channels_max =	2,
	.buffer_bytes_max =	MAX_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 32,
};

static struct snd_pcm_hardware rtk_snd_capture = {
	.info =	SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_NONINTERLEAVED |
			SNDRV_PCM_INFO_MMAP_VALID,
	.formats = USE_FORMATS,
	.rates = SNDRV_PCM_RATE_48000,
	.rate_min =	48000,
	.rate_max =	48000,
	.channels_min =	USE_CHANNELS_MIN,
	.channels_max =	USE_CHANNELS_MAX,
	.buffer_bytes_max =	MAX_BUFFER_SIZE,
	.period_bytes_min =	MIN_PERIOD_SIZE,
	.period_bytes_max =	MAX_PERIOD_SIZE,
	.periods_min = USE_PERIODS_MIN,
	.periods_max = USE_PERIODS_MAX,
	.fifo_size = 32,
};

static void rtk_snd_runtime_free(struct snd_pcm_runtime *runtime)
{
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	struct snd_card *card;
	int i;
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	if (dpcm == NULL) {
		return;
	}

	card = dpcm->card->card;

	if (dpcm->pcm_lock != NULL) {
	kfree(dpcm->pcm_lock);
	}

	if (dpcm->elapsed_tasklet != NULL) {
	kfree(dpcm->elapsed_tasklet);
	}

	if (dpcm->timer != NULL) {
	kfree(dpcm->timer);
	}

	for (i = 0; i < runtime->channels; i++) {
		if (dpcm->ao_in_ring_p[i]) {
			if (dpcm->audiopath == AUDIO_PATH_DECODER_AO) {
				dvr_free(dpcm->ao_in_ring_p[i]);
			} else {
				ALSA_ERROR("[ALSA] dpcm->ao_in_ring_p[%d] %ld\n", i, (unsigned long)dpcm->ao_in_ring_p[i]);
			}
		}
	}

	dma_free_coherent(dpcm->card->card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);
	runtime->private_data = NULL;
}

static void snd_card_capture_runtime_free(struct snd_pcm_runtime *runtime)
{
	struct snd_card *card;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] [%s] %d\n", __func__, __LINE__);

	if (dpcm == NULL) {
		return;
	}

	card = dpcm->card->card;

	if (dpcm->pcm_lock != NULL) {
	kfree(dpcm->pcm_lock);
	}

	if (dpcm->elapsed_tasklet != NULL) {
	kfree(dpcm->elapsed_tasklet);
	}

	if (dpcm->timer != NULL) {
	kfree(dpcm->timer);
	}

	if (dpcm->ring_p)
		dma_free_coherent(card->dev, dpcm->buffer_byte_size,
			dpcm->ring_p, dpcm->ring_phy_addr);

	dma_free_coherent(card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);
	runtime->private_data = NULL;
}

static int rtk_snd_playback_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	int err = 0;
	unsigned int bits;
	unsigned int bps;
	int dev;
	struct timeval t;
	long ctms;
	long ptms;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	ALSA_DEBUG_INFO("[ALSA] [%s] size %x\n", __func__, params_buffer_bytes(hw_params));

	if (rtk_snd_open(substream) < 0)
		return -ENOMEM;

	err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));

	ALSA_DEBUG_INFO("[ALSA] acs %d, fmt %d, ch %d, rate %d\n",
		params_access(hw_params),
		params_format(hw_params),
		params_channels(hw_params),
		params_rate(hw_params));

	ALSA_DEBUG_INFO("[ALSA] ps %d, p %d, bs %d, bits %d\n",
		params_period_size(hw_params),
		params_periods(hw_params),
		params_buffer_size(hw_params),
		snd_pcm_format_physical_width(params_format(hw_params)));

	/* add for usbmic delay */
#if 1

	runtime->access = params_access(hw_params);
	runtime->format = params_format(hw_params);
	runtime->subformat = params_subformat(hw_params);
	runtime->channels = params_channels(hw_params);
	runtime->rate = params_rate(hw_params);
	runtime->period_size = params_period_size(hw_params);
	runtime->periods = params_periods(hw_params);
	runtime->buffer_size = params_buffer_size(hw_params);

	bits = snd_pcm_format_physical_width(runtime->format);
	runtime->sample_bits = bits;
	bits *= runtime->channels;
	runtime->frame_bits = bits;

	if (dpcm->ring_init == 1) {
		if (dpcm->periods != runtime->periods) {
			ALSA_ERROR("[ALSA] periods different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->period_size != runtime->period_size) {
			ALSA_ERROR("[ALSA] period_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->buffer_size != runtime->buffer_size) {
			ALSA_ERROR("[ALSA] buffer_size different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->access != runtime->access) {
			ALSA_ERROR("[ALSA] access different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->format != runtime->format) {
			ALSA_ERROR("[ALSA] format different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->channels != runtime->channels) {
			ALSA_ERROR("[ALSA] channels different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->ring_bak[0].beginAddr !=
			(unsigned long) runtime->dma_addr) {
			ALSA_ERROR("[ALSA] dma_addr different\n");
			dpcm->ring_init = 0;
		}
		if (dpcm->ring_init == 0) {
			if (PLAY_ES_DEVICE(substream->pcm->device)) {
				rtk_snd_es_stop(substream->pcm->device);
				rtk_snd_es_close(substream->pcm->device);
			} else {
				rtk_snd_flush(substream);
				rtk_snd_pause(substream);
				rtk_snd_close(substream);
			}
		}
	}

	bps = runtime->rate * runtime->channels;
	bps *= snd_pcm_format_width(runtime->format);
	bps /= 8;
	if (bps <= 0)
		return -EINVAL;

	dpcm->pcm_bps = bps;
	dpcm->pcm_jiffie = runtime->rate / HZ;
	dpcm->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	dpcm->pcm_count = snd_pcm_lib_period_bytes(substream);
	dpcm->pcm_buf_pos = 0;
	dpcm->pcm_irq_pos = 0;
	dpcm->appl_ptr = 0;
	dpcm->flush_state = RTK_SND_FLUSH_STATE_NONE;
	dpcm->periods = runtime->periods;
	dpcm->buffer_size = runtime->buffer_size;
	dpcm->access = runtime->access;
	dpcm->channels = runtime->channels;
	dpcm->format = runtime->format;
	dpcm->period_size = runtime->period_size;
	dpcm->sample_bits = runtime->sample_bits;
	dpcm->rate = runtime->rate;
	dpcm->empty_timeout = jiffies;
	dpcm->sample_jiffies = runtime->rate / 10000;
	dpcm->remain_sample = 0;
	if (dpcm->sample_jiffies == 0)
		dpcm->sample_jiffies = 1;

	dpcm->period_jiffies =
		runtime->period_size * 100 / runtime->rate * 1 / 20;
	if (dpcm->period_jiffies == 0)
		dpcm->period_jiffies = 1;

	dpcm->hwptr_error_times = 0;

#ifdef ALSA_DEBUG_LOG
	ALSA_DEBUG_INFO("[ALSA] dpcm->period_jiffies = %d\n", dpcm->period_jiffies);
#endif
	switch (runtime->access) {
		case SNDRV_PCM_ACCESS_MMAP_INTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] MMAP_INTERLEAVED\n");
#ifdef USE_DECODER
			/*old flow ALSA -> Decoder -> AO */
			dpcm->audiopath = AUDIO_PATH_DECODER_AO;
#else
			/* new flow ALSA -> AO */
			dpcm->audiopath = AUDIO_PATH_AO;
#endif
			break;
		case SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] MMAP_NONINTERLEAVED\n");
			dpcm->audiopath = AUDIO_PATH_AO_BYPASS;
			break;
		case SNDRV_PCM_ACCESS_RW_INTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] RW_INTERLEAVED\n");
#ifdef USE_DECODER
			/*old flow ALSA -> Decoder -> AO */
			dpcm->audiopath = AUDIO_PATH_DECODER_AO;
#else
			/* new flow ALSA -> AO */
			dpcm->audiopath = AUDIO_PATH_AO;
#endif
			break;
		case SNDRV_PCM_ACCESS_RW_NONINTERLEAVED:
			ALSA_DEBUG_INFO("[ALSA] RW_NONINTERLEAVED\n");
			dpcm->audiopath = AUDIO_PATH_AO_BYPASS;
			break;
		default:
			ALSA_DEBUG_INFO("[ALSA][%d] unsupport mode\n", __LINE__);
			return -1;
			break;
	}

	if (dpcm->audiopath == AUDIO_PATH_AO) {
		switch (runtime->format) {
#ifdef USE_DECODER
		case SNDRV_PCM_FORMAT_S32_BE:
		case SNDRV_PCM_FORMAT_S24_BE:
		case SNDRV_PCM_FORMAT_S24_LE:
		case SNDRV_PCM_FORMAT_S8:
#endif
		case SNDRV_PCM_FORMAT_S16_BE:
		case SNDRV_PCM_FORMAT_S16_LE:
			break;
		default:
			ALSA_DEBUG_INFO("[ALSA] unsupport format\n");
			return -1;
			break;
		}
	}

	if (dpcm->ring_init == 1) {
		ALSA_DEBUG_INFO("[ALSA] reprepare!!\n");
		spin_lock(dpcm->pcm_lock);
		dpcm->pcm_buf_pos = dpcm->appl_ptr;
		runtime->status->hw_ptr = runtime->control->appl_ptr;
		dpcm->ring_bak[0].writePtr = dpcm->ring_bak[0].beginAddr +
			(dpcm->appl_ptr % runtime->buffer_size) * dpcm->output_frame_bytes;
		dpcm->ring[0].writePtr = htonl(0xa0000000|
			(dpcm->ring_bak[0].writePtr & 0x1FFFFFFF));
		spin_unlock(dpcm->pcm_lock);
		if(PLAY_ES_DEVICE(substream->pcm->device)) {
			rtk_snd_es_resume(substream->pcm->device, (dpcm->ring_bak[0].writePtr));
		} else {
			rtk_snd_pause(substream);
			rtk_snd_flush(substream);
			rtk_snd_resume(substream);

			do_gettimeofday(&t);
			ptms = t.tv_sec*1000 + t.tv_usec/1000;

			/* wait ADSP finish flush */
			while (1) {
				if ((dpcm->ring_bak[0].writePtr & 0x1FFFFFFF) ==
						((ntohl(dpcm->ring[0].readPtr[0]) & 0x1FFFFFFF))) {
					ALSA_DEBUG_INFO("[ALSA] wp %lx, rp %x\n",
							dpcm->ring_bak[0].writePtr, ntohl(dpcm->ring[0].readPtr[0]));
					break;
				}

				do_gettimeofday(&t);
				ctms = t.tv_sec*1000 + t.tv_usec/1000;
				if ((ctms - ptms) >= 100) {
					/* wait timeout prevent deadlock */
					ALSA_ERROR("[ALSA] wait ADSP flush timeout!!!\n");
					break;
				}
			}
		}
		return 0;
	}

	dpcm->ring_init = 1;
	dev = substream->pcm->device;

	ALSA_DEBUG_INFO("[ALSA] freerun = %d\n", dpcm->freerun);
	ALSA_DEBUG_INFO("[ALSA] start_threshold = %lx\n", runtime->start_threshold);
	ALSA_DEBUG_INFO("[ALSA] stop_threshold = %lx\n", runtime->stop_threshold);
	ALSA_DEBUG_INFO("[ALSA] avail_min = %lx\n", runtime->control->avail_min);
	ALSA_DEBUG_INFO("[ALSA] buffer_size = %lx\n", runtime->buffer_size);
	ALSA_DEBUG_INFO("[ALSA] period_size = %lx\n", runtime->period_size);
	ALSA_DEBUG_INFO("[ALSA] sample rate = %d\n", runtime->rate);

	if (PLAY_ES_DEVICE(dev)) {
		ALSA_DEBUG_INFO("[ALSA] ES play\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id   = dpcm->ao_pin_id;

		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		if (rtk_snd_es_init_dec_ring(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}
		rtk_snd_es_resume(dev, (dpcm->ring_bak[0].writePtr));
	} else if (dpcm->audiopath == AUDIO_PATH_DECODER_AO) {
		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		rtk_snd_create_decoder_agent(substream);

		ALSA_DEBUG_INFO("[ALSA] dec_agent = %d, dec_pin_id = %d\n",
			dpcm->dec_agent, dpcm->dec_pin_id);

		rtk_snd_init_decoder_ring(substream);
		if (rtk_snd_init_connect_decoder_ao(substream)) {
			ALSA_DEBUG_INFO("[ALSA] %s %d\n", __func__, __LINE__);
			return -ENOMEM;
		}

		/*
		*	AO Pause
		*	Decoder Pause
		*	Decoder Flush
		*	Write Inband data
		*/
		rtk_snd_init_decoder_info(substream);
		rtk_snd_set_ao_flashpin_volume(substream);
		rtk_snd_resume(substream);

		/*
		substream->ops->silence = NULL;
		substream->ops->copy = snd_card_std_copy;
		*/

	} else if (dpcm->audiopath == AUDIO_PATH_AO_BYPASS) {

		ALSA_DEBUG_INFO("[ALSA] AO BYPASS\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id = dpcm->ao_pin_id;
		dpcm->extend_to_32be_ratio = 0;
		dpcm->output_frame_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = samples_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		/*
		substream->ops->copy = NULL;
		substream->ops->silence = NULL;
		*/

		if (rtk_snd_init_ao_ring(substream))
			return -ENOMEM;

		if (rtk_snd_ao_info(substream)) {
			ALSA_ERROR("[ALSA] %s %d\n", __func__, __LINE__);
			return -ENOMEM;
		}
		rtk_snd_resume(substream);
	} else if (dpcm->audiopath == AUDIO_PATH_AO) {
		ALSA_DEBUG_INFO("[ALSA] AO interleaved\n");
		dpcm->agent_id = dpcm->ao_agent;
		dpcm->pin_id = dpcm->ao_pin_id;
		dpcm->card->ao_pin_id[dev] = dpcm->ao_pin_id;
		dpcm->volume = dpcm->card->ao_flash_volume[dev];

		dpcm->output_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->input_frame_bytes = frames_to_bytes(runtime, 1);
		dpcm->output_sample_bytes = samples_to_bytes(runtime, 1);
		dpcm->input_sample_bytes = samples_to_bytes(runtime, 1);

		/*
		substream->ops->copy = NULL;
		substream->ops->silence = NULL;
		*/

		if (rtk_snd_init_ao_ring(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}

		if (rtk_snd_ao_info(substream)) {
			ALSA_ERROR("[ALSA] %d Fail!!\n", __LINE__);
			return -ENOMEM;
		}

		rtk_snd_set_ao_flashpin_volume(substream);
		rtk_snd_resume(substream);
	}

#endif

	return err;
}

static int rtk_snd_capture_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	int err = 0;
	ALSA_DEBUG_INFO("[ALSA] [%s] size %x\n", __func__, params_buffer_bytes(hw_params));
	err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	return err;
}

static int rtk_snd_playback_hw_free(struct snd_pcm_substream *substream)
{
	int ret;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;

	ALSA_DEBUG_INFO("[ALSA] [%s]\n", __func__);

	ret = 0;

	if (dpcm == NULL || dpcm->ring_init == 0) {
		if (dpcm && dpcm->ao_pin_id != 0) {
			ret = rtk_snd_release_flashpin_id(dpcm->card->card, dpcm->ao_pin_id);
#ifdef USE_FIXED_AO_PINID
			if (ret < 0) {
				release_error[substream->pcm->device] = dpcm->ao_pin_id;
			}

			dpcm->ao_pin_id = 0;
			used_ao_pin[substream->pcm->device] = 0;
			ALSA_DEBUG_INFO("[ALSA] reset used_ao_pin[%d] %d\n",
				substream->pcm->device, used_ao_pin[substream->pcm->device]);
#endif
		}
		goto exit;
	}

	if(PLAY_ES_DEVICE(substream->pcm->device)) {
		rtk_snd_es_stop(substream->pcm->device);
		rtk_snd_es_close(substream->pcm->device);
	} else {
		ret = rtk_snd_flush(substream);

#ifdef USE_FIXED_AO_PINID
		if (ret < 0)
			flush_error[substream->pcm->device] = 1;
#endif

		ret = rtk_snd_pause(substream);

#ifdef USE_FIXED_AO_PINID
		if (ret < 0)
			pause_error[substream->pcm->device] = 1;
#endif

		ret = rtk_snd_close(substream);

#ifdef USE_FIXED_AO_PINID
		if (ret < 0)
			close_error[substream->pcm->device] = 1;
#endif

		ret = rtk_snd_release_flashpin_id(dpcm->card->card, dpcm->ao_pin_id);
		dpcm->card->ao_pin_id[substream->pcm->device] = 0;
#ifdef USE_FIXED_AO_PINID
		if (ret < 0)
			release_error[substream->pcm->device] = dpcm->ao_pin_id;

		dpcm->ao_pin_id = 0;
		used_ao_pin[substream->pcm->device] = 0;
#endif
	}

exit:
	return snd_pcm_lib_free_pages(substream);
}

static int rtk_snd_capture_hw_free(struct snd_pcm_substream *substream)
{
	ALSA_DEBUG_INFO("[ALSA] [%s]\n", __func__);
	return snd_pcm_lib_free_pages(substream);
}

static int rtk_snd_playback_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_card_mars *mars;
	struct rtk_snd_pcm *dpcm;
	dma_addr_t dat;
	void *p;

	ALSA_DEBUG_INFO("[ALSA] [%s]\n", __func__);

	mars = (struct snd_card_mars *)(substream->pcm->card->private_data);

	mars->card->dev->dma_mask = &mars->card->dev->coherent_dma_mask;
	if(dma_set_mask(mars->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(mars->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(mars->card->dev, sizeof(*dpcm), &dat, GFP_ATOMIC);

	if (!p) {
		ALSA_ERROR("[ALSA] %d alloc memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, sizeof(struct rtk_snd_pcm));

	dpcm = p;
	dpcm->phy_addr = dat;
	ALSA_DEBUG_INFO("[ALSA] dpcm address = %lx\n", (unsigned long) dpcm);

	if (rtk_snd_init(mars->card)) {
		if (p) {
			dma_free_coherent(mars->card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);
		}
		return -ENOMEDIUM;
	}

	dpcm->ao_agent = alsa_agent;
	dpcm->pcm_lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	dpcm->elapsed_tasklet =	kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
	dpcm->timer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (dpcm->pcm_lock == NULL ||
		dpcm->elapsed_tasklet == NULL ||
		dpcm->timer == NULL) {
		ALSA_DEBUG_INFO("[ALSA] malloc mem fail\n");

		if (dpcm->pcm_lock != NULL) {
			kfree(dpcm->pcm_lock);
		}

		if (dpcm->elapsed_tasklet != NULL) {
			kfree(dpcm->elapsed_tasklet);
		}

		if (dpcm->timer != NULL) {
			kfree(dpcm->timer);
		}
		dma_free_coherent(mars->card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);

		return -ENOMEM;
	}

	spin_lock_init(dpcm->pcm_lock);

	runtime->private_data = dpcm;
	runtime->private_free = rtk_snd_runtime_free;

	memcpy(&runtime->hw,&rtk_snd_playback, sizeof(struct snd_pcm_hardware));
	runtime->hw.period_bytes_min = min_period_size;
	dpcm->substream = substream;
	dpcm->card = mars;

	init_timer(dpcm->timer);
	dpcm->timer->data = (unsigned long) dpcm;
	dpcm->timer->function = rtk_snd_pcm_freerun_timer_function;

	dpcm->ring_init = 0;

	tasklet_init(dpcm->elapsed_tasklet,snd_realtek_pcm_elapsed, (unsigned long)dpcm);

	INIT_WORK(&dpcm->work_resume, rtk_snd_resume_work);
	INIT_WORK(&dpcm->work_pause, rtk_snd_pause_work);
	INIT_WORK(&dpcm->work_flush, rtk_snd_eos_work);

	snd_pcm_hw_constraint_minmax(runtime,SNDRV_PCM_HW_PARAM_BUFFER_TIME, CONSTRAINT_MIN, CONSTRAINT_MAX);

	ALSA_DEBUG_INFO("[ALSA] open playback device %d\n", substream->pcm->device);
	return 0;
}

static int rtk_snd_capture_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm;
	struct snd_card_mars *mars;
	int err;
	void *p;
	dma_addr_t dat;

	mars = (struct snd_card_mars *)
		(substream->pcm->card->private_data);

	/* allocate memory for card instance */

	mars->card->dev->dma_mask = &mars->card->dev->coherent_dma_mask;
	if(dma_set_mask(mars->card->dev, DMA_BIT_MASK(32))) {
		ALSA_ERROR("[ALSA] %d:DMA not supported\n",__LINE__);
	}
	arch_setup_dma_ops(mars->card->dev, 0, 0, NULL, false);

	p = dma_alloc_coherent(mars->card->dev, sizeof(*dpcm), &dat, GFP_ATOMIC);

	if (!p)	{
		ALSA_ERROR("[ALSA] %d allocate memory fail\n", __LINE__);
		return -ENOMEM;
	}

	/* set parameter to 0, for snapshot */
	memset(p, 0, sizeof(struct rtk_snd_cap_pcm));

	dpcm = p;
	dpcm->phy_addr = dat;

	dpcm->pcm_lock = kmalloc(sizeof(spinlock_t), GFP_KERNEL);
	dpcm->elapsed_tasklet =
		kmalloc(sizeof(struct tasklet_struct), GFP_KERNEL);
	dpcm->timer = kmalloc(sizeof(struct timer_list), GFP_KERNEL);

	if (dpcm->pcm_lock == NULL ||
		dpcm->elapsed_tasklet == NULL ||
		dpcm->timer == NULL) {
		ALSA_DEBUG_INFO("[ALSA] malloc mem fail\n");

		if (dpcm->pcm_lock != NULL) {
			kfree(dpcm->pcm_lock);
		}

		if (dpcm->elapsed_tasklet != NULL) {
			kfree(dpcm->elapsed_tasklet);
		}

		if (dpcm->timer != NULL) {
			kfree(dpcm->timer);
		}
		dma_free_coherent(mars->card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);
		return -ENOMEM;
	}

	//ALSA_INFO("[ALSA] %s:%d start open capture device-%d\n", __FUNCTION__, __LINE__, substream->pcm->device);
	spin_lock_init(dpcm->pcm_lock);

	/* set pcm capture hardware consraint */
	memcpy(&runtime->hw, &rtk_snd_capture,
		sizeof(struct snd_pcm_hardware));

	dpcm->substream = substream;
	runtime->private_data = dpcm;
	runtime->private_free = snd_card_capture_runtime_free;

	dpcm->card = mars;

    if(!strcmp(dpcm->card->card->longname, "Mars 1"))//snd card 0
    {
        if(substream->pcm->device == CAPTURE_PCM)//c0d10 for wisa/bt
        {
			if (rtk_snd_create_ao_capture_agent(substream) < 0) {
				err = -ENOMEM;
				goto fail;
			}
        }
        else if(substream->pcm->device == CAPTURE_ES){//c0d11 for AENC
			#ifdef AENC_DUMP_BS
			aenc_dump_file = aenc_file_open("/tmp/khal_aend_bs.aac", O_RDWR | O_CREAT, 0644);
			#endif
			if(!aenc_flow_start(aenc_cmd_info.decoder_idx, aenc_cmd_info.codec, aenc_cmd_info.bitrate, adec_cmd_info.src_type[sndout_cmd_info.main_audio_port], aenc_cmd_info.volume, aenc_cmd_info.gain)) {
				ALSA_ERROR("[ALSA:%d] aenc_flow_start() failed\n", __LINE__);
				err = -EPERM;
				goto fail;
			}
        }
        else
        {
			if (rtk_snd_create_ai_agent(substream) < 0) {
				err = -ENOMEM;
				goto fail;
			}
        }
    }

	/* init capture timer */
	init_timer(dpcm->timer);
	dpcm->timer->data = (unsigned long) dpcm;
	if(substream->pcm->device == CAPTURE_ES){
		dpcm->timer->function = rtk_snd_pcm_capture_es_timer_function;
	} else {
		dpcm->timer->function = rtk_snd_pcm_capture_timer_function;
	}

	dpcm->ring_init = 0;
	tasklet_init(dpcm->elapsed_tasklet, snd_realtek_pcm_capture_elapsed,
		(unsigned long)dpcm);

	ALSA_INFO("[ALSA] %s open a realtek pcm capture device-%d success\n", __FUNCTION__, substream->pcm->device);
	return 0;

fail:

	if (dpcm) {
		if (dpcm->pcm_lock != NULL) {
			kfree(dpcm->pcm_lock);
		}

		if (dpcm->elapsed_tasklet != NULL) {
			kfree(dpcm->elapsed_tasklet);
		}

		if (dpcm->timer != NULL) {
			kfree(dpcm->timer);
		}

		dma_free_coherent(mars->card->dev, sizeof(*dpcm), dpcm, dpcm->phy_addr);
		// to avoid double free in private_free callback
		runtime->private_data = NULL;
	}
	ALSA_ERROR("[ALSA] open a realtek pcm capture fail\n");
	return err;
}

static int rtk_snd_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_pcm *dpcm = runtime->private_data;
	ALSA_DEBUG_INFO("[ALSA] [%s]\n", __func__);

	tasklet_kill(dpcm->elapsed_tasklet);

	snd_open_count--;
	return 0;
}

static int rtk_snd_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct rtk_snd_cap_pcm *dpcm = runtime->private_data;

	ALSA_INFO("[ALSA] close a realtek pcm capture device-%d\n", substream->pcm->device);

    if(!strcmp(dpcm->card->card->longname, "Mars 1"))//snd card 0
    {
        if(substream->pcm->device == CAPTURE_PCM)//c0d10 for wisa/bt
        {
			ALSA_DEBUG_INFO("[ALSA] close AO capture channel\n");
		    snd_realtek_ao_capture_hw_close(substream);
        }
        else if(substream->pcm->device == CAPTURE_ES){
			ALSA_DEBUG_INFO("[ALSA] close ES capture channel\n");
			aenc_flow_stop(aenc_cmd_info.decoder_idx, adec_cmd_info.src_type[sndout_cmd_info.main_audio_port]);
			#ifdef AENC_DUMP_BS
			aenc_file_close(aenc_dump_file);
			aenc_dump_file = NULL;
			#endif
        }
        else
        {
            ALSA_DEBUG_INFO("[ALSA] close AI capture channel\n");
			snd_realtek_ai_hw_close(substream);
        }
    }

	tasklet_kill(dpcm->elapsed_tasklet);
	snd_open_ai_count--;
	return 0;
}

static int rtk_snd_playback_ioctl(struct snd_pcm_substream *substream,
	unsigned int cmd, void *arg)
{
	switch (cmd) {
	default:
		return snd_pcm_lib_ioctl(substream, cmd, arg);
		break;

	}

	return 0;
}

static int rtk_snd_capture_ioctl(struct snd_pcm_substream *substream,
	unsigned int cmd, void *arg)
{
	switch (cmd) {
	default:
		return snd_pcm_lib_ioctl(substream, cmd, arg);
		break;
	}

	return 0;
}

static struct snd_pcm_ops rtk_snd_playback_ops = {
	.open =	rtk_snd_playback_open,
	.close = rtk_snd_playback_close,
	.ioctl = rtk_snd_playback_ioctl,
	.hw_params = rtk_snd_playback_hw_params,
	.hw_free = rtk_snd_playback_hw_free,
	.prepare = rtk_snd_playback_prepare,
	.trigger = rtk_snd_playback_trigger,
	.pointer = rtk_snd_playback_pointer,
};

static struct snd_pcm_ops rtk_snd_capture_ops = {
	.open =	rtk_snd_capture_open,
	.close = rtk_snd_capture_close,
	.ioctl = rtk_snd_capture_ioctl,
	.hw_params = rtk_snd_capture_hw_params,
	.hw_free = rtk_snd_capture_hw_free,
	.prepare = rtk_snd_capture_prepare,
	.trigger = rtk_snd_capture_trigger,
	.pointer = rtk_snd_capture_pointer,
};

static int __init rtk_snd_pcm(struct snd_card_mars *mars,
	int device, int substreams, int card)
{
	struct snd_pcm *pcm;
	int err;
	struct snd_pcm_substream *p;
	int i;

#ifndef CONFIG_CUSTOMER_TV006
	if (have_global_ai == 1 && device < 4  && card == 0) {
		ALSA_DEBUG_INFO("[ALSA] support capture deivce C%dD%d\n",
			card, device);
		err = snd_pcm_new(mars->card, "Mars PCM", device,
			substreams, substreams, &pcm);
		if (err < 0)
			return err;
	} else if (card == 0 && device != 0) {
		ALSA_DEBUG_INFO("[ALSA] not support capture device C%dD%d\n",
			card, device);
		err = snd_pcm_new(mars->card, "Mars PCM", device,
			substreams, 0, &pcm);
		if (err < 0)
			return err;
	} else if (card == 1 && device == 0) {
		ALSA_DEBUG_INFO("[ALSA] not support capture device C%dD%d\n",
			card, device);
		err = snd_pcm_new(mars->card, "Mars PCM", device,
			0, substreams, &pcm);
		if (err < 0)
			return err;
	} else {
		pr_info("[ALSA] not support all device C%dD%d\n", card, device);
	}

	if (card == 0){
		snd_pcm_set_ops(pcm,
			SNDRV_PCM_STREAM_PLAYBACK, &rtk_snd_playback_ops);
	}

	if ((have_global_ai == 1 && device < 4 && card == 0) ||
		(card == 1 && device == 0)) {
		snd_pcm_set_ops(pcm,
			SNDRV_PCM_STREAM_CAPTURE, &rtk_snd_capture_ops);
	}

	/* set init flashpin volume to 0dB*/
	for (i = 0; i < MIXER_ADDR_MAX; i++)
		mars->ao_flash_volume[i] = 389;

	pcm->private_data = mars;
	pcm->info_flags = 0;
	strcpy(pcm->name, "Mars PCM");

	/* set playback device */
	if (card == 0) {
		p = pcm->streams[0].substream;
		for (i = 0; i < substreams; i++) {
			p->dma_buffer.dev.dev =	snd_dma_continuous_data(GFP_KERNEL);
			p->dma_buffer.dev.type = SNDRV_RTK_DMA_TYPE;
			p = p->next;
		}
	}

	/* set capture device */
	if ((have_global_ai == 1 && device < 4 && card == 0) ||
		(card == 1 && device == 0)) {
		p = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
		for (i = 0; i < substreams; i++) {
			p->dma_buffer.dev.dev =	snd_dma_continuous_data(GFP_KERNEL);
			p->dma_buffer.dev.type = SNDRV_RTK_DMA_TYPE;
			p = p->next;
		}
	}
#else
	if (card == 0 && device < 8) {
		ALSA_DEBUG_INFO("[ALSA] support PCM playback deivce C%dD%d\n",card, device);
		err = snd_pcm_new(mars->card, SND_DEVICE_NAME[device], device, substreams, 0, &pcm);
		if (err < 0)
			return err;
	} else if(card == 0 && device < 10) {
		ALSA_DEBUG_INFO("[ALSA] support ES playback deivce C%dD%d\n",card, device);
		err = snd_pcm_new(mars->card, SND_DEVICE_NAME[device], device, substreams, 0, &pcm);
		if (err < 0)
			return err;
	} else if(card == 0 && device < SNDRV_CARDS && have_global_ai == 1) {
		ALSA_DEBUG_INFO("[ALSA] support capture deivce C%dD%d\n",card, device);
		err = snd_pcm_new(mars->card, SND_DEVICE_NAME[device], device, 0, substreams, &pcm);
		if (err < 0)
			return err;
	} else {
		pr_info("[ALSA] not support all device C%dD%d\n", card, device);
		return -EINVAL;
	}

	if (card == 0 && device < 10){
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &rtk_snd_playback_ops);
	}

	if ((card == 0 && have_global_ai == 1 && device >= 10 && device < SNDRV_CARDS)) {
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &rtk_snd_capture_ops);
	}

	/* set init flashpin volume to 0dB*/
	for (i = 0; i < MIXER_ADDR_MAX; i++)
		mars->ao_flash_volume[i] = 389;

	pcm->private_data = mars;
	pcm->info_flags = 0;
	strcpy(pcm->name, SND_DEVICE_NAME[device]);

	/* set playback device */
	if (card == 0 && device < 10){
		p = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
		for (i = 0; i < substreams; i++) {
			p->dma_buffer.dev.dev =	snd_dma_continuous_data(GFP_KERNEL);
			p->dma_buffer.dev.type = SNDRV_RTK_DMA_TYPE;
			p = p->next;
		}
	}

	/* set capture device */
	if ((card == 0 && have_global_ai == 1 && device >= 10 && device < SNDRV_CARDS)) {
		p = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
		for (i = 0; i < substreams; i++) {
			p->dma_buffer.dev.dev =	snd_dma_continuous_data(GFP_KERNEL);
			p->dma_buffer.dev.type = SNDRV_RTK_DMA_TYPE;
			p = p->next;
		}
	}
#endif

	return 0;
}

void fill_get_struct_1v(UINT32 *item, int is_gain)
{
    int i;
    for(i = 0; i < ADEC_MAX_CAP; i++) {
        item[(i)*2]     = i;
        if(is_gain)
            item[(i)*2+1] = ALSA_GAIN_0dB;
    }
    for(; i < ADEC_TABLE_MAX; i++) {
        item[(i)*2]     = 0xFF;
        item[(i)*2 + 1] = 0xFF;
    }
    for(i = 0; i < AMIXER_MAX_CAP; i++) {
        item[ALSA_AMIXER_BASE+(i)*2] = i;
        if(is_gain)
            item[ALSA_AMIXER_BASE+(i)*2+1] = ALSA_GAIN_0dB;
    }
}

void set_dly_output_status(UINT32 *dly_status, UINT32 output_type, UINT32 output_dly)
{
    switch(output_type){
        case COMMON_SPK:
            dly_status[DLY_OUTPUT_BASE + INDEX_SPK] = output_dly;
            break;
        case COMMON_OPTIC:
            dly_status[DLY_OUTPUT_BASE + INDEX_OPTIC] = output_dly;
            break;
        case COMMON_OPTIC_LG:
            dly_status[DLY_OUTPUT_BASE + INDEX_OPTIC_LG] = output_dly;
            break;
        case COMMON_HP:
            dly_status[DLY_OUTPUT_BASE + INDEX_HP] = output_dly;
            break;
        case COMMON_ARC:
            dly_status[DLY_OUTPUT_BASE + INDEX_ARC] = output_dly;
            break;
        case COMMON_BLUETOOTH:
            dly_status[DLY_OUTPUT_BASE + INDEX_BLUETOOTH] = output_dly;
            break;
        case COMMON_WISA:
            dly_status[DLY_OUTPUT_BASE + INDEX_WISA] = output_dly;
            break;
        case COMMON_SE_BT:
            dly_status[DLY_OUTPUT_BASE + INDEX_SE_BT] = output_dly;
            break;
        default:
            ALSA_WARN("[ALSA] output type(%d) not found!\n",output_type);
    }
}

int snd_mars_ctl_delay_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 400;//AUD_MAX_SNDOUT_DELAY
    uinfo->count = 32;
    return 0;
}

int snd_mars_ctl_delay_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    UINT32 *item = ucontrol->value.enumerated.item;

    ALSA_DEBUG_INFO("[ALSA] get name %s\n", ucontrol->id.name);
    memset(item, 0, sizeof(UINT32)*128);
    spin_lock_irqsave(&mars->mixer_lock, flags);
    if(strcmp(ucontrol->id.name, DELAY_INPUTOUTPUT)==0){
        memcpy(item, &dly_cmd_info.dly_status[0], sizeof(UINT32)*(GAIN_INPUT_TABLE_MAX+INDEX_SNDOUT_MAX));
    }
    spin_unlock_irqrestore(&mars->mixer_lock, flags);
    return 0;
}

int snd_mars_ctl_delay_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    int ret = NOT_OK;
    UINT32 input_type, input_port, output_type, input_dly, output_dly;
    UINT32 *item = ucontrol->value.enumerated.item;

    if(strcmp(ucontrol->id.name, DELAY_INPUTOUTPUT)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        input_type  = item[0];
        input_port  = item[1];
        input_dly   = item[2];
        output_type = item[3];
        output_dly  = item[4];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if(input_type != COMMON_NO_INPUT && output_type != COMMON_NO_OUTPUT)
        {
            HAL_AUDIO_INPUT_DELAY_PARAM_T  inputParam;
            HAL_AUDIO_OUTPUT_DELAY_PARAM_T outputParam;
            inputParam.adecIndex     = input_port;
            inputParam.delayTime     = input_dly;
            outputParam.soundOutType = output_type;
            outputParam.delayTime    = output_dly;
            ret = HAL_AUDIO_SetInputOutputDelay(inputParam, outputParam);
            if(ret == OK){
                if(input_type == COMMON_INPUT_ADEC){
                    dly_cmd_info.dly_status[input_port*2+1] = input_dly;
                }else if(input_type == COMMON_INPUT_AMIXER)
                    dly_cmd_info.dly_status[ALSA_AMIXER_BASE + input_port*2+1] = input_dly;
                set_dly_output_status(&dly_cmd_info.dly_status[0], output_type, output_dly);
            }
        }else if(input_type != COMMON_NO_INPUT){
            ret = HAL_AUDIO_SetDecoderDelayTime(input_port, input_dly);
            if(ret == OK){
                if(input_type == COMMON_INPUT_ADEC){
                    dly_cmd_info.dly_status[input_port*2+1] = input_dly;
                }else if(input_type == COMMON_INPUT_AMIXER)
                    dly_cmd_info.dly_status[ALSA_AMIXER_BASE + input_port*2+1] = input_dly;
            }
        }else if(output_type != COMMON_NO_OUTPUT){
            switch(output_type){
            case COMMON_SPK:
                ret = HAL_AUDIO_SetSPKOutDelayTime(output_dly ,0);
                break;
            case COMMON_OPTIC:
                ret = HAL_AUDIO_SetSPDIFOutDelayTime(output_dly ,0);
                break;
            case COMMON_OPTIC_LG:
                ret = HAL_AUDIO_SetSPDIFSBOutDelayTime(output_dly ,0);
                break;
            case COMMON_HP:
                ret = HAL_AUDIO_SetHPOutDelayTime(output_dly ,0);
                break;
            case COMMON_ARC:
                ret = HAL_AUDIO_SetARCOutDelayTime(output_dly ,0);
                break;
            case COMMON_BLUETOOTH:
                ret = HAL_AUDIO_SetBTOutDelayTime(output_dly);
                break;
            case COMMON_WISA:
                ret = HAL_AUDIO_SetWISAOutDelayTime(output_dly);
                break;
            case COMMON_SE_BT:
                ret = HAL_AUDIO_SetSEBTOutDelayTime(output_dly);
                break;
            default:
                ALSA_DEBUG_INFO("[ALSA] output type(%d) not found!\n",output_type);
                break;
            }
            if(ret == OK)
                set_dly_output_status(&dly_cmd_info.dly_status[0], output_type, output_dly);
        }
        ALSA_INFO("[ALSA-DELAY] put name \033[31m%s\033[0m, input_type %d, input_port_num %d, input_delay %d output_type %d, output_delay %d\n",
                ucontrol->id.name, input_type, input_port, input_dly, output_type, output_dly);
    }

    return ret;
}

int snd_mars_ctl_hdmi_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 256;
    uinfo->count = 1;

    return 0;
}

int snd_mars_ctl_hdmi_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    int ret = NOT_OK;
    UINT32 *item = ucontrol->value.enumerated.item;

    if(strcmp(ucontrol->id.name, AHDMI_PORT0_AUDIOMODE)==0){
        ret = HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI0, (ahdmi_type_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT1_AUDIOMODE)==0){
        ret = HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI1, (ahdmi_type_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT2_AUDIOMODE)==0){
        ret = HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI2, (ahdmi_type_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT3_AUDIOMODE)==0){
        ret = HAL_AUDIO_HDMI_GetPortAudioMode(HAL_AUDIO_HDMI3, (ahdmi_type_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT0_COPYPROTECTIONINFO)==0){
        ret = HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI0,
                (ahdmi_copyprotection_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT1_COPYPROTECTIONINFO)==0){
        ret = HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI1,
                (ahdmi_copyprotection_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT2_COPYPROTECTIONINFO)==0){
        ret = HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI2,
                (ahdmi_copyprotection_ext_type_t*)&item[0]);
    }else if(strcmp(ucontrol->id.name, AHDMI_PORT3_COPYPROTECTIONINFO)==0){
        ret = HAL_AUDIO_HDMI_GetPortCopyInfo(HAL_AUDIO_HDMI3,
                (ahdmi_copyprotection_ext_type_t*)&item[0]);
    }
    /*ALSA_DEBUG_INFO("[ALSA] get name \033[31m%s\033[0m item0 %d\n", ucontrol->id.name,*/
                                        /*ucontrol->value.enumerated.item[0]);*/
	return ret;
}

int snd_mars_ctl_hdmi_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    int ret = OK;

    ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m item0 %d item1 %d\n", ucontrol->id.name,
                                        ucontrol->value.enumerated.item[0],
                                        ucontrol->value.enumerated.item[1]);
	return ret;
}

static DIRECT_CODEC_INFO codec_info;
int snd_mars_ctl_direct_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	/*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 256;
	uinfo->count = 5;

	return 0;
}

int snd_mars_ctl_direct_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = OK;
	UINT32 *item = ucontrol->value.enumerated.item;

	spin_lock_irqsave(&mars->mixer_lock, flags);
	if(strcmp(ucontrol->id.name, DIRECT_CODEC)==0){
		item[0] = 0;
		item[1] = es_codec[0];
		item[2] = es_fs[0];
		item[3] = es_nch[0];
		item[4] = es_bps[0];
	}
	spin_unlock_irqrestore(&mars->mixer_lock, flags);
	ALSA_DEBUG_INFO("[ALSA] get name \033[31m%s\033[0m item0 %d\n", ucontrol->id.name,
			ucontrol->value.enumerated.item[0]);
	return ret;
}

int snd_mars_ctl_direct_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = NOT_OK;
	UINT32 *item = ucontrol->value.enumerated.item;

	ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m item0 %d item1 %d item2 %d item3 %d item4 %d\n",
			ucontrol->id.name, item[0], item[1], item[2], item[3], item[4]);
	if(strcmp(ucontrol->id.name, DIRECT_CODEC)==0){
		spin_lock_irqsave(&mars->mixer_lock, flags);
		codec_info.adecport      = item[0];
		codec_info.codecType     = item[1];
		codec_info.samplingFreq  = item[2];
		codec_info.numbOfChannel = item[3];
		codec_info.bitPerSample  = item[4];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		if(codec_info.adecport != 0 && codec_info.adecport != 1){
			ret = NOT_OK;
		} else {
			es_codec[codec_info.adecport] = codec_info.codecType;
			es_fs[codec_info.adecport]    = codec_info.samplingFreq;
			es_nch[codec_info.adecport]   = codec_info.numbOfChannel;
			es_bps[codec_info.adecport]   = codec_info.bitPerSample;
			ret = OK;
		}
	}
	return ret;
}

int snd_mars_ctl_mute_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 256;
    if(strcmp(uinfo->id.name, MUTE_INPUT)==0){
        uinfo->count = 24;
    }else if(strcmp(uinfo->id.name, MUTE_OUTPUT)==0){
        uinfo->count = 8;
    }
    return 0;
}

int snd_mars_ctl_mute_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    UINT32 *item = ucontrol->value.enumerated.item;

    memset(item, 0, sizeof(UINT32)*128);
    spin_lock_irqsave(&mars->mixer_lock, flags);
    if(strcmp(ucontrol->id.name, MUTE_INPUT)==0){
        fill_get_struct_1v(item, FALSE);
        KHAL_AUDIO_Get_INPUT_MuteStatus(item);
    }else if(strcmp(ucontrol->id.name, MUTE_OUTPUT)==0){
        KHAL_AUDIO_Get_OUTPUT_MuteStatus(item);
    }
    spin_unlock_irqrestore(&mars->mixer_lock, flags);
    ALSA_DEBUG_INFO("[ALSA] get name %s\n", ucontrol->id.name);
    return 0;
}

int snd_mars_ctl_mute_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    int ret = NOT_OK;
    unsigned int output_type, input_type, port, mute_status;
    UINT32 *item = ucontrol->value.enumerated.item;

    ALSA_INFO("[ALSA-MUTE] put name \033[31m%s\033[0m item0 %d item1 %d item2 %d\n",
                    ucontrol->id.name, item[0], item[1], item[2]);
    if(strcmp(ucontrol->id.name, MUTE_INPUT)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        input_type  = item[0];
        port        = item[1];
        mute_status = item[2];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if(!(AUDIO_CHECK_ISBOOLEAN(mute_status))){
            return NOT_OK;
        }
        if (input_type == COMMON_INPUT_ADEC){
            ret = HAL_AUDIO_SetDecoderInputMute(port, mute_status);
        }else if (input_type == COMMON_INPUT_AMIXER){
            ret = HAL_AUDIO_SetMixerInputMute(port, mute_status);
        }
    }else if(strcmp(ucontrol->id.name, MUTE_OUTPUT)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        output_type = item[0];
        mute_status = item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if(!(AUDIO_CHECK_ISBOOLEAN(mute_status))){
            return NOT_OK;
        }

        switch(output_type){
        case COMMON_SPK:
            ret = HAL_AUDIO_SetSPKOutMute(mute_status);
            break;
        case COMMON_OPTIC:
        case COMMON_OPTIC_LG:
            ret = HAL_AUDIO_SetSPDIFOutMute(mute_status, output_type);
            break;
        case COMMON_HP:
            ret = HAL_AUDIO_SetHPOutMute(mute_status);
            break;
        case COMMON_ARC:
            ret = HAL_AUDIO_SetARCOutMute(mute_status);
            break;
        case COMMON_BLUETOOTH:
            g_capture_dev_mute[1] = mute_status;
            if(g_capture_dev == ENUM_DEVICE_BLUETOOTH) {
                rtk_snd_set_ao_capture_mute(g_capture_dev_mute[1]);
                HAL_AUDIO_SetBTOutMute(mute_status);
            }
            ret = OK;
            break;
        case COMMON_WISA:
            g_capture_dev_mute[2] = mute_status;
            if(g_capture_dev == ENUM_DEVICE_WISA) {
                rtk_snd_set_ao_capture_mute(g_capture_dev_mute[2]);
                HAL_AUDIO_SetWISAOutMute(mute_status);
            }
            ret = OK;
            break;
        case COMMON_SE_BT:
            g_capture_dev_mute[3] = mute_status;
            if(g_capture_dev == ENUM_DEVICE_SE_BT) {
                rtk_snd_set_ao_capture_mute(g_capture_dev_mute[3]);
                HAL_AUDIO_SetSEBTOutMute(mute_status);
            }
            ret = OK;
            break;
        default:
            ALSA_DEBUG_INFO("[ALSA] output type(%d) not found!\n", output_type);
            break;
        }
    }

	return ret;
}

extern unsigned int adecVolume_search(unsigned int adecVolume);
int snd_mars_ctl_gain_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
    uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = GAIN_MAX_VALUE; //30 dB
    if(strcmp(uinfo->id.name, GAIN_INPUT)==0){
        uinfo->count = 24;
    }else if(strcmp(uinfo->id.name, GAIN_OUTPUT)==0){
        uinfo->count = 8;
    }else if(strcmp(uinfo->id.name, GAIN_AUDIODESCRIPTION)==0){
        uinfo->count = 1;
    }
    return 0;
}

int snd_mars_ctl_gain_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
    UINT32 *item = ucontrol->value.enumerated.item;

    memset(item, 0, sizeof(UINT32)*128);
    ALSA_DEBUG_INFO("[ALSA] get name %s\n", ucontrol->id.name);
    spin_lock_irqsave(&mars->mixer_lock, flags);
    if(strcmp(ucontrol->id.name, GAIN_INPUT)==0){
        memcpy(item, gain_cmd_info.input_gain, sizeof(unsigned int)*GAIN_INPUT_TABLE_MAX);
    }else if(strcmp(ucontrol->id.name, GAIN_OUTPUT)==0){
        memcpy(item, gain_cmd_info.output_gain, sizeof(unsigned int)*INDEX_SNDOUT_MAX);
    }else if(strcmp(ucontrol->id.name, GAIN_AUDIODESCRIPTION)==0){
        item[0] = gain_cmd_info.ad_gain;
    }
    spin_unlock_irqrestore(&mars->mixer_lock, flags);
    return 0;
}

int snd_mars_ctl_gain_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    int ret = NOT_OK;
    unsigned int rtk_gain, gain, input_type, output_type, port;
    HAL_AUDIO_VOLUME_T volume;
    UINT32 *item = ucontrol->value.enumerated.item;

    ALSA_INFO("[ALSA-GAIN] put name \033[31m%s\033[0m item0 %d item1 %d item2 %d\n",
                    ucontrol->id.name, item[0], item[1], item[2]);
    if(strcmp(ucontrol->id.name, GAIN_INPUT)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        input_type = item[0];
        port       = item[1];
        gain       = item[2];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        rtk_gain = adecVolume_search(gain);
        volume.mainVol = (rtk_gain >> BITS_PER_BYTE) & 0xff;
        volume.fineVol = rtk_gain & 0xff;
        if(gain > GAIN_MAX_VALUE)
            return NOT_OK;
        if(input_type == COMMON_INPUT_ADEC){
            ret = HAL_AUDIO_SetDecoderInputGain(port, volume, gain);
            if(ret == OK)
                gain_cmd_info.input_gain[(port)*2+1] = gain;
            else
                gain_cmd_info.input_gain[(port)*2+1] = ALSA_GAIN_0dB;
        }else if(input_type == COMMON_INPUT_AMIXER){
            ret = HAL_AUDIO_SetMixerInputGain(port, volume);
            if(ret == OK)
                gain_cmd_info.input_gain[ALSA_AMIXER_BASE+(port)*2+1] = gain;
            else
                gain_cmd_info.input_gain[ALSA_AMIXER_BASE+(port)*2+1] = ALSA_GAIN_0dB;
        }
    }else if(strcmp(ucontrol->id.name, GAIN_OUTPUT)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        output_type = item[0];
        gain        = item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        rtk_gain = adecVolume_search(gain);
        volume.mainVol = (rtk_gain >> BITS_PER_BYTE) & 0xff;
        volume.fineVol = rtk_gain & 0xff;
        if(gain > GAIN_MAX_VALUE)
            return NOT_OK;
        switch(output_type){
        case COMMON_SPK:
            ret = HAL_AUDIO_SetSPKOutVolume(volume, gain);
            if(ret == OK)
                gain_cmd_info.output_gain[INDEX_SPK] = gain;
            break;
        case COMMON_OPTIC:
            ret = HAL_AUDIO_SetSPDIFOutVolume(volume, gain);
            if(ret == OK)
                gain_cmd_info.output_gain[INDEX_OPTIC]    = gain;
            break;
        case COMMON_OPTIC_LG:
            ret = HAL_AUDIO_SetSPDIFOutVolume(volume, gain);
            if(ret == OK)
                gain_cmd_info.output_gain[INDEX_OPTIC_LG] = gain;
            break;
        case COMMON_HP:
            ret = HAL_AUDIO_SetHPOutVolume(volume, 0, gain);
            if(ret == OK)
                gain_cmd_info.output_gain[INDEX_HP] = gain;
            break;
        case COMMON_ARC:
            ret = HAL_AUDIO_SetARCOutVolume(volume, gain);
            if(ret == OK)
                gain_cmd_info.output_gain[INDEX_ARC] = gain;
            break;
        case COMMON_BLUETOOTH:
            g_capture_dev_vol[1] = volume;
            if(g_capture_dev == ENUM_DEVICE_BLUETOOTH) {
                ret = rtk_snd_set_ao_capture_volume(g_capture_dev_vol[1]);
                HAL_AUDIO_SetBTOutVolume(gain);
            }
            if(ret == S_OK)
                gain_cmd_info.output_gain[INDEX_BLUETOOTH] = gain;
            break;
        case COMMON_WISA:
            g_capture_dev_vol[2] = volume;
            if(g_capture_dev == ENUM_DEVICE_WISA) {
                ret = rtk_snd_set_ao_capture_volume(g_capture_dev_vol[2]);
                HAL_AUDIO_SetWISAOutVolume(gain);
            }
            if(ret == S_OK)
                gain_cmd_info.output_gain[INDEX_WISA] = gain;
            break;
        case COMMON_SE_BT:
            g_capture_dev_vol[3] = volume;
            if(g_capture_dev == ENUM_DEVICE_SE_BT) {
                ret = rtk_snd_set_ao_capture_volume(g_capture_dev_vol[3]);
                HAL_AUDIO_SetSEBTOutVolume(gain);
            }
            if(ret == S_OK)
                gain_cmd_info.output_gain[INDEX_SE_BT] = gain;
            break;
        default:
            ALSA_DEBUG_INFO("[ALSA] output type(%d) not found!\n",output_type);
            break;
        }
    }else if(strcmp(ucontrol->id.name, GAIN_AUDIODESCRIPTION)==0){
        spin_lock_irqsave(&mars->mixer_lock, flags);
        gain = item[0];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        rtk_gain = adecVolume_search(gain);
        volume.mainVol = (rtk_gain >> BITS_PER_BYTE) & 0xff;
        volume.fineVol = rtk_gain & 0xff;
        if(gain > GAIN_MAX_VALUE)
            return NOT_OK;
        ret = HAL_AUDIO_SetAudioDescriptionVolume(HAL_AUDIO_ADEC0, volume);
        if(ret == NOT_OK){
            gain_cmd_info.ad_gain = ALSA_GAIN_0dB;
        }else{
            gain_cmd_info.ad_gain = gain;
        }
    }

	return ret;
}

int snd_mars_ctl_sndout_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 1;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 256;
    if(strcmp(uinfo->id.name, SNDOUT_OPEN)==0){
    }else if(strcmp(uinfo->id.name, SNDOUT_CLOSE)==0){
    }else if(strcmp(uinfo->id.name, SNDOUT_CONNECT)==0){
        uinfo->count = 9;
    }else if(strcmp(uinfo->id.name, SNDOUT_DISCONNECT)==0){
        uinfo->count = 3;
    }else if(strcmp(uinfo->id.name, SNDOUT_MAINAUDIO_OUTPUT)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, SNDOUT_SPDIF_OUTPUTTYPE)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, SNDOUT_SPDIF_CATERGORYCODE)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, SNDOUT_SPDIF_COPYPROTECTIONINFO)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, SNDOUT_OPTIC_LIGHTONOFF)==0){
        uinfo->value.integer.max = 1;
    }else if(strcmp(uinfo->id.name, SNDOUT_ARC_ONOFF)==0){
        uinfo->value.integer.max = 1;
    }else if(strcmp(uinfo->id.name, SNDOUT_OPTIC_LG)==0){
        uinfo->count = 5;
    }else if(strcmp(uinfo->id.name, SNDOUT_SPK_OUTPUT)==0){
    }else if(strcmp(uinfo->id.name, SNDOUT_CAPTURE_DISABLE)==0){
        uinfo->value.integer.max = 1;
    }else if(strcmp(uinfo->id.name, SNDOUT_EARC_ONOFF)==0){
        uinfo->value.integer.max = 1;
    }else if(strcmp(uinfo->id.name, SNDOUT_EARC_OUTPUT_TYPE)==0){
        uinfo->count = 5;
        uinfo->value.integer.max = 192000;
    }else if(strcmp(uinfo->id.name, SNDOUT_CAPTURE_OUTPUT_RESAMPLING)==0){
        uinfo->value.integer.min = WISA_RESAMPLER_MIN;
        uinfo->value.integer.max = WISA_RESAMPLER_MAX;
    }else if(strcmp(uinfo->id.name, SNDOUT_CAPTURE_INPUT_TIMECLOCK)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, SNDOUT_EAC3_ATMOS_ENCODE_ONOFF)==0){
        uinfo->value.integer.max = 1;
    }

    return 0;
}

int snd_mars_ctl_sndout_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;

    UINT32 *item = ucontrol->value.enumerated.item;
    memset(item, 0, sizeof(UINT32)*128);

    spin_lock_irqsave(&mars->mixer_lock, flags);
    if(strcmp(ucontrol->id.name, SNDOUT_OPEN)==0){
        item[0] = sndout_cmd_info.opened_device;
    }else if(strcmp(ucontrol->id.name, SNDOUT_CLOSE)==0){
        item[0] = sndout_cmd_info.output_src;
    }else if(strcmp(ucontrol->id.name, SNDOUT_CONNECT)==0){
        memcpy(item, sndout_cmd_info.connected_device, sizeof(sndout_cmd_info.connected_device));
    }else if(strcmp(ucontrol->id.name, SNDOUT_DISCONNECT)==0){
        item[0] = sndout_cmd_info.output_src;
        item[1] = sndout_cmd_info.input_type;
        item[2] = sndout_cmd_info.input_port;
    }else if(strcmp(ucontrol->id.name, SNDOUT_MAINAUDIO_OUTPUT)==0){
        item[0] = sndout_cmd_info.input_type;
        item[1] = sndout_cmd_info.main_audio_port;
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_OUTPUTTYPE)==0){
        item[0] = sndout_cmd_info.optic_mode;
        item[1] = sndout_cmd_info.arc_mode;
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_CATERGORYCODE)==0){
        item[0] = sndout_cmd_info.optic_category_code;
        item[1] = sndout_cmd_info.arc_category_code;
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_COPYPROTECTIONINFO)==0){
        item[0] = sndout_cmd_info.optic_copyprotection;
        item[1] = sndout_cmd_info.arc_copyprotection;
    }else if(strcmp(ucontrol->id.name, SNDOUT_OPTIC_LIGHTONOFF)==0){
        item[0] = sndout_cmd_info.bOnOff_optic;
    }else if(strcmp(ucontrol->id.name, SNDOUT_ARC_ONOFF)==0){
        item[0] = sndout_cmd_info.bOnOff_arc;
    }else if(strcmp(ucontrol->id.name, SNDOUT_OPTIC_LG)==0){
        item[0] = sndout_cmd_info.soundbar_info.barId;
        item[1] = sndout_cmd_info.soundbar_info.volume;
        item[2] = sndout_cmd_info.soundbar_info.bMuteOnOff;
        item[3] = sndout_cmd_info.soundbar_info.bPowerOnOff;
        item[4] = sndout_cmd_info.checksum;
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPK_OUTPUT)==0){
        item[0] = sndout_cmd_info.i2sNumber;
    }else if(strcmp(ucontrol->id.name, SNDOUT_CAPTURE_DISABLE)==0){
        item[0] = sndout_cmd_info.capture_disable;
    }else if(strcmp(ucontrol->id.name, SNDOUT_EARC_ONOFF)==0){
        item[0] = sndout_cmd_info.bOnOff_earc;
    }else if(strcmp(ucontrol->id.name, SNDOUT_EARC_OUTPUT_TYPE)==0){
        item[0] = sndout_cmd_info.earc_info.set_type;
        item[1] = sndout_cmd_info.earc_info.codec_type;
        item[2] = sndout_cmd_info.earc_info.channel_num;
        item[3] = sndout_cmd_info.earc_info.sample_rate;
        item[4] = sndout_cmd_info.earc_info.mix_option;
    }
    else if(strcmp(ucontrol->id.name, SNDOUT_CAPTURE_INPUT_TIMECLOCK)==0){
        int clock_count = 0, clock_rate = 0;
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        rtk_snd_get_clock_count(&clock_count, &clock_rate);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        item[0] = clock_count;
        item[1] = clock_rate;
    }else if(strcmp(ucontrol->id.name, SNDOUT_EAC3_ATMOS_ENCODE_ONOFF)==0){
        item[0] = sndout_cmd_info.bOnOff_eac3_atmos_enc;
    }
    spin_unlock_irqrestore(&mars->mixer_lock, flags);
    return 0;
}

int snd_mars_ctl_sndout_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    unsigned int snd_output_device = 0;
    int ret = NOT_OK;
    UINT32 *item = ucontrol->value.enumerated.item;

    ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m item0 %d item1 %d item2 %d\n",
            ucontrol->id.name, item[0], item[1], item[2]);
    if(strcmp(ucontrol->id.name, SNDOUT_OPEN)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m sndout device %d\n",
				ucontrol->id.name, item[0]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        snd_output_device = item[0];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_SNDOUT_Open(snd_output_device);
        if(ret == OK)
            sndout_cmd_info.opened_device |= snd_output_device;
    }else if(strcmp(ucontrol->id.name, SNDOUT_CLOSE)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m sndout device %d\n",
				ucontrol->id.name, item[0]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        sndout_cmd_info.output_src = item[0];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_SNDOUT_Close(sndout_cmd_info.output_src);
        if(ret == OK){
            sndout_cmd_info.opened_device &= ~sndout_cmd_info.output_src;
            KHAL_AUDIO_SNDOUT_Connect_Set_Status_to_ALSA(sndout_cmd_info.output_src, sndout_cmd_info.connected_device);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_CONNECT)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m sndout device %d input type %d port %d\n",
				ucontrol->id.name, item[0], item[1], item[2]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        sndout_cmd_info.output_src = item[0];
        sndout_cmd_info.input_type = item[1];
        sndout_cmd_info.input_port = item[2];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        switch(sndout_cmd_info.output_src)
        {
            case COMMON_BLUETOOTH:
                g_capture_dev = ENUM_DEVICE_BLUETOOTH;

                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC) {
                    HAL_AUDIO_BT_Connect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_BT_Connect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                rtk_snd_set_ao_capture_volume(g_capture_dev_vol[1]);
                break;
            case COMMON_WISA:
                g_capture_dev = ENUM_DEVICE_WISA;

                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC) {
                    HAL_AUDIO_WISA_Connect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_WISA_Connect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                rtk_snd_set_ao_capture_volume(g_capture_dev_vol[2]);
                break;
            case COMMON_SE_BT:
                g_capture_dev = ENUM_DEVICE_SE_BT;

                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC) {
                    HAL_AUDIO_SE_BT_Connect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_SE_BT_Connect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                rtk_snd_set_ao_capture_volume(g_capture_dev_vol[3]);
                break;
            default:
                break;
        }
        if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC) {
            ret = HAL_AUDIO_SNDOUT_Connect(sndout_cmd_info.output_src, sndout_cmd_info.input_port + HAL_AUDIO_RESOURCE_ADEC0);
        } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
            ret = HAL_AUDIO_SNDOUT_Connect(sndout_cmd_info.output_src, sndout_cmd_info.input_port + HAL_AUDIO_RESOURCE_MIXER0);
        }
        if(ret == OK)
            KHAL_AUDIO_SNDOUT_Connect_Set_Status_to_ALSA(sndout_cmd_info.output_src, sndout_cmd_info.connected_device);
    }else if(strcmp(ucontrol->id.name, SNDOUT_DISCONNECT)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m sndout device %d input type %d port %d\n",
				ucontrol->id.name, item[0], item[1], item[2]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        sndout_cmd_info.output_src = item[0];
        sndout_cmd_info.input_type = item[1];
        sndout_cmd_info.input_port = item[2];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC){
            ret = HAL_AUDIO_SNDOUT_Disconnect(sndout_cmd_info.output_src, sndout_cmd_info.input_port + HAL_AUDIO_RESOURCE_ADEC0);
        } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
            ret = HAL_AUDIO_SNDOUT_Disconnect(sndout_cmd_info.output_src, sndout_cmd_info.input_port + HAL_AUDIO_RESOURCE_MIXER0);
        }
        if(ret == OK)
            KHAL_AUDIO_SNDOUT_Connect_Set_Status_to_ALSA(sndout_cmd_info.output_src, sndout_cmd_info.connected_device);
        switch(sndout_cmd_info.output_src)
        {
            case COMMON_BLUETOOTH:
                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC){
                    HAL_AUDIO_BT_Disconnect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_BT_Disconnect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                if (!IS_BT_Connected()) {
                    g_capture_dev = ENUM_DEVICE_NONE;
                }
                break;
            case COMMON_WISA:
                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC){
                    HAL_AUDIO_WISA_Disconnect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_WISA_Disconnect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                if (!IS_WISA_Connected()) {
                    g_capture_dev = ENUM_DEVICE_NONE;
                }
                break;
            case COMMON_SE_BT:
                if (sndout_cmd_info.input_type == COMMON_INPUT_ADEC){
                    HAL_AUDIO_SE_BT_Disconnect(sndout_cmd_info.input_port);
                } else if (sndout_cmd_info.input_type == COMMON_INPUT_AMIXER) {
                    HAL_AUDIO_SE_BT_Disconnect(sndout_cmd_info.input_port+HAL_AUDIO_INDEX4);
                }

                if (!IS_SE_BT_Connected()) {
                    g_capture_dev = ENUM_DEVICE_NONE;
                }
                break;
            default:
                break;
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_CATERGORYCODE)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m OPT %d ARC %d\n",
				ucontrol->id.name, item[0], item[1]);
        if((int)item[0] < 0 || (int)item[1] < 0){
            ret = NOT_OK;
        }else{
            ret = HAL_AUDIO_SPDIF_SetCategoryCode(item[0]);
            ret &= HAL_AUDIO_ARC_SetCategoryCode(item[1]);
        }
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.optic_category_code = item[0];
            sndout_cmd_info.arc_category_code   = item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_COPYPROTECTIONINFO)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m OPT %d ARC %d\n",
				ucontrol->id.name, item[0], item[1]);
        ret = HAL_AUDIO_SPDIF_SetCopyInfo(item[0]);
        ret &= HAL_AUDIO_ARC_SetCopyInfo(item[1]);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.optic_copyprotection = item[0];
            sndout_cmd_info.arc_copyprotection   = item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_MAINAUDIO_OUTPUT)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m input type %d port %d\n",
				ucontrol->id.name, item[0], item[1]);
        if(item[0] == COMMON_INPUT_ADEC)
            ret = HAL_AUDIO_SetMainAudioOutput(item[1]);
        else if(item[0] == COMMON_INPUT_AMIXER) {
            HAL_AUDIO_SetMainAudioOutput(item[1]+HAL_AUDIO_INDEX4);
            ret = OK;
        } else
            ret = OK;
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.input_type      = item[0];
            sndout_cmd_info.main_audio_port = item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPDIF_OUTPUTTYPE)==0){
        ret = HAL_AUDIO_SPDIF_SetOutputType(item[0], 0);
        ret &= HAL_AUDIO_ARC_SetOutputType(item[1], 0);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.optic_mode = item[0];
            sndout_cmd_info.arc_mode   = item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_OPTIC_LIGHTONOFF)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m item0 %d\n",
				ucontrol->id.name, item[0]);
        if(!(AUDIO_CHECK_ISBOOLEAN(item[0])))
            ret = NOT_OK;
        else
            ret = HAL_AUDIO_SPDIF_SetLightOnOff(item[0]);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.bOnOff_optic = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_ARC_ONOFF)==0){
        if(!(AUDIO_CHECK_ISBOOLEAN(item[0])))
            ret = NOT_OK;
        else{
            ret = HAL_AUDIO_HDMI_SetPortAudioReturnChannel(HAL_AUDIO_HDMI0, item[0]);
        }
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.bOnOff_arc = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_OPTIC_LG)==0){
        HAL_AUDIO_SB_SET_INFO_T soundbar_info;
        soundbar_info.barId       = item[0];
        soundbar_info.volume      = item[1];
        soundbar_info.bMuteOnOff  = item[2];
        soundbar_info.bPowerOnOff = item[3];
        if(!(AUDIO_CHECK_ISBOOLEAN(item[2])) || !(AUDIO_CHECK_ISBOOLEAN(item[3])))
            ret = NOT_OK;
        else
            ret = HAL_AUDIO_SB_SetOpticalIDData(soundbar_info, &sndout_cmd_info.checksum);
        if(ret == OK) {
            spin_lock_irqsave(&mars->mixer_lock, flags);
            memcpy(&sndout_cmd_info.soundbar_info, &soundbar_info, sizeof(HAL_AUDIO_SB_SET_INFO_T));
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_SPK_OUTPUT)==0){
        if(item[0] != SNDOUT_SPK_OUTPUT_2CHANNEL && item[0] != SNDOUT_SPK_OUTPUT_4CHANNEL){
            ret = NOT_OK;
        }
        else
            ret = HAL_AUDIO_SetSPKOutput(item[0], 48000);
        if(ret == OK) {
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.i2sNumber = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_CAPTURE_DISABLE)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m item0 %d\n",
				ucontrol->id.name, item[0]);
        if(!(AUDIO_CHECK_ISBOOLEAN(item[0])))
            ret = NOT_OK;
        else
            ret = OK;
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.capture_disable = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_EARC_ONOFF)==0){
        if(!(AUDIO_CHECK_ISBOOLEAN(item[0])))
            ret = NOT_OK;
        else
            ret = RHAL_AUDIO_HDMI_SetEnhancedAudioReturnChannel(item[0]);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.bOnOff_earc = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, SNDOUT_EARC_OUTPUT_TYPE)==0){
        ALSA_SNDOUT_EARC_INFO earc_info;
        earc_info.set_type    = item[0];
        earc_info.codec_type  = item[1];
        earc_info.channel_num = item[2];
        earc_info.sample_rate = item[3];
        earc_info.mix_option  = item[4];
		ALSA_DEBUG_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m item0 %d item1 %d item2 %d item3 %d item4 %d\n",
				ucontrol->id.name, item[0], item[1], item[2], item[3], item[4]);
		if(!(AUDIO_CHECK_ISVALID(item[0],0,2)) || !(AUDIO_CHECK_ISVALID(item[1], 1, 8))
                || !(AUDIO_CHECK_ISVALID(item[2], 0, 8)) || !(AUDIO_CHECK_ISVALID(item[3], 0, 192000))
                || !(AUDIO_CHECK_ISBOOLEAN(item[4]))){
            ret = NOT_OK;
        }else
            ret = KHAL_AUDIO_SNDOUT_eARC_OutputType(earc_info);

        if(ret == OK) {
            spin_lock_irqsave(&mars->mixer_lock, flags);
            memcpy(&sndout_cmd_info.earc_info, &earc_info, sizeof(ALSA_SNDOUT_EARC_INFO));
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }
    else if(strcmp(ucontrol->id.name, SNDOUT_CAPTURE_OUTPUT_RESAMPLING)==0){
		ALSA_INFO("[ALSA-SNDOUT] put name \033[31m%s\033[0m item0 %d\n",
				ucontrol->id.name, item[0]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        g_capture_wisa_resampler = item[0];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        rtk_snd_set_wisa_resampler();
        ret = OK;
    }else if(strcmp(ucontrol->id.name, SNDOUT_EAC3_ATMOS_ENCODE_ONOFF)==0){
        if(!(AUDIO_CHECK_ISBOOLEAN(item[0])))
            ret = NOT_OK;
        else
            ret = KHAL_AUDIO_SetEAC3_ATMOS_ENCODE((BOOLEAN)item[0]);
        if(ret == OK) {
            spin_lock_irqsave(&mars->mixer_lock, flags);
            sndout_cmd_info.bOnOff_eac3_atmos_enc = item[0];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }

	return ret;
}

int snd_mars_ctl_adec_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
    /*ALSA_DEBUG_INFO("[ALSA] info name %s\n",uinfo->id.name);*/
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
    uinfo->count = 8;
    uinfo->value.integer.min = 0;
    uinfo->value.integer.max = 256;
    if(strcmp(uinfo->id.name, ADEC_OPEN)==0){
        uinfo->value.integer.max = 4;
    }else if(strcmp(uinfo->id.name, ADEC_CONNECT)==0){
    }else if(strcmp(uinfo->id.name, ADEC_CODEC)==0){
    }else if(strcmp(uinfo->id.name, ADEC_START)==0){
        uinfo->value.integer.max = 4;
    }else if(strcmp(uinfo->id.name, ADEC0_INFO)==0){
        uinfo->count = 21;
    }else if(strcmp(uinfo->id.name, ADEC1_INFO)==0){
        uinfo->count = 21;
    }else if(strcmp(uinfo->id.name, ADEC2_INFO)==0){
        uinfo->count = 21;
    }else if(strcmp(uinfo->id.name, ADEC3_INFO)==0){
        uinfo->count = 21;
    }else if(strcmp(uinfo->id.name, ADEC_MAXCAPACITY)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC_USERMAXCAPACITY)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC_SYNCMODE)==0){
        uinfo->value.integer.max = 1;
    }else if(strcmp(uinfo->id.name, ADEC_TP_DECODER_OUTPUTMODE)==0){
        uinfo->value.integer.max = 3;
    }else if(strcmp(uinfo->id.name, ADEC_DOLBYDRCMODE)==0){
        uinfo->value.integer.max = 2;
    }else if(strcmp(uinfo->id.name, ADEC_DOWNMIXMODE)==0){
        uinfo->value.integer.max = 4;
    }else if(strcmp(uinfo->id.name, ADEC_TRICKMODE)==0){
        uinfo->value.integer.max = 8;
    }else if(strcmp(uinfo->id.name, ADEC_TP_AUDIODESCRIPTION)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC_AC4_AUTO1STLANG)==0){
        uinfo->count = 12;
        uinfo->value.integer.max = 0x7FFFFFFF;
    }else if(strcmp(uinfo->id.name, ADEC_AC4_AUTO2NDLANG)==0){
        uinfo->count = 12;
        uinfo->value.integer.max = 0x7FFFFFFF;
    }else if(strcmp(uinfo->id.name, ADEC_AC4_AUTO_ADTYPE)==0){
    }else if(strcmp(uinfo->id.name, ADEC_AC4_AUTO_PRIORITIZE_ADTYPE)==0){
    }else if(strcmp(uinfo->id.name, ADEC_AC4_DIALOGENHANCEGAIN)==0){
    }else if(strcmp(uinfo->id.name, ADEC_AC4_AUTO_ADMIXING)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC_AC4_PRES_GROUP_IDX)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC_DOLBY_OTTMODE)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC0_TP_BUFFERSTATUS)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC1_TP_BUFFERSTATUS)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC2_TP_BUFFERSTATUS)==0){
        uinfo->count = 2;
    }else if(strcmp(uinfo->id.name, ADEC3_TP_BUFFERSTATUS)==0){
        uinfo->count = 2;
    }
    return 0;
}

void fill_adec_get_struct_1v(UINT32 *item, UINT32 *adec_array)
{
    int i;
    for(i = 0; i < ADEC_MAX_CAP; i++) {
        item[(i)*2]     = i;
        item[(i)*2 + 1] = adec_array[i];
    }
    for(; i < ADEC_TABLE_MAX; i++) {
        item[(i)*2]     = 0xFF;
        item[(i)*2 + 1] = 0xFF;
    }
}

void fill_adec_get_struct_2v(UINT32 *item, UINT32 *adec_array1, UINT32 *adec_array2)
{
    int i;
    for(i = 0; i < ADEC_MAX_CAP; i++) {
        item[(i)*3]     = i;
        item[(i)*3 + 1] = adec_array1[i];
        item[(i)*3 + 2] = adec_array2[i];
    }
    for(; i < ADEC_TABLE_MAX; i++) {
        item[(i)*3]     = 0xFF;
        item[(i)*3 + 1] = 0xFF;
        item[(i)*3 + 2] = 0xFF;
    }
}

int snd_mars_ctl_adec_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
    struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
    unsigned long flags;
    int ret = OK;

    UINT32 *item = ucontrol->value.enumerated.item;
    memset(item, 0, sizeof(UINT32)*128);

    spin_lock_irqsave(&mars->mixer_lock, flags);
    if(strcmp(ucontrol->id.name, ADEC_OPEN)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.open_status);
    }else if(strcmp(ucontrol->id.name, ADEC_CLOSE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.close_status);
    }else if(strcmp(ucontrol->id.name, ADEC_CONNECT)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.src_type);
    }else if(strcmp(ucontrol->id.name, ADEC_DISCONNECT)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.src_type);
    }else if(strcmp(ucontrol->id.name, ADEC_CODEC)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.codec_type);
    }else if(strcmp(ucontrol->id.name, ADEC_START)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.start_status);
    }else if(strcmp(ucontrol->id.name, ADEC_STOP)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.stop_status);
    }else if(strcmp(ucontrol->id.name, ADEC0_INFO)==0){
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC0, (HAL_AUDIO_ADEC_INFO_T*)item);
        spin_lock_irqsave(&mars->mixer_lock, flags);
    }else if(strcmp(ucontrol->id.name, ADEC1_INFO)==0){
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC1, (HAL_AUDIO_ADEC_INFO_T*)item);
        spin_lock_irqsave(&mars->mixer_lock, flags);
    }else if(strcmp(ucontrol->id.name, ADEC2_INFO)==0){
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC2, (HAL_AUDIO_ADEC_INFO_T*)item);
        spin_lock_irqsave(&mars->mixer_lock, flags);
    }else if(strcmp(ucontrol->id.name, ADEC3_INFO)==0){
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_GetAdecStatus(HAL_AUDIO_ADEC3, (HAL_AUDIO_ADEC_INFO_T*)item);
        spin_lock_irqsave(&mars->mixer_lock, flags);
    }else if(strcmp(ucontrol->id.name, ADEC_MAXCAPACITY)==0){
        item[0] = ADEC_MAX_CAP;
        item[1] = AMIXER_MAX_CAP;
    }else if(strcmp(ucontrol->id.name, ADEC_USERMAXCAPACITY)==0){
        item[0] = adec_cmd_info.user_adec_max_cap;
        item[1] = adec_cmd_info.user_amixer_max_cap;
    }else if(strcmp(ucontrol->id.name, ADEC_SYNCMODE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.bOnOff_sync_mode);
    }else if(strcmp(ucontrol->id.name, ADEC_DOLBYDRCMODE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.drcMode);
    }else if(strcmp(ucontrol->id.name, ADEC_DOWNMIXMODE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.downmixMode);
    }else if(strcmp(ucontrol->id.name, ADEC_TRICKMODE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.eTrickMode);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO1STLANG)==0){
        fill_adec_get_struct_2v(item, adec_cmd_info.ac4_lang_code_type, adec_cmd_info.firstLang);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO2NDLANG)==0){
        fill_adec_get_struct_2v(item, adec_cmd_info.ac4_lang_code_type, adec_cmd_info.secondLang);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_ADTYPE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.ac4_ad_type);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_PRIORITIZE_ADTYPE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.bIsEnable);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_ADMIXING)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.bOnOff_AC4_ADMixing);
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_PRES_GROUP_IDX)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.adec_ac4_pres_group_idx);
    }else if(strcmp(ucontrol->id.name, ADEC_TP_AUDIODESCRIPTION)==0){
        item[0] = adec_cmd_info.AD_adecIndex;
        item[1] = adec_cmd_info.bOnOff_AD;
    }else if(strcmp(ucontrol->id.name, ADEC_TP_DECODER_OUTPUTMODE)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.dualmono_mode);
    }else if(strcmp(ucontrol->id.name, ADEC0_TP_PTS)==0){
        unsigned int audio_pts;
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC0, &audio_pts);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        if(ret == OK){
            item[0] = audio_pts;
        } else{
            ALSA_ERROR("[ALSA] %s return fail\n",ucontrol->id.name);
        }
    }else if(strcmp(ucontrol->id.name, ADEC1_TP_PTS)==0){
        unsigned int audio_pts;
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC1, &audio_pts);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        if(ret == OK){
            item[0] = audio_pts;
        } else{
            ALSA_ERROR("[ALSA] %s return fail\n",ucontrol->id.name);
        }
    }else if(strcmp(ucontrol->id.name, ADEC2_TP_PTS)==0){
        unsigned int audio_pts;
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC2, &audio_pts);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        if(ret == OK){
            item[0] = audio_pts;
        } else{
            ALSA_ERROR("[ALSA] %s return fail\n",ucontrol->id.name);
        }
    }else if(strcmp(ucontrol->id.name, ADEC3_TP_PTS)==0){
        unsigned int audio_pts;
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_TP_GetAudioPTS(HAL_AUDIO_ADEC3, &audio_pts);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        if(ret == OK){
            item[0] = audio_pts;
        } else{
            ALSA_ERROR("[ALSA] %s return fail\n",ucontrol->id.name);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_DIALOGENHANCEGAIN)==0){
        fill_adec_get_struct_1v(item, adec_cmd_info.dialEnhanceGain);
    }else if(strcmp(ucontrol->id.name, ADEC_DOLBY_OTTMODE)==0){
        item[0] = adec_cmd_info.bIsOTTEnable;
        item[1] = adec_cmd_info.bIsATMOSLockingEnable;
    }else if(strcmp(ucontrol->id.name, ADEC0_TP_BUFFERSTATUS)==0){
        HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC0, &item[0], &item[1]);
    }else if(strcmp(ucontrol->id.name, ADEC1_TP_BUFFERSTATUS)==0){
        HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC1, &item[0], &item[1]);
    }else if(strcmp(ucontrol->id.name, ADEC2_TP_BUFFERSTATUS)==0){
        HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC2, &item[0], &item[1]);
    }else if(strcmp(ucontrol->id.name, ADEC3_TP_BUFFERSTATUS)==0){
        HAL_AUDIO_TP_GetBufferStatus(HAL_AUDIO_ADEC3, &item[0], &item[1]);
    }
    spin_unlock_irqrestore(&mars->mixer_lock, flags);
    /*ALSA_DEBUG_INFO("[ALSA] get name %s item0 %d item1 %d\n",*/
            /*ucontrol->id.name, item[0], item[1]);*/
    return 0;
}

int snd_mars_ctl_adec_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
    int ret = NOT_OK, port;

    ALSA_DEBUG_INFO("[ALSA] put name \033[31m%s\033[0m item0 %d item1 %d\n", ucontrol->id.name,
                                        ucontrol->value.enumerated.item[0],
                                        ucontrol->value.enumerated.item[1]);
    if(strcmp(ucontrol->id.name, ADEC_OPEN)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0]);
        port = ucontrol->value.enumerated.item[0];
        ret = HAL_AUDIO_ADEC_Open(port);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.open_status[port] = true;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_CLOSE)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0]);
        port = ucontrol->value.enumerated.item[0];
        ret = HAL_AUDIO_ADEC_Close(port);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.close_status[port] = TRUE;
            adec_cmd_info.b_set_codec = FALSE;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
        switch(adec_cmd_info.src_type[port]){
        case ADEC_SRC_ATP0:
            HAL_AUDIO_TP_Close(HAL_AUDIO_TP0);
            break;
        case ADEC_SRC_ATP1:
            HAL_AUDIO_TP_Close(HAL_AUDIO_TP1);
            break;
        case ADEC_SRC_ADC:
            break;
        case ADEC_SRC_AAD:
            break;
        case ADEC_SRC_HDMI_PORT0:
            HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI0);
            break;
        case ADEC_SRC_HDMI_PORT1:
            HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI1);
            break;
        case ADEC_SRC_HDMI_PORT2:
            HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI2);
            break;
        case ADEC_SRC_HDMI_PORT3:
            HAL_AUDIO_HDMI_ClosePort(HAL_AUDIO_HDMI3);
            break;
        case ADEC_SRC_HDMI_DEFAULT:
            HAL_AUDIO_HDMI_Close();
            break;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_CONNECT)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d src_type %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        port = ucontrol->value.enumerated.item[0];
        switch(ucontrol->value.enumerated.item[1]){
        case ADEC_SRC_ATP0:
            HAL_AUDIO_TP_Open(HAL_AUDIO_TP0);
            ret = HAL_AUDIO_TP_Connect(HAL_AUDIO_RESOURCE_ATP0, HAL_AUDIO_RESOURCE_SDEC0, (HAL_AUDIO_ADEC_INDEX_T)port);
            break;
        case ADEC_SRC_ATP1:
            HAL_AUDIO_TP_Open(HAL_AUDIO_TP1);
            ret = HAL_AUDIO_TP_Connect(HAL_AUDIO_RESOURCE_ATP1, HAL_AUDIO_RESOURCE_SDEC1, (HAL_AUDIO_ADEC_INDEX_T)port);
            break;
        case ADEC_SRC_ADC:
            ret = OK;
            break;
        case ADEC_SRC_AAD:
            ret = OK;
            break;
        case ADEC_SRC_HDMI_PORT0:
            HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI0);
            HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI0);
            ret = OK;
            break;
        case ADEC_SRC_HDMI_PORT1:
            HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI1);
            HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI1);
            ret = OK;
            break;
        case ADEC_SRC_HDMI_PORT2:
            HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI2);
            HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI2);
            ret = OK;
            break;
        case ADEC_SRC_HDMI_PORT3:
            HAL_AUDIO_HDMI_OpenPort(HAL_AUDIO_HDMI3);
            HAL_AUDIO_HDMI_ConnectPort(HAL_AUDIO_HDMI3);
            ret = OK;
            break;
        case ADEC_SRC_HDMI_DEFAULT:
            HAL_AUDIO_HDMI_Open();
            HAL_AUDIO_HDMI_Connect();
            ret = OK;
            break;
        }
        ret |= HAL_AUDIO_ADEC_Connect(port, ucontrol->value.enumerated.item[1]);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.src_type[port] = ucontrol->value.enumerated.item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_DISCONNECT)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0]);
        port = ucontrol->value.enumerated.item[0];
        ret = HAL_AUDIO_ADEC_Disconnect(port, adec_cmd_info.src_type[port]);
        switch(adec_cmd_info.src_type[port]){
        case ADEC_SRC_ATP0:
            HAL_AUDIO_TP_Disconnect(HAL_AUDIO_RESOURCE_ATP0, HAL_AUDIO_RESOURCE_SDEC0, (HAL_AUDIO_ADEC_INDEX_T)port);
            break;
        case ADEC_SRC_ATP1:
            HAL_AUDIO_TP_Disconnect(HAL_AUDIO_RESOURCE_ATP1, HAL_AUDIO_RESOURCE_SDEC1, (HAL_AUDIO_ADEC_INDEX_T)port);
            break;
        case ADEC_SRC_ADC:
            break;
        case ADEC_SRC_AAD:
            break;
        case ADEC_SRC_HDMI_PORT0:
            HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI0);
            break;
        case ADEC_SRC_HDMI_PORT1:
            HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI1);
            break;
        case ADEC_SRC_HDMI_PORT2:
            HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI2);
            break;
        case ADEC_SRC_HDMI_PORT3:
            HAL_AUDIO_HDMI_DisconnectPort(HAL_AUDIO_HDMI3);
            break;
        case ADEC_SRC_HDMI_DEFAULT:
            HAL_AUDIO_HDMI_Disconnect();
            break;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_CODEC)==0){
        unsigned int codec_type;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d codec %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        port = ucontrol->value.enumerated.item[0];
        codec_type = ucontrol->value.enumerated.item[1];
        ret = HAL_AUDIO_ADEC_CODEC(port, codec_type);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.codec_type[port] = codec_type;
            adec_cmd_info.b_set_codec = TRUE;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_START)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        port = ucontrol->value.enumerated.item[0];
        if(adec_cmd_info.b_set_codec == TRUE){
            if(adec_cmd_info.src_type[port] == ADEC_SRC_ATP0 || adec_cmd_info.src_type[port] == ADEC_SRC_ATP1){
                ret = HAL_AUDIO_SETUP_ATP(port);
                if(ret == OK)
                    ret |= HAL_AUDIO_StartDecoding(port, adec_cmd_info.codec_type[port]);
            } else {
                ret = HAL_AUDIO_StartDecoding(port, adec_cmd_info.codec_type[port]);
            }
        } else
            ALSA_INFO("[ALSA-ADEC] b_set_codec %d, can not start\n",adec_cmd_info.b_set_codec);
        if(ret == OK)
            adec_cmd_info.start_status[port] = TRUE;
    }else if(strcmp(ucontrol->id.name, ADEC_STOP)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        port = ucontrol->value.enumerated.item[0];
        ret = HAL_AUDIO_StopDecoding(port);
        if(ret == OK)
            adec_cmd_info.stop_status[port] = TRUE;
    }else if(strcmp(ucontrol->id.name, ADEC_USERMAXCAPACITY)==0){
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m adec_max_cap %d amixer_max_cap %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        if((ucontrol->value.enumerated.item[0] > HAL_AUDIO_ADEC_MAX)
                || (ucontrol->value.enumerated.item[1] > HAL_AUDIO_MIXER_MAX)){
            ret = NOT_OK;
        }
        else{
            ret = OK;
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.user_adec_max_cap   = ucontrol->value.enumerated.item[0];
            adec_cmd_info.user_amixer_max_cap = ucontrol->value.enumerated.item[1];
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_SYNCMODE)==0){
        unsigned int bOnOff_sync_mode;
        port             = ucontrol->value.enumerated.item[0];
        bOnOff_sync_mode = ucontrol->value.enumerated.item[1];
        ret = HAL_AUDIO_SetSyncMode(port, bOnOff_sync_mode);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.bOnOff_sync_mode[port] = bOnOff_sync_mode;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_DOLBYDRCMODE)==0){
        unsigned int drcMode;
        port    = ucontrol->value.enumerated.item[0];
        drcMode = ucontrol->value.enumerated.item[1];
        ret = HAL_AUDIO_SetDolbyDRCMode(port, drcMode);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.drcMode[port] = drcMode;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_DOWNMIXMODE)==0){
        unsigned int downmixMode;
        port        = ucontrol->value.enumerated.item[0];
        downmixMode = ucontrol->value.enumerated.item[1];
        ret = HAL_AUDIO_SetDownMixMode(port, downmixMode);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.downmixMode[port] = downmixMode;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_TRICKMODE)==0){
        unsigned int eTrickMode;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d mode %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        port       = ucontrol->value.enumerated.item[0];
        eTrickMode = ucontrol->value.enumerated.item[1];
        ret = HAL_AUDIO_SetTrickMode(port, eTrickMode);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.eTrickMode[port] = eTrickMode;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO1STLANG)==0){
        adec_ac4_lang_code_ext_type_t ac4_lang_code_type;
        unsigned int firstLang;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d lang_code_type %d firstLang %x\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1],
				ucontrol->value.enumerated.item[2]);
        port               = ucontrol->value.enumerated.item[0];
        ac4_lang_code_type = ucontrol->value.enumerated.item[1];
        firstLang          = ucontrol->value.enumerated.item[2];
        ret = HAL_AUDIO_AC4_SetAutoPresentationFirstLanguage(
                port, ac4_lang_code_type, firstLang);
        if(ret == OK){
            spin_lock_irqsave(&mars->mixer_lock, flags);
            adec_cmd_info.ac4_lang_code_type[port] = ac4_lang_code_type;
            adec_cmd_info.firstLang[port]          = firstLang;
            spin_unlock_irqrestore(&mars->mixer_lock, flags);
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO2NDLANG)==0){
        adec_ac4_lang_code_ext_type_t ac4_lang_code_type;
        unsigned int secondLang;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d lang_code_type %d secondLang %x\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1],
				ucontrol->value.enumerated.item[2]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port               = ucontrol->value.enumerated.item[0];
        ac4_lang_code_type = ucontrol->value.enumerated.item[1];
        secondLang         = ucontrol->value.enumerated.item[2];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_AC4_SetAutoPresentationSecondLanguage(
                port, ac4_lang_code_type, secondLang);
        if(ret == OK){
            adec_cmd_info.ac4_lang_code_type[port] = ac4_lang_code_type;
            adec_cmd_info.secondLang[port]         = secondLang;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_ADTYPE)==0){
        adec_ac4_ad_ext_type_t ac4_ad_type;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d ad_type %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port        = ucontrol->value.enumerated.item[0];
        ac4_ad_type = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_AC4_SetAutoPresentationADType(port, ac4_ad_type);
        if(ret == OK){
            adec_cmd_info.ac4_ad_type[port] = ac4_ad_type;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_PRIORITIZE_ADTYPE)==0){
        unsigned int bIsEnable;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d bIsEnable %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port      = ucontrol->value.enumerated.item[0];
        bIsEnable = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_AC4_SetAutoPresentationPrioritizeADType(port, bIsEnable);
        if(ret == OK){
            adec_cmd_info.bIsEnable[port] = bIsEnable;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_TP_AUDIODESCRIPTION)==0){
        unsigned int bOnOff_AD;
        unsigned int AD_adecIndex;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m AD_adecIndex %d bOnOff_AD %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        AD_adecIndex = ucontrol->value.enumerated.item[0];
        bOnOff_AD    = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if(AUDIO_CHECK_ISVALID(AD_adecIndex, HAL_AUDIO_ADEC0, HAL_AUDIO_ADEC1)){
            HAL_AUDIO_ADEC_INDEX_T main_adecIndex = HAL_AUDIO_ADEC1;
            if(AD_adecIndex == HAL_AUDIO_ADEC0) main_adecIndex = HAL_AUDIO_ADEC1;
            else if(AD_adecIndex == HAL_AUDIO_ADEC1) main_adecIndex = HAL_AUDIO_ADEC0;
            HAL_AUDIO_SetMainAudioOutput(main_adecIndex);
            ret = HAL_AUDIO_TP_SetAudioDescriptionMain(main_adecIndex, bOnOff_AD);
        }
        if(ret == OK){
            adec_cmd_info.AD_adecIndex = AD_adecIndex;
            adec_cmd_info.bOnOff_AD    = bOnOff_AD;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_TP_DECODER_OUTPUTMODE)==0){
        adec_dualmono_mode_ext_type_t dualmono_mode;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d dualmono_mode %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port          = ucontrol->value.enumerated.item[0];
        dualmono_mode = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_SetDualMonoOutMode(port, dualmono_mode);
        if(ret == OK){
            adec_cmd_info.dualmono_mode[port] = dualmono_mode;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_DIALOGENHANCEGAIN)==0){
        unsigned int dialEnhanceGain;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d dialEnhanceGain %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port            = ucontrol->value.enumerated.item[0];
        dialEnhanceGain = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_AC4_SetDialogueEnhancementGain(port, dialEnhanceGain);
        if(ret == OK){
            adec_cmd_info.dialEnhanceGain[port] = dialEnhanceGain;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_AUTO_ADMIXING)==0){
        unsigned int bOnOff_AC4_ADMixing;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d bOnOff_AC4_ADMixing %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        if(!(AUDIO_CHECK_ISBOOLEAN(ucontrol->value.enumerated.item[1])))
            return NOT_OK;
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port                = ucontrol->value.enumerated.item[0];
        bOnOff_AC4_ADMixing = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        if(adec_cmd_info.ac4_ad_type[port] == ADEC_AC4_AD_TYPE_NONE && bOnOff_AC4_ADMixing == TRUE)
            return NOT_OK;
        ret = HAL_AUDIO_AC4_SetAutoPresentationADMixing(port, bOnOff_AC4_ADMixing);
        if(ret == OK){
            adec_cmd_info.bOnOff_AC4_ADMixing[port] = bOnOff_AC4_ADMixing;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_AC4_PRES_GROUP_IDX)==0){
        unsigned int adec_ac4_pres_group_idx;
		ALSA_INFO("[ALSA-ADEC] put name \033[31m%s\033[0m port %d pres_group_idx %d\n", ucontrol->id.name,
				ucontrol->value.enumerated.item[0],
				ucontrol->value.enumerated.item[1]);
        if(!AUDIO_CHECK_ISVALID(ucontrol->value.enumerated.item[1],0,0x20))
            return NOT_OK;
        spin_lock_irqsave(&mars->mixer_lock, flags);
        port                    = ucontrol->value.enumerated.item[0];
        adec_ac4_pres_group_idx = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_AC4_SetPresentationGroupIndex(port, adec_ac4_pres_group_idx);
        if(ret == OK){
            adec_cmd_info.adec_ac4_pres_group_idx[port] = adec_ac4_pres_group_idx;
        }
    }else if(strcmp(ucontrol->id.name, ADEC_DOLBY_OTTMODE)==0){
        unsigned int bIsOTTEnable;
        unsigned int bIsATMOSLockingEnable;
        spin_lock_irqsave(&mars->mixer_lock, flags);
        bIsOTTEnable          = ucontrol->value.enumerated.item[0];
        bIsATMOSLockingEnable = ucontrol->value.enumerated.item[1];
        spin_unlock_irqrestore(&mars->mixer_lock, flags);
        ret = HAL_AUDIO_SetDolbyOTTMode(bIsOTTEnable, bIsATMOSLockingEnable);
        if(ret == OK){
            adec_cmd_info.bIsOTTEnable          = bIsOTTEnable;
            adec_cmd_info.bIsATMOSLockingEnable = bIsATMOSLockingEnable;
        }
    }
    return ret;
}

static int snd_mars_volume_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 100;
	uinfo->value.integer.max = 509;
	return 0;
}

static int snd_mars_volume_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int addr = kcontrol->private_value;
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);
	spin_lock_irqsave(&mars->mixer_lock, flags);
	ucontrol->value.integer.value[0] = mars->ao_flash_volume[addr];
	spin_unlock_irqrestore(&mars->mixer_lock, flags);
	return 0;
}

static int snd_mars_volume_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int addr, volume;
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	addr = kcontrol->private_value;
	volume = ucontrol->value.integer.value[0];

	/* check volume gain is correct */
	if (volume < 100)
		volume = 100;
	if (volume > 509)
		volume = 509;

	ALSA_DEBUG_INFO("[ALSA] volume(%d) %d\n", addr, volume);

	if ((addr >= MIXER_ADDR_0) && (addr <= MIXER_ADDR_7)) {
		spin_lock_irqsave(&mars->mixer_lock, flags);
		mars->ao_flash_change[addr] = (mars->ao_flash_volume[addr] != volume);
		mars->ao_flash_volume[addr] = volume;
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		schedule_work(&mars->work_volume);
		return mars->ao_flash_change[addr];
	}

	return 0;
}

int snd_mars_ctl_aenc_info(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_info *uinfo)
{
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);
	if(strcmp(uinfo->id.name, AENC_INFO)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 3; // AENC INFO has 3 parameters
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = AENC_BIT_320K; // max value of 3 parameters for AENC INFO
	} else if(strcmp(uinfo->id.name, AENC_VOLUME)==0){
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1; // Aenc Volume has 1 parameter
		uinfo->value.integer.min = 0;
		uinfo->value.integer.max = GAIN_MAX_VALUE;
	} else if(strcmp(uinfo->id.name, AENC_PTS)==0){

	}
	return 0;
}

int snd_mars_ctl_aenc_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	if(strcmp(ucontrol->id.name, AENC_INFO)==0){
		ucontrol->value.enumerated.item[0] = aenc_cmd_info.decoder_idx;
		ucontrol->value.enumerated.item[1] = aenc_cmd_info.codec;
		ucontrol->value.enumerated.item[2] = aenc_cmd_info.bitrate;
	} else if(strcmp(ucontrol->id.name, AENC_VOLUME)==0){
		ucontrol->value.enumerated.item[0] = aenc_cmd_info.gain;
	} else if(strcmp(ucontrol->id.name, AENC_PTS)==0){

	}
	return 0;
}

int snd_mars_ctl_aenc_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = NOT_OK;
	ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);

	if(strcmp(ucontrol->id.name, AENC_INFO)==0){
		HAL_AUDIO_AENC_ENCODING_FORMAT_T codec = HAL_AUDIO_AENC_ENCODE_AAC;
		spin_lock_irqsave(&mars->mixer_lock, flags);
		aenc_cmd_info.decoder_idx = ucontrol->value.enumerated.item[0];
		aenc_cmd_info.codec       = ucontrol->value.enumerated.item[1];
		aenc_cmd_info.bitrate     = ucontrol->value.enumerated.item[2];
		switch(aenc_cmd_info.codec)
		{
			case AENC_ENCODE_MP3:
				codec = HAL_AUDIO_AENC_ENCODE_MP3;
				break;
			case AENC_ENCODE_AAC:
				codec = HAL_AUDIO_AENC_ENCODE_AAC;
				break;
			case AENC_ENCODE_PCM:
				codec = HAL_AUDIO_AENC_ENCODE_PCM;
				break;
			default:
				break;
		}
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
		HAL_AUDIO_AENC_SetCodec(HAL_AUDIO_AENC0, codec);
		ALSA_DEBUG_INFO("[ALSA] %s put, decoder_idx:%d codec:%d bitrate:%d\n", AENC_INFO, aenc_cmd_info.decoder_idx, aenc_cmd_info.codec, aenc_cmd_info.bitrate);
	} else if(strcmp(ucontrol->id.name, AENC_VOLUME)==0){
		unsigned int gain, rtk_gain;
		HAL_AUDIO_VOLUME_T volume;
		gain = ucontrol->value.enumerated.item[0];
		rtk_gain = adecVolume_search(gain);
		volume.mainVol = (rtk_gain >> BITS_PER_BYTE) & 0xff;
		volume.fineVol = rtk_gain & 0xff;
		ALSA_INFO("[ALSA] %s put, volume: 0x%x, main: 0x%x, fine: 0x%x\n", AENC_VOLUME, gain, volume.mainVol, volume.fineVol);
		ret = HAL_AUDIO_AENC_SetVolume(HAL_AUDIO_AENC0, volume, gain);
		if(ret == OK){
			spin_lock_irqsave(&mars->mixer_lock, flags);
			aenc_cmd_info.volume.mainVol = (rtk_gain >> BITS_PER_BYTE) & 0xff;
			aenc_cmd_info.volume.fineVol = rtk_gain & 0xff;
			aenc_cmd_info.gain = gain;
			spin_unlock_irqrestore(&mars->mixer_lock, flags);
		}
	} else if(strcmp(ucontrol->id.name, AENC_PTS)==0){

	}
	return 0;
}

int snd_mars_ctl_adc_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	/*ALSA_DEBUG_INFO("[ALSA] %s\n", __func__);*/
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	if(strcmp(uinfo->id.name, ADC_OPEN)==0){
	} else if(strcmp(uinfo->id.name, ADC_CLOSE)==0){
	} else if(strcmp(uinfo->id.name, ADC_CONNECT)==0){
		uinfo->value.integer.max = 0xFF;
	} else if(strcmp(uinfo->id.name, ADC_DISCONNECT)==0){
	}
	return 0;
}

int snd_mars_ctl_adc_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	UINT32 *item = ucontrol->value.enumerated.item;

	spin_lock_irqsave(&mars->mixer_lock, flags);
	if(strcmp(ucontrol->id.name, ADC_OPEN)==0){
		item[0] = adc_cmd_info.open_status;
		ALSA_DEBUG_INFO("[ALSA-ADC] get name \033[31m%s\033[0m status %d\n",ucontrol->id.name, adc_cmd_info.open_status);
	} else if(strcmp(ucontrol->id.name, ADC_CLOSE)==0){
		item[0] = adc_cmd_info.close_status;
		ALSA_DEBUG_INFO("[ALSA-ADC] get name \033[31m%s\033[0m status %d\n",ucontrol->id.name, adc_cmd_info.close_status);
	} else if(strcmp(ucontrol->id.name, ADC_CONNECT)==0){
		item[0] = adc_cmd_info.connected_port;
		ALSA_DEBUG_INFO("[ALSA-ADC] get name \033[31m%s\033[0m port %d\n",ucontrol->id.name, adc_cmd_info.connected_port);
	} else if(strcmp(ucontrol->id.name, ADC_DISCONNECT)==0){
		item[0] = adc_cmd_info.disconnect_status;
		ALSA_DEBUG_INFO("[ALSA-ADC] get name \033[31m%s\033[0m status %d\n",ucontrol->id.name, adc_cmd_info.disconnect_status);
	}
	spin_unlock_irqrestore(&mars->mixer_lock, flags);

	return 0;
}

int snd_mars_ctl_adc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_card_mars *mars = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int ret = NOT_OK;

	if(strcmp(ucontrol->id.name, ADC_OPEN)==0){
		ALSA_INFO("[ALSA-ADC] put name \033[31m%s\033[0m \n", ucontrol->id.name);
		ret = HAL_AUDIO_ADC_Open();
		spin_lock_irqsave(&mars->mixer_lock, flags);
		if(ret == OK)
			adc_cmd_info.open_status = TRUE;
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
	} else if(strcmp(ucontrol->id.name, ADC_CLOSE)==0){
		ALSA_INFO("[ALSA-ADC] put name \033[31m%s\033[0m \n", ucontrol->id.name);
		ret = HAL_AUDIO_ADC_Close();
		if(ret == OK){
			adc_cmd_info.close_status   = TRUE;
			adc_cmd_info.connected_port = 0xFF;
		}
	} else if(strcmp(ucontrol->id.name, ADC_CONNECT)==0){
		ALSA_INFO("[ALSA-ADC] put name \033[31m%s\033[0m item0 %d\n", ucontrol->id.name,
								ucontrol->value.enumerated.item[0]);
		if (ucontrol->value.enumerated.item[0] == 0xFF){
			ALSA_DEBUG_INFO("[ALSA] ADC connect port %d -> virtual port\n", ucontrol->value.enumerated.item[0]);
			ret = HAL_AUDIO_ADC_Connect(0);
		} else
			ret = HAL_AUDIO_ADC_Connect(ucontrol->value.enumerated.item[0]);
		spin_lock_irqsave(&mars->mixer_lock, flags);
		if(ret == OK)
			adc_cmd_info.connected_port = ucontrol->value.enumerated.item[0];
		spin_unlock_irqrestore(&mars->mixer_lock, flags);
	} else if(strcmp(ucontrol->id.name, ADC_DISCONNECT)==0){
		ALSA_INFO("[ALSA-ADC] put name \033[31m%s\033[0m item0 %d\n", ucontrol->id.name,
								ucontrol->value.enumerated.item[0]);
		if (adc_cmd_info.connected_port == 0xFF){
			ALSA_DEBUG_INFO("[ALSA] ADC disconnect port %d -> virtual port\n", adc_cmd_info.connected_port);
			ret = HAL_AUDIO_ADC_Disconnect(0);
		} else
			ret = HAL_AUDIO_ADC_Disconnect(adc_cmd_info.connected_port);
		if(ret == OK)
			adc_cmd_info.disconnect_status = TRUE;
	}
	return ret;
}

static struct snd_kcontrol_new snd_mars_controls[] = {
	MARS_VOLUME("AMIXER0", 0, MIXER_ADDR_0),
	MARS_VOLUME("AMIXER1", 0, MIXER_ADDR_1),
	MARS_VOLUME("AMIXER2", 0, MIXER_ADDR_2),
	MARS_VOLUME("AMIXER3", 0, MIXER_ADDR_3),
	MARS_VOLUME("AMIXER4", 0, MIXER_ADDR_4),
	MARS_VOLUME("AMIXER5", 0, MIXER_ADDR_5),
	MARS_VOLUME("AMIXER6", 0, MIXER_ADDR_6),
	MARS_VOLUME("AMIXER7", 0, MIXER_ADDR_7),
};

static int __init rtk_snd_new_mixer(struct snd_card_mars *mars)
{
	struct snd_card *card = mars->card;
	unsigned int idx, i;
	int err;

	spin_lock_init(&mars->mixer_lock);
	INIT_WORK(&mars->work_volume, rtk_snd_playback_volume_work);

	strcpy(card->mixername, "Mars Mixer");

	for (idx = 0; idx < ARRAY_SIZE(snd_mars_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_adec_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_adec_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_sndout_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_sndout_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_mute_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_mute_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_gain_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_gain_controls[idx], mars));
		if (err < 0)
			return err;
	}
	if(!b_gain_get_init){
		fill_get_struct_1v(gain_cmd_info.input_gain, TRUE);
		for(i = 0; i < INDEX_SNDOUT_MAX; i++)
			gain_cmd_info.output_gain[i] = ALSA_GAIN_0dB;
		b_gain_get_init = TRUE;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_delay_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_delay_controls[idx], mars));
		if (err < 0)
			return err;
	}
	if(!b_delay_get_init){
		fill_get_struct_1v(dly_cmd_info.dly_status, FALSE);
		for(i = 0; i < INDEX_SNDOUT_MAX; i++)
			dly_cmd_info.dly_status[DLY_OUTPUT_BASE + i] = 0;
		b_delay_get_init = TRUE;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_hdmi_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_hdmi_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_direct_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_direct_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_lgse_SPK_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_lgse_SPK_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_lgse_SPK_BTSUR_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_lgse_SPK_BTSUR_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_lgse_BT_BTSUR_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_lgse_BT_BTSUR_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_sif_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_sif_controls[idx], mars));
		if (err < 0)
			return err;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_aenc_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_aenc_controls[idx], mars));
		if (err < 0)
			return err;
		// init aenc volume 0db here.
		aenc_cmd_info.volume.mainVol = 0x7F;
		aenc_cmd_info.volume.fineVol = 0x0;
	}
	for (idx = 0; idx < ARRAY_SIZE(snd_mars_adc_controls); idx++) {
		err = snd_ctl_add(card,
			snd_ctl_new1(&snd_mars_adc_controls[idx], mars));
		if (err < 0)
			return err;
		adc_cmd_info.connected_port = 0xFF;
	}

	return 0;
}

static int rtk_snd_probe(struct platform_device *devptr)
{
	struct snd_card *card;
	struct snd_card_mars *mars;
	int idx, err;
	int dev;

	dev = devptr->id;

	if (!enable[dev])
		return -ENODEV;

	err = snd_card_new(&devptr->dev, index[dev], id[dev],
			THIS_MODULE, sizeof(struct snd_card_mars), &card);
	if (err < 0)
		return err;
	mars = (struct snd_card_mars *)card->private_data;
	mars->card = card;
	mutex_init(&mars->rpc_lock);

	if (get_cap == 0) {
		err = rtk_snd_check_audio_fw_capability(card);

		if (err < 0)
			return err;

		get_cap = 1;
	}

	for (idx = 0; idx < SNDRV_CARDS && idx < pcm_devs[dev]; idx++) {
		if (pcm_substreams[dev] < 1)
			pcm_substreams[dev] = 1;
		if (pcm_substreams[dev] > MAX_PCM_SUBSTREAMS)
			pcm_substreams[dev] = MAX_PCM_SUBSTREAMS;

		err = rtk_snd_pcm(mars, idx, pcm_substreams[dev], dev);

		if (err < 0)
			goto __nodev;
	}

	err = rtk_snd_new_mixer(mars);
	if (err < 0)
		goto __nodev;

	strcpy(card->driver, "Mars");
	strcpy(card->shortname, "Mars");
	sprintf(card->longname, "Mars %i", dev + 1);

	err = snd_card_register(card);
	if (err == 0) {
		platform_set_drvdata(devptr, card);
		return 0;
	}

__nodev:
	snd_card_free(platform_get_drvdata(devptr));
	return err;
}

static int rtk_snd_remove(struct platform_device *devptr)
{
	snd_card_free(platform_get_drvdata(devptr));
	platform_set_drvdata(devptr, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rtk_snd_pm_suspend(struct device *dev)
{
	ALSA_DEBUG_INFO("[ALSA] rtk_snd_pm_suspend!!!\n");
	return 0;
}

static int rtk_snd_pm_resume(struct device *dev)
{
	ALSA_DEBUG_INFO("[ALSA] rtk_snd_pm_resume!!!\n");
	return 0;
}

static SIMPLE_DEV_PM_OPS(rtk_snd_pm, rtk_snd_pm_suspend, rtk_snd_pm_resume);
#define RTK_SND_OPS	&rtk_snd_pm
#else
#define RTK_SND_OPS	NULL
#endif

#define RTK_SND_DRIVER	"rtk_snd"

static struct platform_driver rtk_snd_driver = {
	.probe		= rtk_snd_probe,
	.remove		= rtk_snd_remove,
	.driver		= {
		.name	= RTK_SND_DRIVER,
		.owner	= THIS_MODULE,
		.pm	= RTK_SND_OPS,
	},
};

static int alsa_card_mars_init(void)
{
	int dev, cards, err;
	int ret = 0;
	struct platform_device *device;
	int j, gain;
	HAL_AUDIO_VOLUME_T volume;

#ifdef USE_FIXED_AO_PINID
	int i;

	for (i = 0; i < MAX_PCM_DEVICES; i++) {
		used_ao_pin[i] = 0;
		flush_error[i] = 0;
		pause_error[i] = 0;
		close_error[i] = 0;
		release_error[i] = 0;
	}
#endif

	for(i = 0; i < 2; i++) {
		es_codec[i] = DIRECT_CODEC_MP3;
		es_fs[i] = 48000;
		es_nch[i] = 2;
		es_bps[i] = 16;
	}

	snd_open_count = 0;
	snd_open_ai_count = 0;
	alsa_agent = -1;

	err = platform_driver_register(&rtk_snd_driver);
	if (err < 0)
		return err;

	for (dev = cards = 0; dev < SNDRV_CARDS && enable[dev]; dev++) {
		device = platform_device_register_simple(RTK_SND_DRIVER,
							 dev, NULL, 0);

		if (IS_ERR(device))
			continue;
		if (!platform_get_drvdata(device)) {
			platform_device_unregister(device);
			continue;
		}
		devices[dev] = device;

		cards++;
	}

	if (!cards)
		ret = -ENODEV;

    //init capture device mute/vol
    //init capture device mute/vol
    volume.mainVol = 0x00;
    volume.fineVol = 0x00;
    for(j=0; j<4; j++)
    {
        g_capture_dev_mute[j] = 1;
        if(j == 0)//index 0 is deafult volume
        {
            g_capture_dev_vol[j].mainVol = 0x7f;
            g_capture_dev_vol[j].fineVol = 0x00;
        }
        else//reset each capture device volume as minimum value in initial state
        {
            g_capture_dev_vol[j]  = volume;
        }
    }

    /* for KTASKWBS-10539, when disconnect need 0dB and unmute */
    g_capture_dev_mute[0] = 0;

	return ret;
}

static void __exit alsa_card_mars_exit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(devices); ++i)
		platform_device_unregister(devices[i]);
	platform_driver_unregister(&rtk_snd_driver);
}

late_initcall(alsa_card_mars_init);
module_exit(alsa_card_mars_exit);
