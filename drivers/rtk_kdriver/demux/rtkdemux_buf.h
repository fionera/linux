#ifndef _DEMUX_BUF_H_
#define _DEMUX_BUF_H_


void DEMUX_BUF_ResetRBHeader(DEMUX_BUF_T *pBufH, DEMUX_BUF_T *pBuf, RINGBUFFER_TYPE type, unsigned char reverse);
int DEMUX_BUF_AllocBuffer(DEMUX_BUF_T *pBuf, int size, int isCache, int useZone);
int DEMUX_BUF_FreeBuffer(DEMUX_BUF_T *pBuf);
int DEMUX_BUF_AllocFromPoll(DEMUX_BUF_T *pBuf, int size, DEMUX_BUF_T *pSrcBuf, int isCache);
void DEMUX_Buf_SetPinInfo(DEMUX_BUF_PIN_T *pPinInfo, RINGBUFFER_HEADER *pRBH, DEMUX_BUF_T *pBuf);

//video
void DEMUX_BUF_InitVTP(void);
int DEMUX_BUF_PrepareVideoBuffer(demux_channel *pCh, DEMUX_PES_DEST_T dest);
int DEMUX_BUF_ReturnVideoBuffer(demux_channel *pCh, int port);
int DEMUX_BUF_UpdatePinInfo(demux_channel *pCh, char bVideoPin);

//audio
#ifdef DEMUX_AUDIO_USE_GLOBAL_BUFFER
void DEMUX_BUF_InitAudioBuffer(void);
int DEMUX_BUF_RequestAudioBuffer(demux_channel *pCh, int pin);
int DEMUX_BUF_ReleaseAudioBuffer(demux_channel *pCh, int pin);
int DEMUX_BUF_UpdateChannelAudioPinStatus(demux_channel *pCh);
#endif

#ifdef DEMUX_PES_BUFFER_DYNAMIC_ALLOC
int DEMUX_PESBuffer_Alloc(DEMUX_PES_MAP_T *pMap);
void DEMUX_PESBuffer_UpdateBuferStatus(demux_channel *pCh);
void DEMUX_PESBuffer_UpdateBuferStatus(demux_channel *pCh);
#endif 

int DEMUX_BUF_AllocateTPBuffer(demux_channel * pCh);
int DEMUX_BUF_FreeTPBuffer(demux_channel *pCh);
int DEMUX_BUF_ConfigTPBuffer(demux_channel *pCh);
int  DEMUX_BUF_MTPBuffer_Create(demux_channel *pCh);
int DEMUX_BUF_MTPBuffer_Destroy(demux_channel *pCh);
#endif