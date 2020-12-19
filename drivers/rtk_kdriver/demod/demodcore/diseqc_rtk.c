#include <mach/io.h>
#include "tv_osal.h"
#include "foundation.h"
#include "diseqc_rtk.h"
#include "dvbsx_demod_rtk_a.h"
#include <rtk_kdriver/rtk_crt.h>


unsigned int DvbS2_Keep_22k_on_off = 0;
extern struct RtkDemodDvbSxBsSpecialParams gDvbSxBsSpecialParams;

int
realtek_Diseqc_SetDiseqcContinue22kOnOff(
	unsigned int Diseqc22kOnOff
)
{
	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqcContinue22kOnOff (%u)\n" "\033[m", Diseqc22kOnOff);
	DvbS2_Keep_22k_on_off = Diseqc22kOnOff;

	if (!(rtd_inl(SYS_REG_SYS_CLKEN2_reg) & SYS_REG_SYS_CLKEN2_clken_dtv_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV DEMOD clk\n");
		CRT_CLK_OnOff(DTVDEMOD, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_atb_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV ATB clk\n");
		CRT_CLK_OnOff(DTVATB, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_diseqc_mask))
	{
		REALTEK_DISEQC_INFO("Enable DISEQC clk\n");
		CRT_CLK_OnOff(DISEQC, CLK_ON, NULL);
	}

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	
	rtd_maskl(0xb815E004, ~(0x80000000), 0x80000000);//En loop back mode
	
	#ifdef CONFIG_CUSTOMER_TV006
	rtd_maskl(0xb815E000, ~(0x40000000), 0x40000000);//Tx envelop mode
	#else
	rtd_maskl(0xb815E000, ~(0x40000000), 0x00000000);//Tx pulse mode
	#endif
	
	rtd_maskl(0xb815E004, ~(0x0000c000), 0x0000c000);//Continue Tone Mode
	
	//rtd_maskl(0xb815E004, ~(0x000003ff), 0x0000027f);//22k freq divider
	
	tv_osal_msleep(1); 
	
	if(Diseqc22kOnOff == 1)
	{
		rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go
	}
	else
	{
		rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go
		rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	}
	
	return FUNCTION_SUCCESS;
}

int
realtek_Diseqc_SetDiseqcCmd(
	unsigned int DataLength,
	unsigned char *Data
)
{
	int DataCount;

	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqcCmd\n" "\033[m");

	if(*(Data+0) == 0xe0 && *(Data+1) == 0x10 && *(Data+2) == 0x5a)
	{
		REALTEK_DISEQC_INFO("\033[1;32;35m" "Unicable v1 Enabled\n" "\033[m");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 1;
	}
	else if(*(Data+0) == 0x70 || *(Data+0) == 0x71)
	{
		REALTEK_DISEQC_INFO("\033[1;32;35m" "Unicable v2 Enabled\n" "\033[m");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 1;
	}
	else
	{
		REALTEK_DISEQC_INFO("\033[1;32;35m" "Unicable Disabled\n" "\033[m");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 0;
	}

	if (!(rtd_inl(SYS_REG_SYS_CLKEN2_reg) & SYS_REG_SYS_CLKEN2_clken_dtv_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV DEMOD clk\n");
		CRT_CLK_OnOff(DTVDEMOD, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_atb_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV ATB clk\n");
		CRT_CLK_OnOff(DTVATB, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_diseqc_mask))
	{
		REALTEK_DISEQC_INFO("Enable DISEQC clk\n");
		CRT_CLK_OnOff(DISEQC, CLK_ON, NULL);
	}

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	
	rtd_maskl(0xb815E004, ~(0x80000000), 0x80000000);//En loop back mode

	#ifdef CONFIG_CUSTOMER_TV006
	rtd_maskl(0xb815E000, ~(0x40000000), 0x40000000);//Tx envelop mode
	#else
	rtd_maskl(0xb815E000, ~(0x40000000), 0x00000000);//Tx pulse mode
	#endif

	rtd_maskl(0xb815E004, ~(0x0000c000), 0x00008000);//Diseqc Cmd Mode

	//rtd_maskl(0xb815E004, ~(0x000003ff), 0x0000027f);//22k freq divider

	rtd_maskl(0xb815E004, ~(0x00010000), 0x00010000);//Odd parity

	rtd_maskl(0xb815E004, ~(0x00003800), ((DataLength-1)<<11));//Set Data Length(0:1Byte, 1:2Byte, ...)

	for(DataCount=0; DataCount<DataLength; DataCount++)
	{
		rtd_outl((0xb815E01C+(4*DataCount)), *(Data+DataCount));
	}

	REALTEK_DISEQC_INFO("Diseqc cmd data (%u) %x %x %x %x %x %x %x %x\n", DataLength, rtd_inl(0xb815E01C), rtd_inl(0xb815E020), rtd_inl(0xb815E024), rtd_inl(0xb815E028), rtd_inl(0xb815E02c), rtd_inl(0xb815E030), rtd_inl(0xb815E034), rtd_inl(0xb815E038));

	tv_osal_msleep(70);

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go

	return FUNCTION_SUCCESS;
}

int
realtek_Diseqc_SetDiseqc2p0Cmd(
	unsigned int *DataLength,
	unsigned char *Data
)
{
	int DataCount = 0;
	unsigned char RxRecDone1 = 0, RxRecDone2 = 0, RxDataCnt = 0;
	unsigned long stime = 0;
	
	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqc2p0Cmd\n" "\033[m");
	
	if(*(Data+0) == 0xe0 && *(Data+1) == 0x10 && *(Data+2) == 0x5a)
	{
		REALTEK_DISEQC_INFO("\033[1;32;35m" "Unicable v1 Enabled\n" "\033[m");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 1;
	}
	else if(*(Data+0) == 0x70 || *(Data+0) == 0x71)
	{
		REALTEK_DISEQC_INFO("\033[1;32;35m" "Unicable v2 Enabled\n" "\033[m");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 1;
	}
	else
	{
		REALTEK_DISEQC_INFO("Unicable Disabled\n");
		gDvbSxBsSpecialParams.u8UnicableEnabled = 0;
	}

	if (!(rtd_inl(SYS_REG_SYS_CLKEN2_reg) & SYS_REG_SYS_CLKEN2_clken_dtv_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV DEMOD clk\n");
		CRT_CLK_OnOff(DTVDEMOD, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_atb_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV ATB clk\n");
		CRT_CLK_OnOff(DTVATB, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_diseqc_mask))
	{
		REALTEK_DISEQC_INFO("Enable DISEQC clk\n");
		CRT_CLK_OnOff(DISEQC, CLK_ON, NULL);
	}

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	
	rtd_maskl(0xb815E004, ~(0x80000000), 0x80000000);//En loop back mode
	
	#ifdef CONFIG_CUSTOMER_TV006
	rtd_maskl(0xb815E000, ~(0x40000000), 0x40000000);//Tx envelop mode
	#else
	rtd_maskl(0xb815E000, ~(0x40000000), 0x00000000);//Tx pulse mode
	#endif
	
	rtd_maskl(0xb815E004, ~(0x0000c000), 0x00008000);//Diseqc Cmd Mode
	
	//rtd_maskl(0xb815E004, ~(0x000003ff), 0x0000027f);//22k freq divider
	
	rtd_maskl(0xb815E004, ~(0x00010000), 0x00010000);//Odd parity
	
	rtd_maskl(0xb815E004, ~(0x00003800), ((*DataLength-1)<<11));//Set Data Length(0:1Byte, 1:2Byte, ...)
	
	for(DataCount=0; DataCount<*DataLength; DataCount++)
	{
		rtd_outl((0xb815E01C+(4*DataCount)), *(Data+DataCount));
	}
	REALTEK_DISEQC_INFO("Diseqc cmd data (%u) %x %x %x %x %x %x %x %x\n", *DataLength, rtd_inl(0xb815E01C), rtd_inl(0xb815E020), rtd_inl(0xb815E024), rtd_inl(0xb815E028), rtd_inl(0xb815E02c), rtd_inl(0xb815E030), rtd_inl(0xb815E034), rtd_inl(0xb815E038));
	
	tv_osal_msleep(70);
	
	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go
	
	tv_osal_msleep(15*(*DataLength)+20);
	
	*DataLength = 0xFFFFFFFF;
	
	//if(*(Data+0) == 0xe2 || *(Data+0) == 0xe3)
	//{
		rtd_maskl(0xb815E05C, ~(0x00000003), 0x00000001);//Sel RX DSC_IN pin
		rtd_maskl(0xb80008b0, ~(0x000000F0), 0x00000050);//DSC_IN pinmux enable
		rtd_outl(0xb801bd08, 0x80000001);//DSC_IN GPIO input sel
		REALTEK_DISEQC_INFO("\033[1;32;31m" "Diseqc RX enabled\n" "\033[m");
		
		stime = tv_osal_time();
		while((tv_osal_time() - stime) < 1000)//RX time out 1000 ms
		{
			RxRecDone1 = rtd_inl(0xB815E010);
			RxRecDone2 = rtd_inl(0xB815E014);
			if((RxRecDone2 & 0x02) == 0x02)
			{
				tv_osal_msleep(120);
				RxDataCnt = (rtd_inl(0xB815E014) & 0x78) >> 3;
				for(DataCount=0; DataCount<RxDataCnt; DataCount++)
				{
					*(Data+DataCount) = rtd_inl(0xB815E03C+(4*DataCount)) & 0xFF;
				}
				*DataLength = RxDataCnt;
				REALTEK_DISEQC_INFO("\033[1;32;33m" "%u (0x%02x) (0x%02x) RX Diseqc cmd data (%u) 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n" "\033[m", (RxRecDone2 & 0x02)>>1, RxRecDone1, RxRecDone2, *DataLength, *(Data+0), *(Data+1), *(Data+2), *(Data+3), *(Data+4), *(Data+5), *(Data+6), *(Data+7));
				break;
			}
			else
			{
				REALTEK_DISEQC_DBG("\033[1;32;34m" "%u (0x%02x) (0x%02x) Waiting....\n" "\033[m", (RxRecDone2 & 0x02)>>1, RxRecDone1, RxRecDone2);
			}
			tv_osal_msleep(5);
		}
		rtd_maskl(0xb80008b0, ~(0x000000F0), 0x000000F0);//DSC_IN pinmux disable
		rtd_outl(0xb801bd08, 0x80000000);//DSC_IN GPIO output sel
	//}

	return FUNCTION_SUCCESS;
}

int
realtek_Diseqc_SetDiseqcUnModToneBurst(
)
{
	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqcUnModToneBurst\n" "\033[m");

	if (!(rtd_inl(SYS_REG_SYS_CLKEN2_reg) & SYS_REG_SYS_CLKEN2_clken_dtv_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV DEMOD clk\n");
		CRT_CLK_OnOff(DTVDEMOD, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_atb_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV ATB clk\n");
		CRT_CLK_OnOff(DTVATB, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_diseqc_mask))
	{
		REALTEK_DISEQC_INFO("Enable DISEQC clk\n");
		CRT_CLK_OnOff(DISEQC, CLK_ON, NULL);
	}

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	
	rtd_maskl(0xb815E004, ~(0x80000000), 0x80000000);//En loop back mode
	
	#ifdef CONFIG_CUSTOMER_TV006
	rtd_maskl(0xb815E000, ~(0x40000000), 0x40000000);//Tx envelop mode
	#else
	rtd_maskl(0xb815E000, ~(0x40000000), 0x00000000);//Tx pulse mode
	#endif
	
	rtd_maskl(0xb815E004, ~(0x0000C000), 0x00004000);//Un-Modulated Tone Burst Mode
	
	//rtd_maskl(0xb815E004, ~(0x000003ff), 0x0000027f);//22k freq divider
	
	tv_osal_msleep(1); 
	
	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go
	
	return FUNCTION_SUCCESS;
}

int
realtek_Diseqc_SetDiseqcModToneBurst(
)
{
	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqcModToneBurst\n" "\033[m");

	if (!(rtd_inl(SYS_REG_SYS_CLKEN2_reg) & SYS_REG_SYS_CLKEN2_clken_dtv_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV DEMOD clk\n");
		CRT_CLK_OnOff(DTVDEMOD, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_atb_demod_mask))
	{
		REALTEK_DISEQC_INFO("Enable DTV ATB clk\n");
		CRT_CLK_OnOff(DTVATB, CLK_ON, NULL);
	}
	if (!(rtd_inl(SYS_REG_SYS_CLKEN0_reg) & SYS_REG_SYS_CLKEN0_clken_diseqc_mask))
	{
		REALTEK_DISEQC_INFO("Enable DISEQC clk\n");
		CRT_CLK_OnOff(DISEQC, CLK_ON, NULL);
	}

	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000000);//Tx Off
	
	rtd_maskl(0xb815E004, ~(0x80000000), 0x80000000);//En loop back mode
	
	#ifdef CONFIG_CUSTOMER_TV006
	rtd_maskl(0xb815E000, ~(0x40000000), 0x40000000);//Tx envelop mode
	#else
	rtd_maskl(0xb815E000, ~(0x40000000), 0x00000000);//Tx pulse mode
	#endif
	
	rtd_maskl(0xb815E004, ~(0x0000C000), 0x00000000);//Modulated Tone Burst Mode
	
	//rtd_maskl(0xb815E004, ~(0x000003ff), 0x0000027f);//22k freq divider
	
	tv_osal_msleep(1); 
	
	rtd_maskl(0xb815E004, ~(0x00000400), 0x00000400);//Tx Go
	
	return FUNCTION_SUCCESS;
}


int
realtek_Diseqc_SetDiseqcToneBurst(DISEQC_TONE_MODE mode)
{
	REALTEK_DISEQC_INFO("\033[1;32;31m" "realtek_Diseqc_SetDiseqcToneBurst (%u)\n" "\033[m", (unsigned int)mode);
	
	if(mode == DISEQC_TONE_MODE_A)
		realtek_Diseqc_SetDiseqcUnModToneBurst();
	else
		realtek_Diseqc_SetDiseqcModToneBurst();
	
	return FUNCTION_SUCCESS;
}

