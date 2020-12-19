#ifndef _SND_REALTEK_H_
#define _SND_REALTEK_H_

#define S_OK		0x10000000
#define RPCCMD_CONFIG_USB_AUDIO_IN	0

struct RPC_RBUF_HEADER {
	int instanceID;
	int pin_id;
	int rbuf_list[8]; /* ringbuffer list */
	int rd_idx; /* read index */
	int listsize;
};

/* pin define for AI */
#define PB_OUTPIN PCM_OUT
#define TS_OUTPIN PCM_OUT2
#define ALSA_OUTPIN	BASE_BS_OUT

#define CONFIG_WISA

/* Captured format */
enum AUDIO_FORMAT_OF_CAPTURED_SEND_TO_ALSA {
	AUDIO_ALSA_FORMAT_16BITS_BE_PCM = 0,
	AUDIO_ALSA_FORMAT_24BITS_BE_PCM = 1,
	AUDIO_ALSA_FORMAT_16BITS_LE_LPCM = 2,
	AUDIO_ALSA_FORMAT_24BITS_LE_LPCM = 3,
};

/* Ring Buffer header is the shared memory structure for arm use */
struct RBUF_HEADER_ARM {
	unsigned long magic;
	unsigned long beginAddr;
	unsigned long size;

	unsigned long bufferID;

	unsigned long writePtr;
	unsigned long numOfReadPtr;
	unsigned long reserve2;
	unsigned long reserve3;

	unsigned long readPtr[4];

	long file_offset;
	long req_file_offset;
	long file_size;

	long b_seekable;
};
#endif
