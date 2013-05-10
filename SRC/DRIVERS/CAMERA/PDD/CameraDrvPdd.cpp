// All rights reserved ADENEO EMBEDDED 2010
//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#include <windows.h>
#include <pm.h>

#include "omap3530.h"
#include "omap3530_irq.h"
#include "oal_clock.h"

#include "sdk_padcfg.h"

#include "Cs.h"
#include "Csmedia.h"

#include "CameraPDDProps.h"
#include "dstruct.h"
#include "dbgsettings.h"
#include <camera.h>
#include "CameraDriver.h"
#include "SensorFormats.h"
#include "SensorProperties.h"
#include "PinDriver.h"

#include "oal_io.h"
#include "oalex.h"
#include <pkfuncs.h>
#include "Ispreg.h"
#include "CameraPDD.h"
#include "CameraDrvPdd.h"
#include "wchar.h"
#include "util.h"
#include "params.h"


enum {
	INPUT_YPcPr = 0,
	INPUT_AV,
	INPUT_S_VIDEO
};

enum {
	ENUM_CUST_INPUTMODE
};

void* UYVYtoYUY2(WORD* pInput, BYTE* pOutput, DWORD dwNumByte);
static DWORD CameraInterruptThread(LPVOID lpParameter);

PDDFUNCTBL FuncTbl = {
    sizeof(PDDFUNCTBL),
    PDD_Init,
    PDD_DeInit,
    PDD_GetAdapterInfo,
    PDD_HandleVidProcAmpChanges,
    PDD_HandleCamControlChanges,
    PDD_HandleVideoControlCapsChanges,
    PDD_SetPowerState,
    PDD_HandleAdapterCustomProperties,
    PDD_InitSensorMode,
    PDD_DeInitSensorMode,
    PDD_SetSensorState,
    PDD_TakeStillPicture,
    PDD_GetSensorModeInfo,
    PDD_SetSensorModeFormat,
    PDD_AllocateBuffer,
    PDD_DeAllocateBuffer,
    PDD_RegisterClientBuffer,
    PDD_UnRegisterClientBuffer,
    PDD_FillBuffer,
    PDD_HandleModeCustomProperties
};


CCameraPdd::CCameraPdd()
{
    m_ulCTypes = 2;
    m_bStillCapInProgress = false;
    m_hContext = NULL;
    m_pModeVideoFormat = NULL;
    m_pModeVideoCaps = NULL;
    m_ppModeContext = NULL;
    m_pCurrentFormat = NULL;
    m_pfnTimeSetEvent = NULL;
    m_pfnTimeKillEvent = NULL;
    m_hTimerDll = NULL;
    m_dwInputMode = INPUT_YPcPr;
    m_pIspCtrl = new CIspCtrl;
    m_pTvpCtrl = new CTvpCtrl;
    m_hParent = NULL;
   
    memset( &m_TimerIdentifier, 0x0, sizeof(m_TimerIdentifier));    
    memset( &m_CsState, 0x0, sizeof(m_CsState));
    memset( &m_SensorModeInfo, 0x0, sizeof(m_SensorModeInfo));
    memset( &m_SensorProps, 0x0, sizeof(m_SensorProps));
    memset( &PowerCaps, 0x0, sizeof(PowerCaps));

}

CCameraPdd::~CCameraPdd()
{
    if( m_pfnTimeKillEvent )
    {
        for( int i=0; i < MAX_SUPPORTED_PINS; i++ )
        {
            if ( NULL != m_TimerIdentifier[i] )
            {
                m_pfnTimeKillEvent( m_TimerIdentifier[i] );
                m_TimerIdentifier[i] = NULL;
            }
        }
    }

    if ( NULL != m_hTimerDll )
    {
        FreeLibrary( m_hTimerDll );
        m_hTimerDll = NULL;
    }

    if( NULL != m_pModeVideoCaps )
    {
        delete [] m_pModeVideoCaps;
        m_pModeVideoCaps = NULL;
    }

    if( NULL != m_pCurrentFormat )
    {
        delete [] m_pCurrentFormat;
        m_pCurrentFormat = NULL;
    }

    if( NULL != m_pModeVideoFormat )
    {
        delete [] m_pModeVideoFormat;
        m_pModeVideoFormat = NULL;
    }

    if( NULL != m_ppModeContext )
    {
        delete [] m_ppModeContext;
        m_ppModeContext = NULL;
    }

    if(NULL != m_pIspCtrl)
    {
        delete m_pIspCtrl;
        m_pIspCtrl = NULL;
    }
}

DWORD CCameraPdd::PDDInit( PVOID MDDContext, PPDDFUNCTBL pPDDFuncTbl )
{
    DWORD dwIrq = IRQ_CAM0;

    m_hContext = (HANDLE)MDDContext;
    // Real drivers may want to create their context

    m_ulCTypes = 2; // Default number of Sensor Modes is 2
    
    // Read registry to override the default number of Sensor Modes.
    ReadMemoryModelFromRegistry();  

    if( pPDDFuncTbl->dwSize  > sizeof( PDDFUNCTBL ) )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    memcpy( pPDDFuncTbl, &FuncTbl, sizeof( PDDFUNCTBL ) );

    memset( m_SensorProps, 0x0, sizeof(m_SensorProps) );

    PowerCaps.DeviceDx = 0x11;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Set all VideoProcAmp and CameraControl properties.
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //VideoProcAmp
    m_SensorProps[ENUM_BRIGHTNESS].ulCurrentValue     = BrightnessDefault;
    m_SensorProps[ENUM_BRIGHTNESS].ulDefaultValue     = BrightnessDefault;
    m_SensorProps[ENUM_BRIGHTNESS].pRangeNStep        = &BrightnessRangeAndStep[0];
    m_SensorProps[ENUM_BRIGHTNESS].ulFlags            = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    m_SensorProps[ENUM_BRIGHTNESS].ulCapabilities     = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL|CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_BRIGHTNESS].fSetSupported      = VideoProcAmpProperties[ENUM_BRIGHTNESS].SetSupported;
    m_SensorProps[ENUM_BRIGHTNESS].fGetSupported      = VideoProcAmpProperties[ENUM_BRIGHTNESS].GetSupported;
    m_SensorProps[ENUM_BRIGHTNESS].pCsPropValues      = &BrightnessValues;

    m_SensorProps[ENUM_CONTRAST].ulCurrentValue       = ContrastDefault;
    m_SensorProps[ENUM_CONTRAST].ulDefaultValue       = ContrastDefault;
    m_SensorProps[ENUM_CONTRAST].pRangeNStep          = &ContrastRangeAndStep[0];
    m_SensorProps[ENUM_CONTRAST].ulFlags              = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
    m_SensorProps[ENUM_CONTRAST].ulCapabilities       = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL|CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_CONTRAST].fSetSupported        = VideoProcAmpProperties[ENUM_CONTRAST].SetSupported;
    m_SensorProps[ENUM_CONTRAST].fGetSupported        = VideoProcAmpProperties[ENUM_CONTRAST].GetSupported;
    m_SensorProps[ENUM_CONTRAST].pCsPropValues        = &ContrastValues;

    m_SensorProps[ENUM_HUE].ulCurrentValue            = HueDefault;
    m_SensorProps[ENUM_HUE].ulDefaultValue            = HueDefault;
    m_SensorProps[ENUM_HUE].pRangeNStep               = &HueRangeAndStep[0];
    m_SensorProps[ENUM_HUE].ulFlags                   = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_HUE].ulCapabilities            = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_HUE].fSetSupported             = VideoProcAmpProperties[ENUM_HUE].SetSupported;
    m_SensorProps[ENUM_HUE].fGetSupported             = VideoProcAmpProperties[ENUM_HUE].GetSupported;
    m_SensorProps[ENUM_HUE].pCsPropValues             = &HueValues;

    m_SensorProps[ENUM_SATURATION].ulCurrentValue     = SaturationDefault;
    m_SensorProps[ENUM_SATURATION].ulDefaultValue     = SaturationDefault;
    m_SensorProps[ENUM_SATURATION].pRangeNStep        = &SaturationRangeAndStep[0];
    m_SensorProps[ENUM_SATURATION].ulFlags            = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_SATURATION].ulCapabilities     = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL|CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_SATURATION].fSetSupported      = VideoProcAmpProperties[ENUM_SATURATION].SetSupported;
    m_SensorProps[ENUM_SATURATION].fGetSupported      = VideoProcAmpProperties[ENUM_SATURATION].GetSupported;
    m_SensorProps[ENUM_SATURATION].pCsPropValues      = &SaturationValues;

    m_SensorProps[ENUM_SHARPNESS].ulCurrentValue      = SharpnessDefault;
    m_SensorProps[ENUM_SHARPNESS].ulDefaultValue      = SharpnessDefault;
    m_SensorProps[ENUM_SHARPNESS].pRangeNStep         = &SharpnessRangeAndStep[0];
    m_SensorProps[ENUM_SHARPNESS].ulFlags             = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_SHARPNESS].ulCapabilities      = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_SHARPNESS].fSetSupported       = VideoProcAmpProperties[ENUM_SHARPNESS].SetSupported;
    m_SensorProps[ENUM_SHARPNESS].fGetSupported       = VideoProcAmpProperties[ENUM_SHARPNESS].GetSupported;
    m_SensorProps[ENUM_SHARPNESS].pCsPropValues       = &SharpnessValues;

    m_SensorProps[ENUM_GAMMA].ulCurrentValue          = GammaDefault;
    m_SensorProps[ENUM_GAMMA].ulDefaultValue          = GammaDefault;
    m_SensorProps[ENUM_GAMMA].pRangeNStep             = &GammaRangeAndStep[0];
    m_SensorProps[ENUM_GAMMA].ulFlags                 = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_GAMMA].ulCapabilities          = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_GAMMA].fSetSupported           = VideoProcAmpProperties[ENUM_GAMMA].SetSupported;
    m_SensorProps[ENUM_GAMMA].fGetSupported           = VideoProcAmpProperties[ENUM_GAMMA].GetSupported;
    m_SensorProps[ENUM_GAMMA].pCsPropValues           = &GammaValues;

    m_SensorProps[ENUM_COLORENABLE].ulCurrentValue    = ColorEnableDefault;
    m_SensorProps[ENUM_COLORENABLE].ulDefaultValue    = ColorEnableDefault;
    m_SensorProps[ENUM_COLORENABLE].pRangeNStep       = &ColorEnableRangeAndStep[0];
    m_SensorProps[ENUM_COLORENABLE].ulFlags           = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_COLORENABLE].ulCapabilities    = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL|CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_COLORENABLE].fSetSupported     = VideoProcAmpProperties[ENUM_COLORENABLE].SetSupported;
    m_SensorProps[ENUM_COLORENABLE].fGetSupported     = VideoProcAmpProperties[ENUM_COLORENABLE].GetSupported;
    m_SensorProps[ENUM_COLORENABLE].pCsPropValues     = &ColorEnableValues;

    m_SensorProps[ENUM_WHITEBALANCE].ulCurrentValue   = WhiteBalanceDefault;
    m_SensorProps[ENUM_WHITEBALANCE].ulDefaultValue   = WhiteBalanceDefault;
    m_SensorProps[ENUM_WHITEBALANCE].pRangeNStep      = &WhiteBalanceRangeAndStep[0];
    m_SensorProps[ENUM_WHITEBALANCE].ulFlags          = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_WHITEBALANCE].ulCapabilities   = CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL|CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_WHITEBALANCE].fSetSupported    = VideoProcAmpProperties[ENUM_WHITEBALANCE].SetSupported;
    m_SensorProps[ENUM_WHITEBALANCE].fGetSupported    = VideoProcAmpProperties[ENUM_WHITEBALANCE].GetSupported;
    m_SensorProps[ENUM_WHITEBALANCE].pCsPropValues    = &WhiteBalanceValues;

    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].ulCurrentValue = BackLightCompensationDefault;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].ulDefaultValue = BackLightCompensationDefault;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].pRangeNStep    = &BackLightCompensationRangeAndStep[0];
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].ulFlags        = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].ulCapabilities = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].fSetSupported  = VideoProcAmpProperties[ENUM_BACKLIGHT_COMPENSATION].SetSupported;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].fGetSupported  = VideoProcAmpProperties[ENUM_BACKLIGHT_COMPENSATION].GetSupported;
    m_SensorProps[ENUM_BACKLIGHT_COMPENSATION].pCsPropValues  = &BackLightCompensationValues;

    m_SensorProps[ENUM_GAIN].ulCurrentValue           = GainDefault;
    m_SensorProps[ENUM_GAIN].ulDefaultValue           = GainDefault;
    m_SensorProps[ENUM_GAIN].pRangeNStep              = &GainRangeAndStep[0];
    m_SensorProps[ENUM_GAIN].ulFlags                  = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_GAIN].ulCapabilities           = CSPROPERTY_VIDEOPROCAMP_FLAGS_AUTO;
    m_SensorProps[ENUM_GAIN].fSetSupported            = VideoProcAmpProperties[ENUM_GAIN].SetSupported;
    m_SensorProps[ENUM_GAIN].fGetSupported            = VideoProcAmpProperties[ENUM_GAIN].GetSupported;
    m_SensorProps[ENUM_GAIN].pCsPropValues            = &GainValues;

    //CameraControl
    m_SensorProps[ENUM_PAN].ulCurrentValue            = PanDefault;
    m_SensorProps[ENUM_PAN].ulDefaultValue            = PanDefault;
    m_SensorProps[ENUM_PAN].pRangeNStep               = &PanRangeAndStep[0];
    m_SensorProps[ENUM_PAN].ulFlags                   = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_PAN].ulCapabilities            = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_PAN].fSetSupported             = VideoProcAmpProperties[ENUM_PAN-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_PAN].fGetSupported             = VideoProcAmpProperties[ENUM_PAN-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_PAN].pCsPropValues             = &PanValues;

    m_SensorProps[ENUM_TILT].ulCurrentValue           = TiltDefault;
    m_SensorProps[ENUM_TILT].ulDefaultValue           = TiltDefault;
    m_SensorProps[ENUM_TILT].pRangeNStep              = &TiltRangeAndStep[0];
    m_SensorProps[ENUM_TILT].ulFlags                  = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_TILT].ulCapabilities           = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_TILT].fSetSupported            = VideoProcAmpProperties[ENUM_TILT-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_TILT].fGetSupported            = VideoProcAmpProperties[ENUM_TILT-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_TILT].pCsPropValues            = &TiltValues;

    m_SensorProps[ENUM_ROLL].ulCurrentValue           = RollDefault;
    m_SensorProps[ENUM_ROLL].ulDefaultValue           = RollDefault;
    m_SensorProps[ENUM_ROLL].pRangeNStep              = &RollRangeAndStep[0];
    m_SensorProps[ENUM_ROLL].ulFlags                  = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_ROLL].ulCapabilities           = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_ROLL].fSetSupported            = VideoProcAmpProperties[ENUM_ROLL-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_ROLL].fGetSupported            = VideoProcAmpProperties[ENUM_ROLL-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_ROLL].pCsPropValues            = &RollValues;

    m_SensorProps[ENUM_ZOOM].ulCurrentValue           = ZoomDefault;
    m_SensorProps[ENUM_ZOOM].ulDefaultValue           = ZoomDefault;
    m_SensorProps[ENUM_ZOOM].pRangeNStep              = &ZoomRangeAndStep[0];
    m_SensorProps[ENUM_ZOOM].ulFlags                  = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_ZOOM].ulCapabilities           = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_ZOOM].fSetSupported            = VideoProcAmpProperties[ENUM_ZOOM-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_ZOOM].fGetSupported            = VideoProcAmpProperties[ENUM_ZOOM-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_ZOOM].pCsPropValues            = &ZoomValues;

    m_SensorProps[ENUM_IRIS].ulCurrentValue           = IrisDefault;
    m_SensorProps[ENUM_IRIS].ulDefaultValue           = IrisDefault;
    m_SensorProps[ENUM_IRIS].pRangeNStep              = &IrisRangeAndStep[0];
    m_SensorProps[ENUM_IRIS].ulFlags                  = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_IRIS].ulCapabilities           = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_IRIS].fSetSupported            = VideoProcAmpProperties[ENUM_IRIS-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_IRIS].fGetSupported            = VideoProcAmpProperties[ENUM_IRIS-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_IRIS].pCsPropValues            = &IrisValues;

    m_SensorProps[ENUM_EXPOSURE].ulCurrentValue       = ExposureDefault;
    m_SensorProps[ENUM_EXPOSURE].ulDefaultValue       = ExposureDefault;
    m_SensorProps[ENUM_EXPOSURE].pRangeNStep          = &ExposureRangeAndStep[0];
    m_SensorProps[ENUM_EXPOSURE].ulFlags              = CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_EXPOSURE].ulCapabilities       = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL|CSPROPERTY_CAMERACONTROL_FLAGS_AUTO;
    m_SensorProps[ENUM_EXPOSURE].fSetSupported        = VideoProcAmpProperties[ENUM_EXPOSURE-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_EXPOSURE].fGetSupported        = VideoProcAmpProperties[ENUM_EXPOSURE-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_EXPOSURE].pCsPropValues        = &ExposureValues;

    m_SensorProps[ENUM_FOCUS].ulCurrentValue          = FocusDefault;
    m_SensorProps[ENUM_FOCUS].ulDefaultValue          = FocusDefault;
    m_SensorProps[ENUM_FOCUS].pRangeNStep             = &FocusRangeAndStep[0];
    m_SensorProps[ENUM_FOCUS].ulFlags                 = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
    m_SensorProps[ENUM_FOCUS].ulCapabilities          = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
    m_SensorProps[ENUM_FOCUS].fSetSupported           = VideoProcAmpProperties[ENUM_FOCUS-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_FOCUS].fGetSupported           = VideoProcAmpProperties[ENUM_FOCUS-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_FOCUS].pCsPropValues           = &FocusValues;

    m_SensorProps[ENUM_FLASH].ulCurrentValue          = FlashDefault;
    m_SensorProps[ENUM_FLASH].ulDefaultValue          = FlashDefault;
    m_SensorProps[ENUM_FLASH].pRangeNStep             = &FlashRangeAndStep[0];
    m_SensorProps[ENUM_FLASH].ulFlags                 = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
    m_SensorProps[ENUM_FLASH].ulCapabilities          = CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
    m_SensorProps[ENUM_FLASH].fSetSupported           = VideoProcAmpProperties[ENUM_FLASH-NUM_VIDEOPROCAMP_ITEMS].SetSupported;
    m_SensorProps[ENUM_FLASH].fGetSupported           = VideoProcAmpProperties[ENUM_FLASH-NUM_VIDEOPROCAMP_ITEMS].GetSupported;
    m_SensorProps[ENUM_FLASH].pCsPropValues           = &FlashValues;
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    m_pModeVideoFormat = NULL;
    // Allocate Video Format specific array.
    m_pModeVideoFormat = new PINVIDEOFORMAT[m_ulCTypes];
    if( NULL == m_pModeVideoFormat )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }


    // Video Format initialization
    m_pModeVideoFormat[CAPTURE].categoryGUID         = PINNAME_VIDEO_CAPTURE;
    m_pModeVideoFormat[CAPTURE].ulAvailFormats       = 1;
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[CAPTURE].ulAvailFormats];

    if( NULL == m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[0] = &DCAM_StreamMode_10;

    m_pModeVideoFormat[STILL].categoryGUID           = PINNAME_VIDEO_STILL;
    m_pModeVideoFormat[STILL].ulAvailFormats         = 1;
    m_pModeVideoFormat[STILL].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[STILL].ulAvailFormats];

    if( NULL == m_pModeVideoFormat[STILL].pCsDataRangeVideo )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

#ifdef ENABLE_STILL_IMAGE
    m_pModeVideoFormat[STILL].pCsDataRangeVideo[0]   = &DCAM_StreamMode_10;
#else
    m_pModeVideoFormat[STILL].pCsDataRangeVideo[0]   = &DCAM_StreamMode_0;
#endif 

    if( 3 == m_ulCTypes )
    {
        m_pModeVideoFormat[PREVIEW].categoryGUID         = PINNAME_VIDEO_PREVIEW;
        m_pModeVideoFormat[PREVIEW].ulAvailFormats       = 7;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[PREVIEW].ulAvailFormats];

        if( NULL == m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo )
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[0] = &DCAM_StreamMode_0;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[1] = &DCAM_StreamMode_1;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[2] = &DCAM_StreamMode_2;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[3] = &DCAM_StreamMode_6;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[4] = &DCAM_StreamMode_7;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[5] = &DCAM_StreamMode_8;
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[6] = &DCAM_StreamMode_9;
    }
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    m_pModeVideoCaps = NULL;
    // Allocate Video Control Caps specific array.
    m_pModeVideoCaps = new VIDCONTROLCAPS[m_ulCTypes];
    if( NULL == m_pModeVideoCaps )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }
    // Video Control Caps

    m_pModeVideoCaps[CAPTURE].DefaultVideoControlCaps     = DefaultVideoControlCaps[CAPTURE];
    m_pModeVideoCaps[CAPTURE].CurrentVideoControlCaps     = DefaultVideoControlCaps[CAPTURE];;
    m_pModeVideoCaps[STILL].DefaultVideoControlCaps       = DefaultVideoControlCaps[STILL];
    m_pModeVideoCaps[STILL].CurrentVideoControlCaps       = DefaultVideoControlCaps[STILL];;
    if( 3 == m_ulCTypes )
    {
        // Note PREVIEW control caps are the same, so we don't differentiate
        m_pModeVideoCaps[PREVIEW].DefaultVideoControlCaps     = DefaultVideoControlCaps[PREVIEW];
        m_pModeVideoCaps[PREVIEW].CurrentVideoControlCaps     = DefaultVideoControlCaps[PREVIEW];;
    }

    // Timer specific variables. Only to be used in this NULL PDD
    m_hTimerDll                 = NULL;
    m_pfnTimeSetEvent           = NULL;
    m_pfnTimeKillEvent          = NULL;
    memset( &m_TimerIdentifier, 0, MAX_SUPPORTED_PINS * sizeof(MMRESULT));

    m_SensorModeInfo[CAPTURE].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
    m_SensorModeInfo[CAPTURE].MaxNumOfBuffers = 1;
    m_SensorModeInfo[CAPTURE].PossibleCount = 1;
    m_SensorModeInfo[STILL].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
    m_SensorModeInfo[STILL].MaxNumOfBuffers = 1;
    m_SensorModeInfo[STILL].PossibleCount = 1;
    if( 3 == m_ulCTypes )
    {
        m_SensorModeInfo[PREVIEW].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
        m_SensorModeInfo[PREVIEW].MaxNumOfBuffers = 1;
        m_SensorModeInfo[PREVIEW].PossibleCount = 1;
    }

    m_ppModeContext = new LPVOID[m_ulCTypes];
    if ( NULL == m_ppModeContext )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    m_pCurrentFormat = new CS_DATARANGE_VIDEO[m_ulCTypes];
    if( NULL == m_pCurrentFormat )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    //////    Initial interrupt thread    //////

    // Camera ISP Interrupt Event - IRQ_CAM_0 signaled.
    m_CAMISPEvent = CreateEvent( NULL, FALSE, FALSE, NULL);

    if (!m_CAMISPEvent) {
        ERRORMSG(ZONE_ERROR, (_T("Failed to CreateEvent(m_CAMISPEvent) \r\n")));
        goto clean;
    }

    if(!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR,
        &dwIrq, sizeof(dwIrq), &m_CAMISPIntr, sizeof(DWORD), NULL)) {
        ERRORMSG(ZONE_ERROR, (_T("Failed to KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR)\r\n")));
        m_CAMISPIntr = 0;
    }

    if (m_CAMISPIntr == 0) {
        ERRORMSG(ZONE_ERROR, (_T(" Failed to allocate camera system interrupt event, m_CAMISPIntr\r\n") ));
        goto clean;
    }

    if (!InterruptInitialize(m_CAMISPIntr, m_CAMISPEvent, NULL, 0)) {
        ERRORMSG(ZONE_ERROR, (_T(" Failed to InterruptInitialize(m_CAMISPIntr) \r\n")));
        goto clean;
    }

    InterruptDone( m_CAMISPIntr );

    EnableDeviceClocks(OMAP_DEVICE_CAMERA, TRUE);

    if (!RequestDevicePads(OMAP_DEVICE_CAMERA))
    {
        DEBUGMSG(ZONE_ERROR, (L"ERROR: CAM_Init: Failed request pads\r\n"));
        goto clean;
    }

    m_CurrentPowerState = D0;

    m_hCCDCInterruptThread = CreateThread( NULL,
                                              0,
                      (LPTHREAD_START_ROUTINE)CameraInterruptThread,
                                              this,
                                              0,
                                              NULL); //CREATE_SUSPENDED
    m_pIspCtrl->InitializeCamera(); //Init. ISP 
    m_pTvpCtrl->Init();// Init. tvp5146 video decoder.

    return ERROR_SUCCESS;
clean:
    return FALSE;
}

//------------------------------------------------------------------------------
//
//  CameraInterruptThread
//
//  Description: Camera event interrupt handler function
//
//

static DWORD
CameraInterruptThread(LPVOID lpParameter )
{
    CCameraPdd *pThis = reinterpret_cast<CCameraPdd *>(lpParameter);

    pThis->CameraInterruptThreadImpl();

    return ERROR_SUCCESS;
}



//-----------------------------------------------------------------------------
//
//  CameraInterruptThreadImpl
//
//  Description : This is a thread function for handling the camera data events
//                On receiving camera event, the thread pass the buffer to MDD
//                and notifies the MDD to send it to the Dshow middleware.
//
    
DWORD CCameraPdd::CameraInterruptThreadImpl()
{
    int i=0;
    DWORD dwEventStatus ;
    UINT32 setting = 0;

    do
    {       
        /* wait for start */
        dwEventStatus = WaitForSingleObject(m_CAMISPEvent, INFINITE);

        if (dwEventStatus==WAIT_TIMEOUT)
        {
            DEBUGMSG(ZONE_VERBOSE,(_T("CameraInterruptThreadImpl:Camera thread timed out\r\n")));   
        } 
        else
        {               
            BOOL oddField = FALSE;

            setting=INREG32(&m_pIspCtrl->GetCCDCRegs()->CCDC_SYN_MODE);

            if (!(setting&ISPCCDC_SYN_MODE_FLDSTAT)) {
                oddField = TRUE;
            }

            setting = INREG32(&m_pIspCtrl->GetIspCfgRegs()->IRQ0STATUS);
            DEBUGMSG(ZONE_VERBOSE,(_T("IRQ0: 0x%08x\r\n"), setting));
            
            if(setting & 0x80000000) //VD detected
            {
                //DWORD dwTime=GetTickCount();
                //DWORD dwTime=GetTickCount();
                if(m_bStillCapInProgress)
               		HandleStillInterrupt(STILL);                 	
                else if(m_CsState[CAPTURE] == CSSTATE_RUN)
                	HandleCaptureInterrupt(CAPTURE);
                //dwTime=GetTickCount()-dwTime;
                //DEBUGMSG(ZONE_VERBOSE,(_T("dwTime=%d \r\n", dwTime)));
             }
                
            OUTREG32(&m_pIspCtrl->GetIspCfgRegs()->IRQ0STATUS, setting);
     
            InterruptDone(m_CAMISPIntr);
        }

    }while(1);

    return ERROR_SUCCESS;

}


DWORD CCameraPdd::GetAdapterInfo( PADAPTERINFO pAdapterInfo )
{
    pAdapterInfo->ulCTypes = m_ulCTypes;
    pAdapterInfo->PowerCaps = PowerCaps;
    pAdapterInfo->ulVersionID = DRIVER_VERSION_2; //Camera MDD and DShow support DRIVER_VERSION and DRIVER_VERSION_2. Defined in camera.h
    memcpy( &pAdapterInfo->SensorProps, &m_SensorProps, sizeof(m_SensorProps));

    return ERROR_SUCCESS;

}

DWORD CCameraPdd::HandleVidProcAmpChanges( DWORD dwPropId, LONG lFlags, LONG lValue )
{
    PSENSOR_PROPERTY pDevProp = NULL;

    pDevProp = m_SensorProps + dwPropId;
    
    if( CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL == lFlags )
    {
        pDevProp->ulCurrentValue = lValue;
    }

    pDevProp->ulFlags = lFlags;
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::HandleCamControlChanges( DWORD dwPropId, LONG lFlags, LONG lValue )
{
    PSENSOR_PROPERTY pDevProp = NULL;
    
    pDevProp = m_SensorProps + dwPropId;
    
    if( CSPROPERTY_CAMERACONTROL_FLAGS_MANUAL == lFlags )
    {
        pDevProp->ulCurrentValue = lValue;
    }

    pDevProp->ulFlags = lFlags;
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::HandleVideoControlCapsChanges( LONG lModeType ,ULONG ulCaps )
{
    m_pModeVideoCaps[lModeType].CurrentVideoControlCaps = ulCaps;
    return ERROR_SUCCESS;
}

DWORD CCameraPdd :: SetPowerState( CEDEVICE_POWER_STATE PowerState )
{
    if (PowerState <= D3 && m_CurrentPowerState >= D3)
    {
        // Set the camera Power state to D0
        m_pTvpCtrl->SetPowerState(TRUE);
        EnableDeviceClocks(OMAP_DEVICE_CAMERA, TRUE);
	}
	else if (PowerState >= D3 && m_CurrentPowerState <= D2)
    {

        EnableDeviceClocks(OMAP_DEVICE_CAMERA, FALSE);
        m_pTvpCtrl->SetPowerState(FALSE);
	}

    m_CurrentPowerState = PowerState;
		
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::HandleAdapterCustomProperties( PUCHAR pInBuf, DWORD  InBufLen, PUCHAR pOutBuf, DWORD  OutBufLen, PDWORD pdwBytesTransferred )
{
    DEBUGMSG( ZONE_IOCTL, ( _T("IOControl Adapter PDD: HandleAdapterCustomProperties Request\r\n")) );

    DWORD dwRet = ERROR_SUCCESS;
 
    
    if(InBufLen == sizeof(CSPROPERTY_CAMERACONTROL_S))
    {
        DEBUGMSG(ZONE_VERBOSE, (L"InBufLen == sizeof(CameraControl)()\r\n"));
        CSPROPERTY_CAMERACONTROL_S  CameraControl;
        memcpy(&CameraControl, pInBuf, InBufLen);
       
        switch( CameraControl.Property.Flags )
        {
        case CSPROPERTY_TYPE_GET:
        {
            if(CameraControl.Property.Id ==ENUM_CUST_INPUTMODE)
            {
                DEBUGMSG(ZONE_VERBOSE, (L"CSPROPERTY_TYPE_GET(), m_dwInputMode=%d\r\n", m_dwInputMode));

                CSPROPERTY_CAMERACONTROL_S  CameraControlOut;
                CameraControlOut.Value = m_dwInputMode;
                memcpy(pOutBuf, &CameraControlOut, OutBufLen);
                
                dwRet = ERROR_SUCCESS;
                break;
               }
        }
            dwRet = ERROR_NOT_SUPPORTED;
            break;
            
        case CSPROPERTY_TYPE_SET:
            DEBUGMSG(ZONE_ACTIVITY, (L"CSPROPERTY_TYPE_SET()\r\n"));
            switch (CameraControl.Property.Id)
            {

            case ENUM_CUST_INPUTMODE:
                {
                    DEBUGMSG(ZONE_ACTIVITY, (L"ENUM_CUST_INPUTMODE CameraControl.Value(%d)\r\n", CameraControl.Value));
                    m_dwInputMode = CameraControl.Value;

                    switch(m_dwInputMode)
                   	{
                   		case INPUT_YPcPr:
                   			m_pTvpCtrl->SelectComponent();
                   			break;
                   		case INPUT_AV:
                   			m_pTvpCtrl->SelectComposite();
                   			break;
                   		case INPUT_S_VIDEO:
                   			m_pTvpCtrl->SelectSVideo();
                   			break;
                   	}
                }
                break;
            
            default:
                dwRet = ERROR_NOT_SUPPORTED;            
                break;
            }
                
            default :
            {
                dwRet = ERROR_NOT_SUPPORTED;
            }
            break;
        }

        DEBUGMSG(ZONE_VERBOSE, (L"CameraControl.Property.Flags(%d)\r\n", CameraControl.Property.Flags));
        DEBUGMSG(ZONE_VERBOSE, (L"CameraControl.Property.Id(%d)\r\n", CameraControl.Property.Id));
        DEBUGMSG(ZONE_VERBOSE, (L"CameraControl.Flags(%d)\r\n", CameraControl.Flags));
        DEBUGMSG(ZONE_VERBOSE, (L"CameraControl.Value(%d)\r\n", CameraControl.Value));
        DEBUGMSG(ZONE_VERBOSE, (L"CameraControl.Capabilities(%d)\r\n", CameraControl.Capabilities));
        DEBUGMSG(ZONE_VERBOSE, (L"CSPROPERTY_TYPE_SET(%d)\r\n", CSPROPERTY_TYPE_SET));
        
        return dwRet;
        
    }
    return ERROR_NOT_SUPPORTED;
}

DWORD CCameraPdd::InitSensorMode( ULONG ulModeType, LPVOID ModeContext )
{
    ASSERT( ModeContext );
    DEBUGMSG(ZONE_ERROR,(_T("SetSensorState: InitSensorMode: lModeType=%d, ModeContext=0x%08X\r\n"),
        ulModeType,ModeContext));
    m_ppModeContext[ulModeType] = ModeContext;
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::DeInitSensorMode( ULONG ulModeType )
{
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::Run(ULONG lModeType, CSSTATE csState)
{
	DEBUGMSG(ZONE_FUNCTION,(_T("SetSensorState: CSSTATE_RUN: lModeType=%d, csState=%d \r\n"),lModeType,csState));
	
	if(lModeType==CAPTURE && m_CsState[lModeType] != CSSTATE_RUN)
    {
    	if(!m_pIspCtrl->EnableCamera(NULL))
        	return ERROR_INVALID_PARAMETER;        	
    }
            
    m_CsState[lModeType] = CSSTATE_RUN;     
    return  ERROR_SUCCESS;            
}

DWORD CCameraPdd::Pause(ULONG lModeType, CSSTATE csState)
{   
    DEBUGMSG(ZONE_FUNCTION,(_T("SetSensorState: CSSTATE_PAUSE: lModeType=%d, csState=%d \r\n"),lModeType,csState));
    
    if(lModeType==CAPTURE && (m_CsState[lModeType] != CSSTATE_PAUSE || m_CsState[lModeType] != CSSTATE_STOP))
    {
        if(!m_pIspCtrl->DisableCamera())
            return ERROR_INVALID_PARAMETER;                     
    }           
    m_CsState[lModeType] = CSSTATE_PAUSE;
    return  ERROR_SUCCESS;
}

DWORD CCameraPdd::Stop(ULONG lModeType, CSSTATE csState)
{
    DEBUGMSG(ZONE_FUNCTION,(_T("SetSensorState: CSSTATE_STOP: lModeType=%d, csState=%d \r\n"),lModeType,csState));
    
    if(lModeType==CAPTURE && (m_CsState[lModeType] != CSSTATE_PAUSE || m_CsState[lModeType] != CSSTATE_STOP))
    {
        if(!m_pIspCtrl->DisableCamera())
            return ERROR_INVALID_PARAMETER;                     
    }           
    m_CsState[lModeType] = CSSTATE_STOP;

	if( STILL == lModeType )
	{
		m_bStillCapInProgress = false;
	}

	return 	ERROR_SUCCESS;
}
    
DWORD CCameraPdd::SetSensorState( ULONG lModeType, CSSTATE csState )
{
    DWORD dwError = ERROR_SUCCESS;

    switch ( csState )
    {
        case CSSTATE_STOP:
            dwError = Stop(lModeType, csState);
            break;

        case CSSTATE_PAUSE:
            dwError = Pause(lModeType, csState);
            break;

        case CSSTATE_RUN:
            dwError = Run(lModeType, csState);                      
            break;

        default:
            ERRORMSG( ZONE_IOCTL|ZONE_ERROR, ( _T("IOControl(%08x): Incorrect State\r\n"), this ) );
            dwError = ERROR_INVALID_PARAMETER;
    }

    return dwError;
}

DWORD CCameraPdd::TakeStillPicture( LPVOID pBurstModeInfo )
{
#ifndef ENABLE_STILL_IMAGE
	return 	ERROR_INVALID_PARAMETER;
#endif 
	
    DWORD dwError = ERROR_SUCCESS;
    m_bStillCapInProgress = true;
    
    //Ignore pBurstModeInfo
    m_CsState[STILL] = CSSTATE_RUN;
    
    return dwError;
}


DWORD CCameraPdd::GetSensorModeInfo( ULONG ulModeType, PSENSORMODEINFO pSensorModeInfo )
{
    pSensorModeInfo->MemoryModel = m_SensorModeInfo[ulModeType].MemoryModel;
    pSensorModeInfo->MaxNumOfBuffers = m_SensorModeInfo[ulModeType].MaxNumOfBuffers;
    pSensorModeInfo->PossibleCount = m_SensorModeInfo[ulModeType].PossibleCount;
    pSensorModeInfo->VideoCaps.DefaultVideoControlCaps = DefaultVideoControlCaps[ulModeType];
    pSensorModeInfo->VideoCaps.CurrentVideoControlCaps = m_pModeVideoCaps[ulModeType].CurrentVideoControlCaps;
    pSensorModeInfo->pVideoFormat = &m_pModeVideoFormat[ulModeType];
    
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::SetSensorModeFormat( ULONG ulModeType, PCS_DATARANGE_VIDEO pCsDataRangeVideo )
{
    PCS_VIDEOINFOHEADER pCsVideoInfoHdr = &(pCsDataRangeVideo->VideoInfoHeader);

    UINT biWidth        = pCsVideoInfoHdr->bmiHeader.biWidth;
    UINT biHeight       = pCsVideoInfoHdr->bmiHeader.biHeight;
    UINT biSizeImage    = pCsVideoInfoHdr->bmiHeader.biSizeImage;
    UINT biWidthBytes   = CS_DIBWIDTHBYTES (pCsVideoInfoHdr->bmiHeader);
    UINT biBitCount     = pCsVideoInfoHdr->bmiHeader.biBitCount;
    UINT LinesToCopy    = abs (biHeight);
    DWORD biCompression = pCsVideoInfoHdr->bmiHeader.biCompression & ~BI_SRCPREROTATE;

	DEBUGMSG(ZONE_FUNCTION,(_T("CCameraPdd::SetSensorModeFormat: biWidth=%d, biHeight=%d \r\n"),biWidth, biHeight));
    memcpy( &m_pCurrentFormat[ulModeType], pCsDataRangeVideo, sizeof ( CS_DATARANGE_VIDEO ) );

    return ERROR_SUCCESS;
}

PVOID CCameraPdd::AllocateBuffer( ULONG ulModeType )
{  
    // Real PDD may want to save off this allocated pointer
    // in an array.
    ULONG ulFrameSize = CS__DIBSIZE (m_pCurrentFormat[ulModeType].VideoInfoHeader.bmiHeader);
    return RemoteLocalAlloc( LPTR, ulFrameSize );
}

DWORD CCameraPdd::DeAllocateBuffer( ULONG ulModeType, PVOID pBuffer )
{
    RemoteLocalFree( pBuffer );
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::RegisterClientBuffer( ULONG ulModeType, PVOID pBuffer )
{
    // Real PDD may want to save pBuffer which is a pointer to buffer that DShow created.   
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::UnRegisterClientBuffer( ULONG ulModeType, PVOID pBuffer )
{
    // DShow is not going to use pBuffer (which was originally allocated by DShow) anymore. If the PDD
    // is keeping a cached pBuffer pointer (in RegisterClientBuffer()) then this is the right place to
    // stop using it and maybe set the cached pointer to NULL. 
    // Note: PDD must not delete this pointer as it will be deleted by DShow itself
    return ERROR_SUCCESS;
}

DWORD CCameraPdd::HandleSensorModeCustomProperties( ULONG ulModeType, PUCHAR pInBuf, DWORD  InBufLen, PUCHAR pOutBuf, DWORD  OutBufLen, PDWORD pdwBytesTransferred )
{
    DEBUGMSG( ZONE_IOCTL, ( _T("IOControl: Unsupported PropertySet Request\r\n")) );
    return ERROR_NOT_SUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following code is only meant for this sample pdd
// The real PDD should not contain any of the code below.
// Instead the real PDD should implement its own FillPinBuffer() 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern "C" { WINGDIAPI HBITMAP WINAPI CreateBitmapFromPointer( CONST BITMAPINFO *pbmi, int iStride, PVOID pvBits); }

#ifndef ENABLE_PACK8
LPVOID UYVYtoYUY2(WORD* pInput, BYTE* pOutput, DWORD dwNumByte)
{
    ULONG* pIn= (ULONG*) pInput;
    PBYTE pOut=pOutput;
    
    if(dwNumByte/2==0)
        return NULL;

    DWORD i=0;
    for(i; i<dwNumByte/2; i++)
    {
        pOut[i*2]=(BYTE) (pIn[i]>>18);
        pOut[i*2+1]=(BYTE) (pIn[i] >>2);
    }
    
    return pOutput;
}
#else

LPVOID UYVYtoYUY2(WORD* pInput, BYTE* pOutput, DWORD dwNumByte)
{
    WORD* pIn= (WORD*) pInput;
    PBYTE pOut=pOutput;
    
    if(dwNumByte/2==0)
        return NULL;

    DWORD i=0;
    for(i; i<dwNumByte/2; i++)
    {
        pOut[i*2]=(BYTE) (pIn[i]>>8);
        pOut[i*2+1]=(BYTE) (pIn[i]);
    }
    
    return pOutput;
}
#endif //ENABLE_PACK8

DWORD CCameraPdd::FillBuffer( ULONG ulModeType, PUCHAR pImage )
{
    PCS_VIDEOINFOHEADER pCsVideoInfoHdr = &m_pCurrentFormat[ulModeType].VideoInfoHeader;

    UINT biWidth        = pCsVideoInfoHdr->bmiHeader.biWidth;
    UINT biHeight       = pCsVideoInfoHdr->bmiHeader.biHeight;
    UINT biSizeImage    = pCsVideoInfoHdr->bmiHeader.biSizeImage;
    UINT biWidthBytes   = CS_DIBWIDTHBYTES (pCsVideoInfoHdr->bmiHeader);
    UINT biBitCount     = pCsVideoInfoHdr->bmiHeader.biBitCount;
    UINT LinesToCopy    = abs (biHeight);
    DWORD biCompression = pCsVideoInfoHdr->bmiHeader.biCompression;

    HBITMAP hbmp = NULL;
    HDC hdc = NULL;

	DWORD dwTemp;

	dwTemp = (DWORD)m_pIspCtrl->GetFrameBuffer();

	DEBUGMSG(ZONE_FUNCTION,(_T("CCameraPdd::FillBuffer: biWidth=%d, biHeight=%d, biSizeImage=%d, pImage=0x%08X \r\n"),
			biWidth, biHeight, biSizeImage, pImage));
    if ( (FOURCC_YUY2 == (biCompression & ~BI_SRCPREROTATE)) ||
         (FOURCC_YV12 == (biCompression & ~BI_SRCPREROTATE)) )
    {
		#ifndef ENABLE_PACK8		
			UYVYtoYUY2((WORD*) m_pIspCtrl->GetFrameBuffer(), pImage, biSizeImage);
		#else
			memcpy(pImage,m_pIspCtrl->GetFrameBuffer(),biSizeImage); //HW swapped, just copy.
		#endif//ENABLE_PACK8
		return biSizeImage;

    }
	ERRORMSG(ZONE_ERROR,(_T("CCameraPdd::FillBuffer: No this format \r\n"),biWidth, biHeight, biSizeImage, pImage));
        
	return(-1);
}

bool CCameraPdd :: CreateTimer( ULONG ulModeType )
{
    if ( NULL == m_hTimerDll )
    {
        m_hTimerDll        = LoadLibrary( L"MMTimer.dll" );
        m_pfnTimeSetEvent  = (FNTIMESETEVENT)GetProcAddress( m_hTimerDll, L"timeSetEvent" );
        m_pfnTimeKillEvent = (FNTIMEKILLEVENT)GetProcAddress( m_hTimerDll, L"timeKillEvent" );

        if ( NULL == m_pfnTimeSetEvent || NULL == m_pfnTimeKillEvent )
        {
            ERRORMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl(%08x): GetProcAddress Returned Null.\r\n"), this));
            return false;
        }
    }
    
    if ( NULL == m_hTimerDll )
    {
            ERRORMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl(%08x): LoadLibrary failed.\r\n"), this));
            return false;
    }

    if (ulModeType >= MAX_SUPPORTED_PINS)
        return FALSE;

    ASSERT( m_pfnTimeSetEvent );

    if ( NULL == m_TimerIdentifier[ulModeType] )
    {
        DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl(%08x): Creating new timer.\r\n"), this));
        if( STILL == ulModeType )
        {
            m_TimerIdentifier[ulModeType] = m_pfnTimeSetEvent( (ULONG)m_pCurrentFormat[ulModeType].VideoInfoHeader.AvgTimePerFrame/10000, 10, CCameraPdd::StillTimerCallBack, reinterpret_cast<DWORD>(this), TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
        }
        else
        {
            m_TimerIdentifier[ulModeType] = m_pfnTimeSetEvent( (ULONG)m_pCurrentFormat[ulModeType].VideoInfoHeader.AvgTimePerFrame/10000, 10, CCameraPdd::CaptureTimerCallBack, reinterpret_cast<DWORD>(this), TIME_PERIODIC|TIME_CALLBACK_FUNCTION);
        }

        if ( NULL == m_TimerIdentifier[ulModeType] )
        {
            DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl(%08x): Timer could not be created.\r\n"), this));
            return false;
        }
    }
    else
    {
        DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl(%08x): Timer already created.\r\n"), this));
    }

    return true;
}

void CCameraPdd :: CaptureTimerCallBack( UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2 )
{
    UNREFERENCED_PARAMETER(uTimerID) ;
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dw1);
    UNREFERENCED_PARAMETER(dw2);

    CCameraPdd *pNullPdd= reinterpret_cast<CCameraPdd *>(dwUser);
    if( NULL == pNullPdd )
    {
        DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl: TimerCallBack pNullPdd is NULL.\r\n"))) ;
    }
    else
    {
        __try
        {
            pNullPdd->HandleCaptureInterrupt( uTimerID );
        }
        __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl: TimerCallBack, Access violation.\r\n"))) ;
        }
    }
}

void CCameraPdd :: StillTimerCallBack( UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2 )
{
    UNREFERENCED_PARAMETER(uTimerID) ;
    UNREFERENCED_PARAMETER(uMsg);
    UNREFERENCED_PARAMETER(dw1);
    UNREFERENCED_PARAMETER(dw2);

    CCameraPdd *pNullPdd= reinterpret_cast<CCameraPdd *>(dwUser);
    if( NULL == pNullPdd )
    {
        DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl: TimerCallBack pNullPdd is NULL.\r\n"))) ;
    }
    else
    {
        __try
        {
            pNullPdd->HandleStillInterrupt( uTimerID );
        }
        __except(GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        {
            DEBUGMSG(ZONE_IOCTL|ZONE_ERROR, (_T("IOControl: TimerCallBack, Access violation.\r\n"))) ;
        }
    }
}


void CCameraPdd :: HandleCaptureInterrupt( UINT uTimerID )
{
    ULONG ulModeType;

    if( m_bStillCapInProgress )
    {
        return;
    }
    
    if(uTimerID == CAPTURE)
    {
        ulModeType = CAPTURE;
    }
    else if ( m_ulCTypes == 3 && m_TimerIdentifier[PREVIEW] == uTimerID )
    {
        ulModeType = PREVIEW;
    }
    else
    {
        ASSERT(false);
        return;
    }

    MDD_HandleIO( m_ppModeContext[ulModeType], ulModeType );

}


void CCameraPdd :: HandleStillInterrupt( UINT uTimerID )
{
	DEBUGMSG(ZONE_FUNCTION,(_T("+HandleStillInterrupt \r\n")));
	
    MDD_HandleIO( m_ppModeContext[STILL], STILL );
    m_bStillCapInProgress = false;
}

bool CCameraPdd::ReadMemoryModelFromRegistry()
{
    HKEY  hKey = 0;
    DWORD dwType  = 0;
    DWORD dwSize  = sizeof ( DWORD );
    DWORD dwValue = -1;


    if( ERROR_SUCCESS != RegOpenKeyEx( HKEY_LOCAL_MACHINE, L"Drivers\\Capture\\SampleCam", 0, 0, &hKey ))
    {
        false;
    }

    if( ERROR_SUCCESS == RegQueryValueEx( hKey, L"MemoryModel", 0, &dwType, (BYTE *)&dwValue, &dwSize ) )
    {
        if(   ( REG_DWORD == dwType ) 
           && ( sizeof( DWORD ) == dwSize ) 
           && (( dwValue == CSPROPERTY_BUFFER_DRIVER ) || ( dwValue == CSPROPERTY_BUFFER_CLIENT_LIMITED ) || ( dwValue == CSPROPERTY_BUFFER_CLIENT_UNLIMITED )))
        {
            for( int i=0; i<MAX_SUPPORTED_PINS ; i++ )
            {
                m_SensorModeInfo[i].MemoryModel = (CSPROPERTY_BUFFER_MODE) dwValue;
            }
        }
    }

    // Find out if we should be using some other number of supported modes. The only
    // valid options is 1 (capture)
    if ( ERROR_SUCCESS == RegQueryValueEx( hKey, L"PinCount", 0, &dwType, (BYTE *)&dwValue, &dwSize ) )
    {
        if ( REG_DWORD == dwType
             && sizeof ( DWORD ) == dwSize
             && 3 == dwValue )
        {
            m_ulCTypes = 3;
        }
    }

    RegCloseKey( hKey );
    return true;
}
