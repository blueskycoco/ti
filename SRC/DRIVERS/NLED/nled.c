//===================================================================
//
//  Module Name:    NLED.DLL
//
//  File Name:      nleddrv.c
//
//  Description:    Control of the notification LED(s)
//
//===================================================================
// Copyright (c) 2007- 2009 BSQUARE Corporation. All rights reserved.
//===================================================================

//-----------------------------------------------------------------------------
//
// header files
//
//-----------------------------------------------------------------------------

#include "bsp.h"
#include "ceddkex.h"
#include "sdk_gpio.h"
#include "bsp_padcfg.h"
#include "sdk_padcfg.h"
#include "oalex.h"

//DWORD g_GPIOId[] = {NOTIFICATION_LED_GPIO};
DWORD g_GPIOId[] = {136,137,138,139};
DWORD g_GPIOActiveState[dimof(g_GPIOId)] = {0};
DWORD g_dwNbLeds = dimof(g_GPIOId);
BOOL g_LastLEDIsVibrator = FALSE;

static const PAD_INFO LedPinMux[] = {
	PAD_ENTRY(MMC2_DAT4, INPUT_DISABLED | PULL_RESISTOR_DISABLED | MUXMODE(4)) \
    PAD_ENTRY(MMC2_DAT5, INPUT_DISABLED | PULL_RESISTOR_DISABLED | MUXMODE(4)) \
    PAD_ENTRY(MMC2_DAT6, INPUT_DISABLED | PULL_RESISTOR_DISABLED | MUXMODE(4)) \
    PAD_ENTRY(MMC2_DAT7, INPUT_DISABLED | PULL_RESISTOR_DISABLED | MUXMODE(4)) \
    END_OF_PAD_ARRAY
    };


int NLedCpuFamily = -1;

BOOL NLedBoardInit()
{
	RETAILMSG(1,(TEXT("NLedBoardInit++\r\n")));
        if (RequestAndConfigurePadArray(LedPinMux))
        {
			RETAILMSG(1,(TEXT("NLedBoardInit OK\r\n")));
            return TRUE;
        }
        else
        {
            ERRORMSG(1,(TEXT("Unable to request PAD configuration for NLED driver\r\n")));
            return FALSE;
        }
    return FALSE;
}

BOOL NLedBoardDeinit()
{
    if( NLedCpuFamily != CPU_FAMILY_DM37XX)
    {
        ReleasePadArray(LedPinMux);
        return TRUE;
    }
    return FALSE;
}
