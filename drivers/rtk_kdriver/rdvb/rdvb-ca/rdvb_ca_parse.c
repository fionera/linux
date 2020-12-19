#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include "rdvb_ca_parse.h"

BOOLEAN m_CI_CovertVersionToBit(SINT32 nNum,UINT32 flag,UINT32* pu32CiPlusVersion)
{
    BOOLEAN bRet = FALSE;
    INT32 i;
    if((nNum<=0)||(nNum > MAX_SUPPORT_VERSION))
    {
        *pu32CiPlusVersion = 0;
        return bRet;
    }

    nNum-=1; // if num == 1 then return 00000001b

    switch (flag)
    {
        case '+':
            *pu32CiPlusVersion = (1<<(nNum*4));
            bRet = TRUE;
        break;

        case '-':
            *pu32CiPlusVersion &= ~(1<<(nNum*4));
            bRet = TRUE;
        break;

        case '*':
            for(i=0;i<=nNum;i++)
                *pu32CiPlusVersion |= (1<<(i*4));
            bRet = TRUE;
        break;

        default:
            *pu32CiPlusVersion = 0;
        break;
    }

    return bRet;

}


void CI_HandleCiplusItem(CI_TEXT_T* pstIdentity, UINT32 flag, BOOLEAN* pbCiPlusEnabled, UINT32* pu32CiPlusVersion)
{
    SINT32 s32Number = 0;
    UINT32 i;

    for (i = 0; i < pstIdentity->u32Len; i++)
    {
        if (pstIdentity->pu8Data[i] >= '0' && pstIdentity->pu8Data[i] <= '9')
        {
            s32Number *= 10;
            s32Number += pstIdentity->pu8Data[i] - '0';
        }
        else
        {
            s32Number = -1;
            break;
        }
    }

    #if 1 // for k3l new version present
    *pbCiPlusEnabled = m_CI_CovertVersionToBit(s32Number,flag,pu32CiPlusVersion);

    printk(KERN_ERR "version : enable =%d, sNum=%d, flag=%d,CiPlusVersion =0x%x \n",*pbCiPlusEnabled,s32Number ,flag, *pu32CiPlusVersion);
    #else
    if ((flag == '-') && (s32Number == 1))
    {
        *pbCiPlusEnabled = FALSE;
    }
    else if ((flag == '+') && (s32Number == 1))
    {
        *pu32CiPlusVersion = (UINT32)s32Number;
        *pbCiPlusEnabled = TRUE;
    }
    else if ((flag == '*') && (s32Number >= 1))
    {
        *pu32CiPlusVersion = (UINT32)s32Number;
        *pbCiPlusEnabled = TRUE;
    }
    #endif
}

void CI_HandleCiprofItem(CI_TEXT_T* pstIdentity, UINT32 flag, UINT32* pu32CiPlusProfile)
{
    BOOLEAN bValid = TRUE;
    UINT32 u32Number = 0;
    UINT32 i;


    if ((pstIdentity->u32Len >= 2) && (pstIdentity->pu8Data[0] == '0') && (pstIdentity->pu8Data[1] == 'x'))
    {
        /* Hexadecimal prefix */
        for (i = 2; (bValid) && (i < pstIdentity->u32Len); i++)
        {
            if (pstIdentity->pu8Data[i] >= '0' && pstIdentity->pu8Data[i] <= '9')
            {
                u32Number *= 16;
                u32Number += pstIdentity->pu8Data[i] - '0';
            }
            else if (pstIdentity->pu8Data[i] >= 'a' && pstIdentity->pu8Data[i] <= 'f')
            {
                u32Number *= 16;
                u32Number += pstIdentity->pu8Data[i] + 10 - 'a';
            }
            else
            {
                bValid = FALSE;
            }
        }
    }
    else
    {
        /* Decimal expected */
        for (i = 0; (bValid) && (i < pstIdentity->u32Len); i++)
        {
            if (pstIdentity->pu8Data[i] >= '0' && pstIdentity->pu8Data[i] <= '9')
            {
                u32Number *= 10;
                u32Number += pstIdentity->pu8Data[i] - '0';
            }
            else
            {
                bValid = FALSE;
            }
        }
    }

    if (bValid)
    {
        *pu32CiPlusProfile = u32Number;
    }
}

void CI_HandleCompatibilityItem(CI_TEXT_T* pstLabel, CI_TEXT_T* pstIdentity,
                                UINT32 flag, BOOLEAN* pbCiPlusEnabled,
                                UINT32* pu32CiPlusVersion, UINT32* pu32CiPlusProfile)
{
    if ((pstLabel == NULL) || (pstIdentity == NULL) || (pbCiPlusEnabled == NULL) ||
        (pu32CiPlusVersion == NULL) || (pu32CiPlusProfile == NULL))
    {
        return;
    }
    if ((pstLabel->u32Len == 6) && (strncmp((char*)pstLabel->pu8Data, "ciplus", 6) == 0))
    {
        CI_HandleCiplusItem(pstIdentity, flag, pbCiPlusEnabled, pu32CiPlusVersion);
    }
    else if ((pstLabel->u32Len == 6) && (strncmp((char*)pstLabel->pu8Data, "ciprof", 6) == 0))
    {
        CI_HandleCiprofItem(pstIdentity, flag, pu32CiPlusProfile);
    }
}


UINT8* CI_GetNextText(UINT8* pu8Data, CI_TEXT_T* pstText)
{

    static char *single = "$[]=+-* ";
    static char *word =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789._";
    char *p;
    int len;

    if ((pu8Data == NULL) || (pstText == NULL))
    {
        return NULL;
    }

    pstText->pu8Data = pu8Data;
    if (*pu8Data == '\0')
    {
      pstText->eText = E_TEXT_UNKNOWN;
      pstText->u32Len = 1;
      pu8Data = NULL;
    }
    else
    {
        p = strchr(single, *pu8Data);
        if (p != NULL)
        {
            /* single character pstText */
            pstText->eText = (EN_TEXT_T)*pu8Data;
            pstText->u32Len = 1;
            pu8Data++;
        }
        else if (strncmp((char*)pu8Data, "compatible", 10) == 0)
        {
            /* "compatible" */
            pstText->eText = E_TEXT_COMPATIBLE;
            pstText->u32Len = 10;
            pu8Data += pstText->u32Len;
        }
        else
        {
            len = strspn((char*)pu8Data, word);
            if (len > 0)
            {
                pstText->eText = E_TEXT_WORD;
                pstText->u32Len = len;
                pu8Data += pstText->u32Len;
            }
            else
            {
                pstText->eText = E_TEXT_UNKNOWN;
                pstText->u32Len = 1;
                pu8Data++;
            }
        }
    }

    return pu8Data;

}


void CI_ParseVer1Line(CI_CAM_INFO_T* pstCAM, UINT8* pu8Buff)
{
    CI_TEXT_T stToken;
    CI_TEXT_T stLabel;
    CI_TEXT_T stIdentity;
    EN_STATE_T eState = E_STATE_START;

    char flag = '+';
    char *p = (char*)pu8Buff;
    BOOLEAN bCiPlusEnable = FALSE;
    UINT32 u32CiPlusProfile = 0;
    UINT32 u32CiPlusVersion = 0;

    if ((pstCAM == NULL) || (pu8Buff == NULL))
    {
        return;
    }

    /* convert to lowercase */
    while (*p++)
    {
        if (*p >= 'A' && *p <= 'Z')
        {
            *p += ('a' - 'A');
        }
    }

    pu8Buff = CI_GetNextText(pu8Buff, &stToken);
    while (pu8Buff != NULL)
    {
        switch (eState)
        {
            case E_STATE_START:
                if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_DOLLAR:
                bCiPlusEnable = FALSE;
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_COMPATIBLE)
                {
                    eState = E_STATE_COMPATIBLE;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_COMPATIBLE:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_LBRACKET)
                {
                    eState = E_STATE_TERM;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_TERM:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_WORD)
                {
                    stLabel = stToken;
                    eState = E_STATE_LABEL;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_LABEL:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_EQUAL)
                {
                    flag = '+';
                    eState = E_STATE_EQUAL;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_EQUAL:
                eState = E_STATE_START;
                if ((stToken.eText == E_TEXT_MINUS) || (stToken.eText == E_TEXT_PLUS) || (stToken.eText == E_TEXT_STAR))
                {
                    flag = stToken.eText;
                    eState = E_STATE_FLAG;
                }
                else if (stToken.eText == E_TEXT_WORD)
                {
                    stIdentity = stToken;
                    eState = E_STATE_IDENTITY;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_FLAG:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_WORD)
                {
                    stIdentity = stToken;
                    eState = E_STATE_IDENTITY;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_IDENTITY:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_SPACE)
                {
                    CI_HandleCompatibilityItem(&stLabel, &stIdentity, flag, &bCiPlusEnable, &u32CiPlusVersion, &u32CiPlusProfile);
                    eState = E_STATE_TERM;
                }
                else if (stToken.eText == E_TEXT_RBRACKET)
                {
                    CI_HandleCompatibilityItem(&stLabel, &stIdentity, flag, &bCiPlusEnable, &u32CiPlusVersion, &u32CiPlusProfile);
                    eState = E_STATE_END;
                }
                else if (stToken.eText == E_TEXT_DOLLAR)
                {
                    eState = E_STATE_DOLLAR;
                }
                break;
            case E_STATE_END:
                eState = E_STATE_START;
                if (stToken.eText == E_TEXT_DOLLAR)
                {
                    pstCAM->stCIS.bCiPlusEnable = bCiPlusEnable;
                    if (bCiPlusEnable)
                    {
                        pstCAM->stCIS.u32CiPlusVersion = u32CiPlusVersion;
                        pstCAM->stCIS.u32CiPlusProfile = u32CiPlusProfile;
                    }
                }
                break;
        }
        pu8Buff = CI_GetNextText(pu8Buff, &stToken);
    }

}


BOOLEAN CI_ParseCistplVer1(CI_CAM_INFO_T* pstCAM, UINT8* pu8Buff, UINT8 u8Size)
{
#define TPLLV1_MAJOR_VALUE      0x05
#define TPLLV1_MINOR_VALUE      0X00

#define TPLLV1_MAJOR        0
#define TPLLV1_MINOR        1
#define TPLLV1_INFO         2

    UINT8 i;
    UINT8* pu8End = NULL;
    if ((pstCAM == NULL) || (pu8Buff == NULL) || (u8Size < TPLLV1_INFO))
    {
        printk("Invalid parameter, pstCAM: %p, pu8Buff: %p, u8Size: %d.\n", pstCAM, pu8Buff, u8Size);
        return FALSE;
    }
    if (pu8Buff[TPLLV1_MAJOR] != TPLLV1_MAJOR_VALUE)
    {
        printk("Invalid TPLLV1_MAJOR_VALUE: 0x%02x != 0x05.\n", pu8Buff[TPLLV1_MAJOR]);
        return FALSE;
    }
    if (pu8Buff[TPLLV1_MINOR] != TPLLV1_MINOR_VALUE)
    {
        printk("Invalid TPLLV1_MINOR_VALUE: 0x%02x != 0x00.\n", pu8Buff[TPLLV1_MINOR]);
        return FALSE;
    }
    if (u8Size > TPLLV1_INFO)
    {
        pu8Buff += TPLLV1_INFO;
        u8Size -= TPLLV1_INFO;
        for (i=0; ((i<4) && (u8Size>0)); i++)
        {
            pu8End = (UINT8*)memchr((void*)pu8Buff, '\0', u8Size);
            if (pu8End != NULL)
            {
                u8Size -= (pu8End - pu8Buff + 1);
                if (i >= 2)
                {
                    CI_ParseVer1Line(pstCAM, pu8Buff);
                }
                pu8Buff = pu8End + 1;
            }
            else
            {
                u8Size = 0;
                pu8Buff = NULL;
            }
        }
    }
    pstCAM->stCIS.bFoundVers1 = TRUE;
    return TRUE;
}


BOOLEAN CI_ParseCistplCftableEntry(CI_CAM_INFO_T* pstCAM, UINT8* pu8Buff, UINT8 u8Size)
{
#define TPCE_INDEX      0
#define TPCE_IF         1
#define TPCE_FS         2

#define STCE_EV         0xC0
#define STCE_PD         0xC1

    BOOLEAN bValid = TRUE;
    UINT8 u8Addr = 0;
    UINT8 u8PowerCount = 0;
    UINT8 u8PowerSelectType = 0;
    UINT8 u8Number = 0;
    UINT8 u8Value;
    UINT8 i, j;
    UINT8 u8StplCode;
    UINT8 u8StplLen;
    BOOLEAN bGetSTCE_EV = FALSE;
    BOOLEAN bGetSTCE_PD = FALSE;


    if ((pstCAM == NULL) || (pu8Buff == NULL) || (u8Size == 0))
    {
        printk("Invalid parameter, pstCAM: %p, pu8Buff: %p, u8Size: %d.\n", pstCAM, pu8Buff, u8Size);
        return FALSE;
    }

    if (pstCAM->stCIS.bFoundCFtableEntry != TRUE)
    {
        pstCAM->stCIS.u8CfgEntry = pu8Buff[TPCE_INDEX] & 0x3F;
        if ((pu8Buff[TPCE_INDEX] & 0xC0) == 0xC0)
        {
            if ((pu8Buff[TPCE_IF] & 0x0F) != 0x04)
            {
                //Interface type != Custom Interface 0, as EN 50221 page 77, we don't parsing this tuple.
                bValid = FALSE;
                printk("Invalid Interface type %d != 0x04.\n", (pu8Buff[TPCE_IF] & 0x0F));
            }
        }
        else
        {
            bValid = FALSE;
            printk("IntFace or Default is not set: 0x%x.\n", pu8Buff[TPCE_INDEX]);
        }

        if (bValid)
        {
            //Check IO Space and Power configuration entry.
            if (((pu8Buff[TPCE_FS] & 0x08) == 0) || ((pu8Buff[TPCE_FS] & 0x03) == 0))
            {
                bValid = FALSE;
                printk("IO Space or Power configuration entry is not set: 0x%x.\n", pu8Buff[TPCE_FS]);
            }
        }

        if (bValid)
        {
            //Skip Power Description Structure (TPCE_PD)
            u8Addr = TPCE_FS + 1;
            u8PowerCount = (pu8Buff[TPCE_FS] & 0x03);
            for (i=0; i<u8PowerCount; i++)
            {
                u8PowerSelectType = pu8Buff[u8Addr];
                u8Addr++;
                u8Number = 0;
                for (j=0; j<8; j++)
                {
                    if (u8PowerSelectType & (1 << j))
                    {
                        u8Number++;
                    }
                }

                while (u8Number > 0)
                {
                    do
                    {
                        //Check EXT bit. If EXT bit set, should have another byte for this paramater.
                        u8Value = pu8Buff[u8Addr] & 0x80;
                        u8Addr++;
                    } while (u8Value != 0);
                    u8Number--;
                }
            }
        }

        if ((bValid) && (u8Addr < u8Size))
        {
            //Skip Cofiguration Timing Information (TPCE_TD)
            if (pu8Buff[TPCE_FS] & 0x04)
            {
                u8Number = 0;
                if ((pu8Buff[u8Addr] & 0x03) != 0x03)
                {
                    u8Number++;
                }
                if (((pu8Buff[u8Addr] >> 2) & 0x07) != 0x07)
                {
                    u8Number++;
                }
                if (((pu8Buff[u8Addr] >> 5) & 0x07) != 0x07)
                {
                    u8Number++;
                }
                u8Addr++;

                while (u8Number > 0)
                {
                    do
                    {
                        //Check EXT bit. If EXT bit set, should have another byte for this paramater.
                        u8Value = pu8Buff[u8Addr] & 0x80;
                        u8Addr++;
                    } while (u8Value != 0);
                    u8Number--;
                }
            }


            if (pu8Buff[u8Addr] == 0x22)
            {
                u8Addr++;
            }
            else
            {
                bValid = FALSE;
                printk("Invalid TPCE_IO value: 0x%x != 0x22.\n", pu8Buff[u8Addr]);
            }
        }
        else if (u8Addr >= u8Size)
        {
            bValid = FALSE;
            printk("Address %d out of range %d.\n", u8Addr, u8Size);
        }


        if ((bValid) && (u8Addr < u8Size))
        {
            //Skip Interrupt Request Description Structure (TPCE_IR)
            if (pu8Buff[TPCE_FS] & 0x10)
            {
                if ((pu8Buff[u8Addr] & 0x10) != 0)
                {
                    u8Addr += 2;
                }
                else
                {
                    if (pu8Buff[u8Addr] & 0x20)
                    {
                        pstCAM->stCIS.bIreqEnable = TRUE;
                    }
                }
                u8Addr++;
            }
        }
        else if (u8Addr >= u8Size)
        {
            bValid = FALSE;
            printk("Address %d out of range %d.\n", u8Addr, u8Size);
        }




        if ((bValid) && (u8Addr < u8Size))
        {
            //Skip Memory Space Description structure (TPCE_MS)
            if ((pu8Buff[TPCE_FS] & 0x60) == 0x20)
            {
                u8Addr += 2;
            }
            else if ((pu8Buff[TPCE_FS] & 0x60) == 0x40)
            {
                u8Addr += 4;
            }
            else if ((pu8Buff[TPCE_FS] & 0x60) == 0x60)
            {
                //Parsing Memory space descriptor
                u8Value = pu8Buff[u8Addr];
                u8Addr++;

                u8Number = ((u8Value >> 5) & 0x3);
                if (u8Value & 0x80)
                {
                    u8Number += u8Number;
                }
                u8Number += ((u8Value >> 3) & 0x3);
                u8Number = ((u8Value & 0x7)+1)*u8Number;
                u8Addr += u8Number;
            }

        }
        else if (u8Addr >= u8Size)
        {
            bValid = FALSE;
            printk("Address %d out of range %d.\n", u8Addr, u8Size);
        }


        if ((bValid) && (u8Addr < u8Size))
        {
            if (pu8Buff[TPCE_FS] & 0x80)
            {
                do
                {
                    //Check EXT bit. If EXT bit set, should have another byte for this paramater.
                    u8Value = pu8Buff[u8Addr] & 0x80;
                    u8Addr++;
                } while (u8Value != 0);
            }
        }
        else if (u8Addr >= u8Size)
        {
            bValid = FALSE;
            printk("Address %d out of range %d.\n", u8Addr, u8Size);
        }

        //Parsing SubTuple
        while (u8Addr < u8Size)
        {
            u8StplCode = pu8Buff[u8Addr];
            if (u8StplCode == 0xFF)
            {
                bValid = FALSE;
                break;
            }
            u8Addr++;
            u8StplLen = pu8Buff[u8Addr];
            u8Addr++;
            if (u8StplCode == STCE_EV)
            {
                if (strncmp((char*)&pu8Buff[u8Addr], "DVB_HOST", 8) == 0)
                {
                    bGetSTCE_EV = TRUE;
                }
            }
            else if (u8StplCode == STCE_PD)
            {
                if (strncmp((char*)&pu8Buff[u8Addr], "DVB_CI_MODULE", 13) == 0)
                {
                    bGetSTCE_PD = TRUE;
                }
            }
            u8Addr += u8StplLen;
        }
    }

    if (bValid && bGetSTCE_EV && bGetSTCE_PD)
    {
        pstCAM->stCIS.bFoundCFtableEntry = TRUE;
    }

    return TRUE; //bValid;
}

