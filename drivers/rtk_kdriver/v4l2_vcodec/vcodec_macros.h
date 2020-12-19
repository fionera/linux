#ifndef __VCODEC_MACROS_H__
#define __VCODEC_MACROS_H__

#define IF_ERR_GOTO(errno, label) \
	if (errno) { \
		pr_err("%s:%d: ret %d\n", __func__, __LINE__, errno); \
		goto label; \
	}

#define IF_ERR_GOTO_VA(errno, label, fmt, ...) \
	if (errno) { \
		pr_err("%s:%d: ret %d ("fmt")\n", __func__, __LINE__, errno, ##__VA_ARGS__); \
		goto label; \
	}

#define IF_ERR_RET(errno) \
	if (errno) { \
		pr_err("%s:%d: ret %d\n", __func__, __LINE__, errno); \
		return errno; \
	}

#define IF_ERR_RET_VA(errno, fmt, ...) \
	if (errno) { \
		pr_err("%s:%d: ret %d ("fmt")\n", __func__, __LINE__, errno, ##__VA_ARGS__); \
		return errno; \
	}

#define RET_ERR(errno) \
	{ \
		pr_err("%s:%d: ret %d\n", __func__, __LINE__, errno); \
		return errno; \
	}

#define RET_ERR_VA(errno, fmt, ...) \
	{ \
		pr_err("%s:%d: ret %d ("fmt")\n", __func__, __LINE__, errno, ##__VA_ARGS__); \
		return errno; \
	}

#define TO_SIGNED(en, idx) ((en)? (idx):-1)

#endif
