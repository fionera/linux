#ifndef _COMMON_H
#define _COMMON_H
/* Please check the comment for abs in include/linux/kernel.h */
#ifndef abs64
#define abs64 abs
#endif

#define MTP_BUFFER_SIZE    (6*1024*1024)

#define HAS_FLAG(flag, bit)    ((flag) & (bit))
#define SET_FLAG(flag, bit)    ((flag) |= (bit))
#define RESET_FLAG(flag, bit)  ((flag) &= (~(bit)))

enum OUTPUT_PIN_ENUM {
	VIDEO_PIN = 0,
	AUDIO_PIN = 1,
	VIDEO2_PIN = 2,
	AUDIO2_PIN = 3,
	NUMBER_OF_PINS
};

#endif /* _COMMON_H */
