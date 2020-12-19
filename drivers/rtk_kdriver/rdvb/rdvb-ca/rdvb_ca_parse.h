#ifndef __RDVB_CA_PARSE__
#define __RDVB_CA_PARSE__

#define MAX_SUPPORT_VERSION 8
#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif


#ifndef NULL
#define NULL 0
#endif

#ifndef BOOLEAN
#ifndef _EMUL_WIN
typedef	unsigned int			__BOOLEAN;
#define BOOLEAN __BOOLEAN
#else
typedef	unsigned char			__BOOLEAN;
#define BOOLEAN __BOOLEAN
#endif
#endif

#ifndef UINT8
typedef unsigned char  UINT8;
#endif
#ifndef UINT16
typedef unsigned short UINT16;
#endif
#ifndef UINT32
typedef unsigned int   UINT32;
#endif

typedef int INT32;

#ifndef SINT32
typedef signed int				__SINT32;
#define SINT32 __SINT32
#endif


#define TPK_MAX_KEY_LENGTH                        32
#define TPK_MAX_IV_LENGTH                         16  /* maximum value of aes_128_cbc  */

typedef enum
{
    E_TEXT_DOLLAR        = '$',
    E_TEXT_COMPATIBLE    = 'c',
    E_TEXT_LBRACKET      = '[',
    E_TEXT_RBRACKET      = ']',
    E_TEXT_WORD          = 'w',
    E_TEXT_EQUAL         = '=',
    E_TEXT_PLUS          = '+',
    E_TEXT_MINUS         = '-',
    E_TEXT_STAR          = '*',
    E_TEXT_SPACE         = ' ',
    E_TEXT_NULL          = '\0',
    E_TEXT_UNKNOWN       = 'u'
} EN_TEXT_T;

typedef  enum {
    E_STATE_START = 0,         //= 's',
    E_STATE_DOLLAR,             //= '$',
    E_STATE_COMPATIBLE,         //= 'c',
    E_STATE_TERM,               //= 't',
    E_STATE_LABEL,              //= 'l',
    E_STATE_EQUAL,              //= '=',
    E_STATE_FLAG,               //= 'f',
    E_STATE_IDENTITY,           //= 'i',
    E_STATE_END                //= 'e'
} EN_STATE_T;


typedef struct
{
   EN_TEXT_T eText;
   UINT8 *pu8Data;
   UINT32 u32Len;
} CI_TEXT_T;

typedef struct
{
    UINT8 u8Key[TPK_MAX_KEY_LENGTH];
    UINT8 u8IV[TPK_MAX_IV_LENGTH];
} CI_KEY_T;

typedef struct
{
    UINT8 TPL_CODE;
    UINT8 TPL_LINK;
} CI_TUPLE_T;

typedef struct
{
   UINT8 data[256];
   UINT32 size;
} CI_TUPLE_DATA_T;

typedef struct
{
    BOOLEAN bFoundVers1;
    BOOLEAN bFoundConfig;
    BOOLEAN bFoundCFtableEntry;
    BOOLEAN bFoundCOR;
    UINT32 u32CORAddr;
    UINT8 u8CfgEntry;
    BOOLEAN bCiPlusEnable;
    UINT32 u32CiPlusVersion;
    UINT32 u32CiPlusProfile;
    BOOLEAN bIreqEnable;
} CI_CIS_INFO_T;


typedef struct
{
    BOOLEAN bCamInsert;
    BOOLEAN bInitCIS;
    BOOLEAN bCISValid;
    BOOLEAN bCISFail;
	BOOLEAN bCISNeedReset;
    CI_CIS_INFO_T stCIS;
    //UINT16 u16BuffSize;
    UINT32 u32CAMRepeatCount;
    CI_TUPLE_DATA_T cistpl_ver1;
    CI_TUPLE_DATA_T cistpl_manfid;
} CI_CAM_INFO_T;

/***
**
***/
BOOLEAN CI_ParseCistplVer1(CI_CAM_INFO_T* pstCAM, UINT8* pu8Buff, UINT8 u8Size);
BOOLEAN CI_ParseCistplCftableEntry(CI_CAM_INFO_T* pstCAM, UINT8* pu8Buff, UINT8 u8Size);


#endif

