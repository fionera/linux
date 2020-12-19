/* ------------------------------------------------------------------------
   scd_rtd299x.c  scd driver for Realtek Neptune/rtd299x
   -------------------------------------------------------------------------
    Copyright (C) 2019 Kevin Wang <kevin_wang@realtek.com.tw>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
----------------------------------------------------------------------------
Update List :
----------------------------------------------------------------------------
    1.0     |   20190312    |   Kevin Wang  | 1) create phase
----------------------------------------------------------------------------*/

#include "rdvb_acas_atr.h"
#include "rdvb_acas_debug.h"

/*------------------------------------------------------------------
 * Func : SC_LRC8
 * Desc : 8 bits LRC check
 * Parm : pData : data to compute LRC
 *        Len   : size of data for LRC computation
 *        InitLRC : Initial value of LRC
 * Retn : LRC value
 *------------------------------------------------------------------*/
unsigned char SC_LRC8(unsigned char* pData, unsigned char Len, unsigned char InitLRC)
{
    while(Len--)
        InitLRC ^= *(pData++);

    return InitLRC;
}

/*------------------------------------------------------------------
 * Func : SC_CheckATR
 * Desc : check is the atr_complete or not
 * Parm : pATR   : atr to check
 *        AtrLen : sizeo fo atr
 * Retn : 1 : complete, 0 : not complete
 *------------------------------------------------------------------*/
extern int SC_CheckATR(unsigned char* pATR, unsigned int AtrLen)
{
    unsigned char  TS    = 0;
    unsigned char  T0    = 0;
    unsigned char  TD1   = 0;
    unsigned char  TD2   = 0;
    unsigned char  nHistoryByte = 0;
    unsigned char  T1Len = 0;
    unsigned char  T2Len = 0;
    unsigned char  T3Len = 0;
    unsigned char  T     = 0;

    if (AtrLen < 3)
        return 0;

    //-------- header complete ---------
    TS = pATR[0];
    if ((TS!=0x3B) && (TS!=0x3F))
        return 1;

    T0 = pATR[1];
    nHistoryByte = T0 & 0xF;
    T1Len = ((T0 >> 4) & 0x01) +
            ((T0 >> 5) & 0x01) +
            ((T0 >> 6) & 0x01) +
            ((T0 >> 7) & 0x01);
    if (AtrLen <  2 + T1Len)
        return 0;

    //-------- TX1 complete ------------
    if (T0 & 0x80){
        // TD1 exists - find length of TD2
        TD1 = pATR[1 + T1Len];
        T2Len = ((TD1 >> 4) & 0x01) +
                ((TD1 >> 5) & 0x01) +
                ((TD1 >> 6) & 0x01) +
                ((TD1 >> 7) & 0x01) ;
        T  = TD1 & 0xF;
    }
    if (AtrLen <  2 + T1Len + T2Len)
        return 0;

    //-------- TX2 complete ------------
    if (TD1 & 0x80){
        // TD2 exists - find length of TD3
        TD2   = pATR[1 + T1Len + T2Len];
        T3Len = ((TD2 >> 4) & 0x01)+
                ((TD2 >> 5) & 0x01)+
                ((TD2 >> 6) & 0x01)+
                ((TD2 >> 7) & 0x01);
    }

    //-------- TX3 & header complete ------------
    switch(T){
       case 0:
       case 15:
           // for T=0 or T=15, TCK should be absent
           if (AtrLen <  2 + T1Len + T2Len + T3Len + nHistoryByte)
               return 0;

           return 1;

        default:
            if (AtrLen <  3 + T1Len + T2Len + T3Len + nHistoryByte)
                return 0;

            if (SC_LRC8(pATR+1, AtrLen-1, 0)==0)
                return 1;

            ACAS_WARNING("SC_CheckATR failed, TCK check failed\n");
            return -1;
    }
}



/*------------------------------------------------------------------
 * Func : SC_ParseATR
 *
 * Desc : Parse ATR Info
 *
 * Parm : pATR   : atr to check 
 *        AtrLen : size of atr
 *        pInfo  : atr info
 * Retn : 1 : complete, 0 : not complete
 *------------------------------------------------------------------*/
int SC_ParseATR(
    unsigned char*          pATR,
    unsigned int            AtrLen,
    SC_ATR_INFO*            pInfo
    )
{
    unsigned char id = 0;
    unsigned char i  = 0;
    int ret = SC_CheckATR(pATR, AtrLen);

    switch (ret){
        case 0:
            ACAS_WARNING("Incomplete ATR\n");
            return -1;
        case -1:
            ACAS_WARNING("ATR CRC Error\n");
            //return -1;
    }

    memset(pInfo, 0, sizeof(*pInfo));
    pInfo->TS = pATR[id++];
    pInfo->T0 = pATR[id++];

    for (i=0; i<4; i++)
        if (pInfo->T0 & (0x10 << i))
            pInfo->T1[i] = pATR[id++];

    for (i=0; i<4; i++)
        if (pInfo->T1[3] & (0x10 << i))
            pInfo->T2[i] = pATR[id++];
    for (i=0; i<4; i++)
        if (pInfo->T2[3] & (0x10 << i))
            pInfo->T3[i] = pATR[id++];

    for (i=0; i<4; i++)
 	    if (pInfo->T3[3] & (0x10 << i))
	        pInfo->T4[i] = pATR[id++];

    switch (pInfo->T1[3] & 0xF)
    {
    case 0:
    case 15:
        if ((AtrLen - id) != (pInfo->T0 & 0xF)){
            ACAS_WARNING("too much history bytes %d/%d \n", AtrLen - id, pInfo->T0 & 0xF);
            return -1;
        }
        break;
    default:
        if ((AtrLen - id -1) != (pInfo->T0 & 0xF)){
            ACAS_WARNING("too much history bytes %d/%d \n", AtrLen - id -1, pInfo->T0 & 0xF);
            return -1;
        }
    }

    pInfo->nHistoryBytes = pInfo->T0 & 0xF;
    memcpy(pInfo->History, &pATR[id], pInfo->nHistoryBytes);
    pInfo->TCK = pATR[AtrLen-1];

    return 0;
}

const int SC_FiMap[] =
{
    372, 372, 558,  744, 1116, 1488, 1860, -1,
    -1,  512, 768, 1024, 1536, 2048,   -1, -1
};

const int SC_FmaxMap[] =
{
    4000000, 5000000, 6000000,  8000000, 12000000, 16000000, 20000000, -1,
    -1,      5000000, 7500000,  1000000, 15000000, 20000000,       -1, -1
};

const int SC_DiMap[] =
{
    -1,  1,  2,  4,  8, 16, 32, -1,
    12, 20, -1, -1, -1, -1, -1, -1
};

int SC_GetFi(unsigned char fi)
{
    if (fi > 0xF)
        return SC_FAIL;

    return SC_FiMap[fi];
}

int SC_GetFmax(unsigned char fi)
{
    if (fi > 0xF)
        return SC_FAIL;

    return SC_FmaxMap[fi];
}

int SC_GetDi(unsigned char di)
{
    if (di > 0xF)
        return SC_FAIL;

    return SC_DiMap[di];
}

int SC_GetT14ETU(unsigned char TAx)
{
    switch(TAx){
    case 0x21:  return 620;
    case 0x11:  return 416;
    case 0x22:  return 310;
    case 0x23:  return 155;
    case 0xb8:  return 96;
    case 0x29:  return 31;
    case 0xff:  return 64;
    case 0x96:  return 16;
    default:
        return -1;
    }
}

int SC_SetPPS(scd *pthis, unsigned char F, unsigned char D)
{
	unsigned char PPS[4];
	unsigned char RPPS[4];
	//int ret = 0;
	PPS[0] = 0xFF;
	PPS[1] = 0x11;
	PPS[2] = ((F&0xF)<<4)|(D&0xF);
	PPS[3] = SC_LRC8(PPS, 3, 0x00);

	if(scd_xmit(pthis, PPS, 4) != SC_SUCCESS){
		ACAS_INFO("scd xmit error\n");
		return -1;
	}
        if(scd_read(pthis, RPPS, 4, 500) != SC_SUCCESS){
		ACAS_INFO("scd read error\n");
		return -1;
	}
	if (memcmp(RPPS, PPS, 4)==0){
	    ACAS_INFO("Set PPS(F=%d, D=%d) successed\n", F, D);
	    return 0;
	}

	ACAS_WARNING("Set PPS (F=%d,D=%d) failed\n", F, D);
	return -1;
}

static int crc_tabccitt_init = 0;
static unsigned short crc_tabccitt[256];
#define P_CCITT              0x1021

static void init_crcccitt_tab(void)
{
    int i, j;
    for (i=0; i<256; i++){
        unsigned short crc = 0;
        unsigned short c   = ((unsigned short) i) << 8;

        for (j=0; j<8; j++){
            if ((crc^c) & 0x8000){
                crc <<= 1;
                crc ^= P_CCITT;
            }else
                crc <<= 1;
            c <<= 1;
        }
        crc_tabccitt[i] = crc;
    }
    crc_tabccitt_init = 1;
}

/*------------------------------------------------------------------
 * Func : SC_CRC16
 *
 * Desc : 16 bits CRC check
 *
 * Parm : pData : data to compute CRC
 *        Len   : size of data for CRC computation
 *        InitCRC : Initial value of CRC
 *
 * Retn : CRC value
 *------------------------------------------------------------------*/
unsigned short SC_CRC16(
    unsigned char*          pData,
    unsigned char           Len,
    unsigned short          InitCRC
    )
{
    unsigned short crc = InitCRC;

    if (!crc_tabccitt_init)
        init_crcccitt_tab();

    while(Len--){
        unsigned short short_c  = 0x00ff & *(pData++);
        unsigned short tmp = (crc >> 8) ^ short_c;
        crc = (crc << 8) ^ crc_tabccitt[tmp];
    }
    return crc;
}


