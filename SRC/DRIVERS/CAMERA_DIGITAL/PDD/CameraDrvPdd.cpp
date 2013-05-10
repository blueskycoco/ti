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
#include <omap3530.h>

#include "omap3530_irq.h"
//#include <omap3430.h>

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
#include "CameraPDD.h"
#include "CameraDrvPdd.h"
#include "wchar.h"
#include "util.h"
#include "module.h"
#include <omap_bus.h>
#include "cam_pdd.h"
#include "sdk_padcfg.h"
/*
#ifdef DEBUGMSG
#undef DEBUGMSG
#define DEBUGMSG RETAILMSG
#endif

#ifdef ZONE_FUNCTION
#undef ZONE_FUNCTION
#define ZONE_FUNCTION 1
#endif

#ifdef ZONE_IOCTL
#undef ZONE_IOCTL
#define ZONE_IOCTL 1
#endif*/


void WriteYuy2(unsigned char * lpBuffer, unsigned int size);
void ReadYuv2(PUCHAR sample);
void YbrToYrb(unsigned char * Buffer)
{
	unsigned int i=0;
	unsigned char temp = 0;
	for(i=0;i<640*480*2;i+=4)
		{
			/*
			temp = Buffer[i + 1];
			Buffer[i+1] = Buffer[i+3];
			Buffer[i+3] = temp;
			*/
			//Buffer[i+0] = Buffer[i+2] = Buffer[i+3];
			Buffer[i+1] = Buffer[i+3] = 0;
		}
}

void memcpy2(BYTE* pOut, BYTE* pIn, UINT num )
{
	//unsigned short * p16 =(unsigned short *)pOut;
	//unsigned int * p32 =(unsigned int *)pIn;
	int i=0;
	unsigned char tmp;
	//num = num /2;
	for(i=0;i<num;i+=4)
		{
		
		pOut[i]=pIn[i];
		pOut[i+2] = pIn[i+2];
		pOut[i+1] = pIn[i+1] ;
		pOut[i+3] = pIn[i+3] ;
		//pOut[i+1] = 0;
		
		}
}


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
    m_pIspCtrl = new CIspCtrl;
    //m_pTvpCtrl = new CTvpCtrl;
    m_hParent = NULL;
	m_InterruptThreadKilled = NULL;
	m_CapturestableFrameCount = 0;
	
    memset( &m_CsState, 0x0, sizeof(m_CsState));
    memset( &m_SensorModeInfo, 0x0, sizeof(m_SensorModeInfo));
    memset( &m_SensorProps, 0x0, sizeof(m_SensorProps));
    memset( &PowerCaps, 0x0, sizeof(PowerCaps));
}

CCameraPdd::~CCameraPdd()
{

	//disable interrupt
	InterruptDisable(m_CAMISPIntr);
	
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
	//release system interrupt
    KernelIoControl(IOCTL_HAL_RELEASE_SYSINTR, &m_CAMISPIntr, sizeof(UINT32), NULL ,0 ,NULL);
	//close m_CAMISPEvent
	CloseHandle(m_CAMISPEvent);		
	//close thread handle
	CloseHandle(m_hCCDCInterruptThread);
	m_InterruptThreadKilled = NULL;
	
		
	DEBUGMSG( ZONE_IOCTL, ( _T("~CCameraPdd\r\n")) );
}

DWORD CCameraPdd::PDDInit( PVOID MDDContext, PPDDFUNCTBL pPDDFuncTbl )
{
    DWORD dwIrq = IRQ_CAM0;

    m_hContext = (HANDLE)MDDContext;
    // Real drivers may want to create their context

    m_ulCTypes = 2; // Default number of Sensor Modes is 2

	RETAILMSG(1,(_T("wangchong:CCameraPdd::PDDInit\r\n")));
    
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
	RETAILMSG(1,(_T("wangchong:CCameraPdd::PDDInit Config Ok\r\n")));

	if (!RequestDevicePads(OMAP_DEVICE_CAMERA))
    {
        RETAILMSG(1, (L"ERROR: CAM_Init: Failed request pads\r\n"));
    }

	//EnableDeviceClocks(OMAP_DEVICE_CAMERA, TRUE);

	m_ulCTypes = 2;  //wangwj changed to test;
    m_pModeVideoFormat = NULL;
    // Allocate Video Format specific array.
    m_pModeVideoFormat = new PINVIDEOFORMAT[m_ulCTypes];
    if( NULL == m_pModeVideoFormat )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }

    // Video Format initialization
    // Video Format initialization
    m_pModeVideoFormat[CAPTURE].categoryGUID         = PINNAME_VIDEO_CAPTURE;
    m_pModeVideoFormat[CAPTURE].ulAvailFormats       = 5;//wangwj changed to 1
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[CAPTURE].ulAvailFormats];

    if( NULL == m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo )
        {
        return ERROR_INSUFFICIENT_BUFFER;
        }
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[1] = &DCAM_StreamMode_101; //320x240 QVGA
//    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[0] = &DCAM_StreamMode_102; //352x288 CIF
//    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[2] = &DCAM_StreamMode_103; //640x480  VGA
	// wangchong 2010-9-6
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[2] = &DCAM_StreamMode_102; //352x288 CIF
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[0] = &DCAM_StreamMode_103; //640x480  VGA
	m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[3] = &DCAM_StreamMode_104; //800x600 SVGA
    m_pModeVideoFormat[CAPTURE].pCsDataRangeVideo[4] = &DCAM_StreamMode_105; //1024x768 XGA

    m_pModeVideoFormat[STILL].categoryGUID           = PINNAME_VIDEO_STILL;
    m_pModeVideoFormat[STILL].ulAvailFormats         = 1;
    m_pModeVideoFormat[STILL].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[STILL].ulAvailFormats];



    if( NULL == m_pModeVideoFormat[STILL].pCsDataRangeVideo )
    {
        return ERROR_INSUFFICIENT_BUFFER;
    }
	//YUV2 1024*768 is ok,but YUV2 2048*1536 don't work,don't know why
	//m_pModeVideoFormat[STILL].pCsDataRangeVideo[0] = &DCAM_StreamMode_006; //2048x1536 QXGA	,not surpport YUV2,why????
//	m_pModeVideoFormat[STILL].pCsDataRangeVideo[0] = &DCAM_StreamMode_103;
	m_pModeVideoFormat[STILL].pCsDataRangeVideo[0] = &DCAM_StreamMode_107;	//ftm changed to DCAM_StreamMode_106 test
	if( 3 == m_ulCTypes )
    {
        m_pModeVideoFormat[PREVIEW].categoryGUID         = PINNAME_VIDEO_PREVIEW;
        m_pModeVideoFormat[PREVIEW].ulAvailFormats       = 1;	//ftm changed to 5
        m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo = new PCS_DATARANGE_VIDEO[m_pModeVideoFormat[PREVIEW].ulAvailFormats];

        if( NULL == m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo )
        {
            return ERROR_INSUFFICIENT_BUFFER;
        }
//	    m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[0] = &DCAM_StreamMode_101; //320x240 QVGA
//	    m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[1] = &DCAM_StreamMode_102; //352x288 CIF
//	    m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[2] = &DCAM_StreamMode_103; //640x480  VGA
		// wangchong 2010-9-6
	    //m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[2] = &DCAM_StreamMode_101; //320x240 QVGA
	    //m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[1] = &DCAM_StreamMode_102; //352x288 CIF
	    m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[0] = &DCAM_StreamMode_103; //640x480  VGA

		//m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[3] = &DCAM_StreamMode_104; //800x600 SVGA
	    //m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[4] = &DCAM_StreamMode_105; //1024x768 XGA	

		 //m_pModeVideoFormat[PREVIEW].pCsDataRangeVideo[5] = &DCAM_StreamMode_107; //1600x1200 UXGA	
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
    m_pModeVideoCaps[CAPTURE].CurrentVideoControlCaps     = DefaultVideoControlCaps[CAPTURE];
    m_pModeVideoCaps[STILL].DefaultVideoControlCaps       = DefaultVideoControlCaps[STILL];
    m_pModeVideoCaps[STILL].CurrentVideoControlCaps       = DefaultVideoControlCaps[STILL];
    if( 3 == m_ulCTypes )
    {
        // Note PREVIEW control caps are the same, so we don't differentiate
        m_pModeVideoCaps[PREVIEW].DefaultVideoControlCaps     = DefaultVideoControlCaps[PREVIEW];
        m_pModeVideoCaps[PREVIEW].CurrentVideoControlCaps     = DefaultVideoControlCaps[PREVIEW];
    }

    m_SensorModeInfo[CAPTURE].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
    m_SensorModeInfo[CAPTURE].MaxNumOfBuffers = 3;
    m_SensorModeInfo[CAPTURE].PossibleCount = 1;
    m_SensorModeInfo[STILL].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
    m_SensorModeInfo[STILL].MaxNumOfBuffers = 1;
    m_SensorModeInfo[STILL].PossibleCount = 1;
    if( 3 == m_ulCTypes )
    {
        m_SensorModeInfo[PREVIEW].MemoryModel = CSPROPERTY_BUFFER_CLIENT_UNLIMITED;
        m_SensorModeInfo[PREVIEW].MaxNumOfBuffers = 3;
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

    if (!m_CAMISPEvent)
        {
        ERRORMSG(ZONE_ERROR, (_T("Failed to CreateEvent(m_CAMISPEvent) \r\n")));
        goto clean;
        }

    if(!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR,
        &dwIrq, sizeof(dwIrq), &m_CAMISPIntr, sizeof(DWORD), NULL))
        {
        ERRORMSG(ZONE_ERROR, (_T("Failed to KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR)\r\n")));
        m_CAMISPIntr = 0;
        }

    if (m_CAMISPIntr == 0)
        {
        ERRORMSG(ZONE_ERROR, (_T(" Failed to allocate camera system interrupt event, m_CAMISPIntr\r\n") ));
        goto clean;
        }

    if (!InterruptInitialize(m_CAMISPIntr, m_CAMISPEvent, NULL, 0))
        {
        ERRORMSG(ZONE_ERROR, (_T(" Failed to InterruptInitialize(m_CAMISPIntr) \r\n")));
        goto clean;
        }

    InterruptDone( m_CAMISPIntr );

    // Get Handle to the ROOTBUS
    m_hParent= CreateBusAccessHandle((LPCTSTR)MDDContext);
    if (m_hParent == NULL)
        {
        DEBUGMSG(ZONE_ERROR, (L"ERROR: CAM_Init: Failed open bus driver\r\n"));
        goto clean;
        }

    // Set the camera Power state to D0
    SetDevicePowerState(m_hParent, D0, NULL);
/*
    if (!BusClockRequest(m_hParent, OMAP_DEVICE_CSI2))
        {
        DEBUGMSG(ZONE_ERROR,(L"Failed to set the CSI2 FCLK\r\n"));
        goto clean;
        }
*/

    m_hCCDCInterruptThread = CreateThread( NULL,
                                              0,
                      (LPTHREAD_START_ROUTINE)CameraInterruptThread,
                                              this,
                                              0,
                                              NULL); //CREATE_SUSPENDED
	RETAILMSG(1,(_T("wangchong:m_pIspCtrl->InitializeCamera()\r\n")));
	if(m_pIspCtrl->InitializeCamera() == FALSE) //Init. ISP 
		return FALSE;

    //RETAILMSG(1,(_T("wangchong:m_pIspCtrl->InitializeCamera OK111...\r\n")));
    //m_pIspCtrl->EnableCamera(VGA, 640, 480);//wangwj added to test	//ftm del test
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

    return pThis->CameraInterruptThreadImpl();
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
	RETAILMSG(1,(TEXT("#wangwj#------------------------CameraInterruptThreadImpl---------------------------------\r\n")));
    do
    {       
        /* wait for start */
		//RETAILMSG(1,(TEXT("#wangwj#---------CameraInterruptThreadImpl-------------111----\r\n")));
        dwEventStatus = WaitForSingleObject(m_CAMISPEvent, INFINITE);
        if (dwEventStatus==WAIT_TIMEOUT)
        {
        RETAILMSG(1,(TEXT("#wangwj#---------CameraInterruptThreadImpl-------------222----\r\n")));
            DEBUGMSG(ZONE_FUNCTION,(_T("CameraInterruptThreadImpl:Camera thread quit\r\n")));  
			m_InterruptThreadKilled = ~NULL;
			return ERROR_SUCCESS;
        } 
        else
        {              
        RETAILMSG(1,(TEXT("*")));
            setting=INREG32(&m_pIspCtrl->GetCCDCRegs()->CCDC_SYN_MODE);               
            setting = INREG32(&m_pIspCtrl->GetIspCfgRegs()->IRQ0STATUS);
            DEBUGMSG(0,(_T("IRQ0: 0x%08x\r\n"), setting));
        //    if(setting & 0x80000000) //VD detected
        //	if(setting & 0x01000000) //resizer done detected
            {
                //DWORD dwTime=GetTickCount();
                if(m_bStillCapInProgress)
                    HandleStillInterrupt(STILL);                    
                else if(m_CsState[CAPTURE] == CSSTATE_RUN)
                    HandleCaptureInterrupt(CAPTURE);
				else if(m_CsState[PREVIEW] == CSSTATE_RUN)
                    HandleCaptureInterrupt(PREVIEW); 
                //dwTime=GetTickCount()-dwTime;
                //DEBUGMSG(ZONE_VERBOSE,(_T("dwTime=%d \r\n", dwTime)));
             }
                
            OUTREG32(&m_pIspCtrl->GetIspCfgRegs()->IRQ0STATUS, setting);     
            InterruptDone(m_CAMISPIntr);
        }
 //RETAILMSG(1,(TEXT("#wangwj#---------CameraInterruptThreadImpl-------------444----\r\n")));
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
//added special attributes.
DWORD CCameraPdd::HandleVidProcAmpChanges( DWORD dwPropId, LONG lFlags, LONG lValue )
{
    PSENSOR_PROPERTY pDevProp = NULL;

    pDevProp = m_SensorProps + dwPropId;
    
   /* if( CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL == lFlags )
    {
        pDevProp->ulCurrentValue = lValue;
    }*/
    if( CSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL == lFlags )
    {
        pDevProp->ulCurrentValue = lValue;
        switch(dwPropId)
        {
          	case ENUM_BRIGHTNESS: 
          		ModuleSetBrightness(lValue);
            	break;
          	case ENUM_CONTRAST:
            	ModuleSetContrast(lValue);
            	break;
			case ENUM_HUE: 
            	break;
          	case ENUM_SATURATION:
            	ModuleSetSaturation(lValue);
			case ENUM_SHARPNESS: 
            	break;
          	case ENUM_GAMMA:
            	break;
			case ENUM_COLORENABLE: 
            	break;
          	case ENUM_WHITEBALANCE:
            	break;
			// CameraControl
			case ENUM_PAN:
				break;
			case ENUM_TILT: 
            	break;
          	case ENUM_ROLL:
            	break;
			case ENUM_ZOOM: 
            	break;
          	case ENUM_IRIS:	
            	break;
			case ENUM_EXPOSURE:	
            	break;
			case ENUM_FOCUS:
				//can start focusing,when taked still pic,stop.
				//SingleFocus(TRUE);
				SingleFocus(TRUE); // wangchong 2010-9-3
            	break;
			case ENUM_FLASH:
            	break;		
          	default:
            	RETAILMSG(1,(L"VIDPROCAMP:PropId:%d not supported\r\n",dwPropId));
        }
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
    return E_NOTIMPL;
}

DWORD CCameraPdd::HandleAdapterCustomProperties( PUCHAR pInBuf, DWORD  InBufLen, PUCHAR pOutBuf, DWORD  OutBufLen, PDWORD pdwBytesTransferred )
{
    DEBUGMSG( ZONE_FUNCTION, ( _T("IOControl Adapter PDD: Unsupported PropertySet Request\r\n")) );
    return ERROR_NOT_SUPPORTED;
}

DWORD CCameraPdd::InitSensorMode( ULONG ulModeType, LPVOID ModeContext )
{
    ASSERT( ModeContext );
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
	RETAILMSG(1,(TEXT("#wangwj#-------------------Run----------\r\n")));
	if(m_CsState[lModeType] != CSSTATE_RUN)
//    if(lModeType==CAPTURE && m_CsState[lModeType] != CSSTATE_RUN)
    {
        if(!m_pIspCtrl->EnableCamera(m_VideoRes[lModeType],s_tblVideoFormat[m_VideoRes[lModeType]].width,s_tblVideoFormat[m_VideoRes[lModeType]].height))
            return ERROR_INVALID_PARAMETER;         
    }            
    m_CsState[lModeType] = CSSTATE_RUN;     
	RETAILMSG(1,(TEXT("#wangwj#-------------------Run--over--------\r\n")));
    return  ERROR_SUCCESS;            
}

DWORD CCameraPdd::Pause(ULONG lModeType, CSSTATE csState)
{   
    DEBUGMSG(ZONE_FUNCTION,(_T("SetSensorState: CSSTATE_PAUSE: lModeType=%d, csState=%d \r\n"),lModeType,csState));
	if(m_CsState[lModeType] == CSSTATE_RUN)
//    if(lModeType==CAPTURE && (m_CsState[lModeType] != CSSTATE_PAUSE && m_CsState[lModeType] != CSSTATE_STOP))
    {
        if(!m_pIspCtrl->PauseCamera())
            return ERROR_INVALID_PARAMETER;                     
    }           
    m_CsState[lModeType] = CSSTATE_PAUSE;
	RETAILMSG(1, (L"lModeType %d CCameraPdd::Pause OK\r\n",lModeType));
    return  ERROR_SUCCESS;
}

DWORD CCameraPdd::Stop(ULONG lModeType, CSSTATE csState)
{
    DEBUGMSG(ZONE_FUNCTION,(_T("SetSensorState: CSSTATE_STOP: lModeType=%d, csState=%d \r\n"),lModeType,csState));
    if(m_CsState[lModeType] != CSSTATE_STOP)
//    if(lModeType==CAPTURE && (m_CsState[lModeType] != CSSTATE_PAUSE && m_CsState[lModeType] != CSSTATE_STOP))
    {
        if(!m_pIspCtrl->DisableCamera())
            return ERROR_INVALID_PARAMETER;                     
    }           
    m_CsState[lModeType] = CSSTATE_STOP;
	
    if( STILL == lModeType )
    {
        m_bStillCapInProgress = false;
    }

    return  ERROR_SUCCESS;
}
    
DWORD CCameraPdd::SetSensorState( ULONG lModeType, CSSTATE csState )
{
    DWORD dwError = ERROR_SUCCESS;

	RETAILMSG(1, (L"SetSensorState:lModeType=%d,csState=%d\r\n", lModeType, csState));

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
    DWORD dwError = ERROR_SUCCESS;
	static CHAR times = 0;
	DEBUGMSG( ZONE_FUNCTION, ( _T("CCameraPdd::TakeStillPicture\r\n")) );	
	//fucosing......
	//in the process,must getting pic for preview!!!!
	//so stop capture first,then do other things.
/*
	ModuleSetLightMode(times++);
	if(m_SensorProps[ENUM_FLASH].ulCurrentValue)//if enable flash	
	{
		ModuleStrobeFlash(1);//start flash
	}
*/
	if(SingleFocus(TRUE) != TRUE)
	{
		ModuleStrobeFlash(FALSE);
		DEBUGMSG( ZONE_FUNCTION, ( _T("CCameraPdd::TakeStillPicture SingleFocus FAILED\r\n")) );	
		RETAILMSG(1, ( _T("wangchong->CCameraPdd::TakeStillPicture SingleFocus FAILED\r\n")) );
		return !ERROR_SUCCESS;
	}
    //Ignore pBurstModeInfo    
	if(m_CsState[CAPTURE] == CSSTATE_RUN)
		SetSensorState(CAPTURE,CSSTATE_PAUSE);
	if(m_CsState[PREVIEW] == CSSTATE_RUN)
		SetSensorState(PREVIEW,CSSTATE_PAUSE);
	m_bStillCapInProgress = true;
    m_StillstableFrameCount = 0;
	SetSensorState(STILL,CSSTATE_RUN);
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
    UINT biHeight       = abs(pCsVideoInfoHdr->bmiHeader.biHeight);
    UINT biSizeImage    = pCsVideoInfoHdr->bmiHeader.biSizeImage;
    UINT biWidthBytes   = CS_DIBWIDTHBYTES (pCsVideoInfoHdr->bmiHeader);
    UINT biBitCount     = pCsVideoInfoHdr->bmiHeader.biBitCount;
    UINT LinesToCopy    = abs (biHeight);
    DWORD biCompression = pCsVideoInfoHdr->bmiHeader.biCompression;	
	DWORD dwResIndex = 0;

	RETAILMSG(1,(_T("CCameraPdd::SetSensorModeFormat: ulModeType:=%d, biWidth=%d, biHeight=%d, biSizeImage=%d , biCompression=%d\r\n"), ulModeType, pCsVideoInfoHdr->bmiHeader.biWidth, pCsVideoInfoHdr->bmiHeader.biHeight, pCsVideoInfoHdr->bmiHeader.biSizeImage, pCsVideoInfoHdr->bmiHeader.biCompression));
    memcpy( &m_pCurrentFormat[ulModeType], pCsDataRangeVideo, sizeof ( CS_DATARANGE_VIDEO ) );

	for(; dwResIndex < N_MAX_RES; dwResIndex++)
	{
        if((biWidth  == s_tblVideoFormat[dwResIndex].width)&&
           (biHeight == s_tblVideoFormat[dwResIndex].height))
            {
            break;   
            }
	}
    
    if (dwResIndex < N_MAX_RES)
	{
        m_VideoRes[ulModeType] = (CAMRES)dwResIndex;	//save resolution index
	}
    else
	{
        DEBUGMSG(ZONE_ERROR,(L"SetModeFormat did not match res\r\n"));
	}
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
    DEBUGMSG( ZONE_IOCTL, ( _T("CCameraPdd::HandleSensorModeCustomProperties: Unsupported PropertySet Request\r\n")) );
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
/*
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
*/
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
//#endif //ENABLE_PACK8

DWORD CCameraPdd::YUVBufferFill( ULONG ulModeType, PUCHAR pImage )
{
    PCS_VIDEOINFOHEADER pCsVideoInfoHdr = &m_pCurrentFormat[ulModeType].VideoInfoHeader;

    UINT biWidth        =   pCsVideoInfoHdr->bmiHeader.biWidth;
    UINT biHeight       =   abs(pCsVideoInfoHdr->bmiHeader.biHeight);
    UINT biSizeImage    =   pCsVideoInfoHdr->bmiHeader.biSizeImage;
    DWORD biCompression =   pCsVideoInfoHdr->bmiHeader.biCompression;
    biCompression &= ~BI_SRCPREROTATE;

    if ( ( FOURCC_YUY2 != biCompression ) && 
         ( FOURCC_YV12 != biCompression ) )
    {
        return -1;
    }

    // make the box width/height 1/8th of the screen, so it's visible but doesn't
    // overwhelm
    UINT uiBoxWidth = biWidth/BOXWIDTHDIVIDER;
    UINT uiBoxHeight = biHeight/BOXHEIGHTDIVIDER;

    float fWidthStep = (float) (biWidth - uiBoxWidth)/LOCATIONWIDTHMASK;
    float fHeightStep = (float) (biHeight - uiBoxHeight)/LOCATIONHEIGHTMASK;

    UINT x, y;
    UINT uiTickCount;
    UINT uiDirection;
    UINT uiLocationX, uiLocationY;

    uiTickCount = GetTickCount() >> SPEEDSHIFT;

    // and the result with 0x3 so the value ranges between 0 and 3 for the direction.
    uiDirection = (uiTickCount >> LOCATIONSHIFT) & 0x3;

    uiLocationX = static_cast<UINT>(fWidthStep * (uiTickCount & LOCATIONWIDTHMASK));
    uiLocationY = static_cast<UINT>(fHeightStep * (uiTickCount & LOCATIONHEIGHTMASK));

    switch (uiDirection)
    {
        case 0:
            x = uiLocationX;
            y = 0;
            break;
        case 1:
            x = biWidth - uiBoxWidth;
            y = uiLocationY;
            break;
        case 2:
            x = (biWidth - uiBoxWidth) - uiLocationX;
            y = biHeight - uiBoxHeight;
            break;
        case 3:
            x = 0;
            y = (biHeight - uiBoxHeight) - uiLocationY;
            break;
    }
    
    if (MAKEFOURCC('Y','V','1','2') == biCompression )
    {
        UINT uiStride = (2 * biSizeImage) / (3 * biHeight);
        UINT uiDelta = uiStride-biWidth;
        UINT uiSizeYPlane = uiStride * biHeight;
        UINT uiHalfStride = uiStride >> 1;
        UINT uiHalfWidth = biWidth >> 1;
        UINT uiHalfDelta = uiHalfStride - uiHalfWidth;

        // 1. Make the entire image white.

        // Y plane
        for (UINT i = 0; i < uiSizeYPlane; i += uiStride)
        {
            memset(&pImage[i], Y_WHITE, biWidth);
            memset(&pImage[i+biWidth], 0, uiDelta);
        }

        // V & U planes
        for (; i < biSizeImage; i += uiHalfStride)
        {
            memset(&pImage[i], VU_BOTH, uiHalfWidth);
            memset(&pImage[i+uiHalfWidth], 0, uiHalfDelta);
        }

        // 2. Add the black square.  Since the chroma (V & U) values are the same
        //    for both black and white, we only have to touch the luma (Y) plane.
        UINT uiBound = ((y + uiBoxHeight) * uiStride) + (x + uiBoxWidth);

        for (i = (y * uiStride) + x; i < uiBound; i += uiStride)
        {
            memset(&pImage[i], Y_BLACK, uiBoxWidth);
        }
    }
    else if (MAKEFOURCC('Y','U','Y','2') == biCompression )
    {
        UINT uiStrideDWORD = (biSizeImage / biHeight) / sizeof(DWORD);
        UINT uiWidthDWORD = biWidth / 2;
        UINT uiDeltaDWORD = uiStrideDWORD - uiWidthDWORD;

        // upcast should be safe because images are dword aligned
        DWORD* pdwImage = reinterpret_cast<DWORD*>(pImage);
        ASSERT(pdwImage);
        if (!pdwImage)
        {
            return 0;
        }

        // paint the background color first
        for (UINT i = 0; i < biHeight; i++)
        {
            for (UINT j = 0; j < uiWidthDWORD; j++)
#if 0
                *pdwImage++ = MACROPIXEL_WHITE;
#else
                *pdwImage++ = MACROPIXEL_RED;
#endif

            for (j = 0; j < uiDeltaDWORD; j++)
                *pdwImage++ = 0;
        }

        // then add the square
        UINT uiBoundDWORD = ((y + uiBoxHeight) * uiStrideDWORD) + ((x + uiBoxWidth) / 2);
        UINT uiBoxWidthDWORD = uiBoxWidth / 2;

        pdwImage = reinterpret_cast<DWORD*>(pImage);
        pdwImage += (y * uiStrideDWORD) + (x / 2);

        for (i = 0; i < uiBoxHeight; i++)
        {
            for (UINT j = 0; j < uiBoxWidthDWORD; j++)
#if 0
                *pdwImage++ = MACROPIXEL_BLACK;
#else
                *pdwImage++ = MACROPIXEL_GRN;
#endif

            pdwImage += (uiStrideDWORD - uiBoxWidthDWORD);
        }
    }

    return(biSizeImage);
}

//------------------------------------------------------------------------------
//
//  Saturate2Byte
//  
//  Description: This is a helper function to crop the pixel value to 255.
//               This function is used by YUV2RGB565
//
inline UINT8 Saturate2Byte(
    int value
    )
{
    if (value < 0)
      value = 0;
    if (value > 255)
      value = 255;

    return value;
}


//------------------------------------------------------------------------------
//
//  ColorConversion24To16
//  
//  Description: This is a helper function to convert RGB888 --> RGB 565.
//               This function is used by YUV2RGB565
//
DWORD ColorConversion24To16(
    PUCHAR pBits24, 
    ULONG ulSize, 
    PUCHAR pBits16
    )
{
    int nPos16 = 0;
    unsigned short RGB2Bytes = 0;
    
    BYTE Red24   = pBits24[2]; // 8-bit red   [2]
    BYTE Green24 = pBits24[1]; // 8-bit green
    BYTE Blue24  = pBits24[0]; // 8-bit blue      [0]
  
    BYTE Red16   = Red24   >> 3;  // 5-bit red
    BYTE Green16 = Green24 >> 2;  // 6-bit green
    BYTE Blue16  = Blue24  >> 3;  // 5-bit blue

    RGB2Bytes = Blue16 + (Green16<<5) + (Red16<<(5+6));

    pBits16[0] = RGB2Bytes & 0xFF;          //LOBYTE(RGB2Bytes);
    pBits16[1] = (RGB2Bytes >> 8) & 0xFF;     //HIBYTE(RGB2Bytes);
  
    return ERROR_SUCCESS;
}


#define LIMIT(color) \
	(unsigned char)((color > 0xff)?0xff:((color < 0)?0:color))


static void yuvconvert(PUCHAR yuv, PUCHAR rgb,int ulSize)
{
	int y, u, v, r, g, b, y1;
	for(int i=0;i<ulSize;i +=4)
	{
		y = (*yuv++ - 16) * 76310;
		u = *yuv++ - 128;
		y1 = (*yuv++ - 16) * 76310;
		v = *yuv - 128;
		r = 104635 * v;
		g = -25690 * u + -53294 * v;
		b = 132278 * u;

		*rgb++ = ((LIMIT(g+y) & 0xfc) << 3) | (LIMIT(b+y) >> 3);
		*rgb++ = (LIMIT(r+y) & 0xf8) | (LIMIT(g+y) >> 5);
		*rgb++ = ((LIMIT(g+y1) & 0xfc) << 3) | (LIMIT(b+y1) >> 3);
		*rgb++ = (LIMIT(r+y1) & 0xf8) | (LIMIT(g+y1) >> 5);		
	}
}
  
//------------------------------------------------------------------------------
//
//  YUV2RGB565
//  
//  Description: This is a helper function to convert YUV format image to RGB565
//
DWORD YUV2RGB565(
    PUCHAR pYuvBuffer,
    PUCHAR pImage, 
    ULONG ulSize
    )
{    
		register INT32 r,g,b;
		register INT32 y0,y1,cb,cr;
		register INT32 temp,temp1,temp2,temp3,temp4;
		UINT32 srcPixels = 0, dstPixels = 0;
		UINT8 RGB[3];
		UINT32 BPP = ulSize; //Take 4 bytes/2 pixels (UYVY)
		
		for (; srcPixels < BPP; srcPixels = srcPixels+4, dstPixels = dstPixels + 4)
			{
/*
			cb = pYuvBuffer[srcPixels];
			y0 = pYuvBuffer[srcPixels+1];
			cr = pYuvBuffer[srcPixels+2];
			y1 = pYuvBuffer[srcPixels+3];
*/

			y0 = pYuvBuffer[srcPixels];
			cb = pYuvBuffer[srcPixels+1];
			y1 = pYuvBuffer[srcPixels+2];
			cr = pYuvBuffer[srcPixels+3];

			y0 -= 16;
			cr -= 128;
			cb -= 128;
			temp = (19070 * y0) >> 14;
			temp1 = ((26148 * cr) >> 14);
			temp2 = ((13320 * cr) >> 14);
			temp3 = ((33062 * cb) >> 14);
			temp4 = ((6406 * cb) >> 14);
			r = temp + temp1;
			g = temp - temp2 - temp4;
			b = temp + temp3;
	
			if (r > 255)
			  r = 255;
			if (g > 255)
			  g = 255;
			if (b > 255)
			  b = 255;


			RGB[0] = Saturate2Byte(b);
			RGB[1] = Saturate2Byte(g);
			RGB[2] = Saturate2Byte(r);
		
			/* convert from RGB888(24) to RGB565(16) - Lower 16 bits */
			ColorConversion24To16((PUCHAR)&RGB, ulSize, (PUCHAR)(pImage + dstPixels)); 
	
			y1 -= 16;
			temp = (19070 * y1) >> 14;
			r = temp + temp1;
			g = temp - temp2 - temp4;
			b = temp + temp3;
	
			RGB[0] = Saturate2Byte(b);
			RGB[1] = Saturate2Byte(g);
			RGB[2] = Saturate2Byte(r);
	
			/* convert from RGB888(24) to RGB 565(16) - Higher 16 bits */
			ColorConversion24To16((PUCHAR)&RGB, ulSize, (PUCHAR)(pImage + dstPixels + 2));
			}
		return ERROR_SUCCESS; 
	}


/*------------------------
yuv422 to rgb565
转换公式：
R=Y+1.4075*(V-128)
G=Y-0.3455*(U-128) - 0.7169*(V-128)
B=Y+1.779*(U-128)
为了加快运算速度，采用下面的整形计算法：
u = YUVdata[UPOS] - 128;
v = YUVdata[VPOS] - 128;

rdif = v + ((v * 103) >> 8);
invgdif = ((u * 88) >> 8) +((v * 183) >> 8);
bdif = u +( (u*198) >> 8);

r = YUVdata[YPOS] + rdif;
g = YUVdata[YPOS] - invgdif;
b = YUVdata[YPOS] + bdif;
r=r>255?:255:(r<0:?0:r);
g=g>255?:255:(g<0:?0:g);
b=b>255?:255:(b<0:?0:b);
以上得到的是rgb888的数据，再将rgb888转为rgb555
RGBdata[1] =( (r & 0xF8) | ( g >> 5) );
RGBdata[0] =( ((g & 0x1C) << 3) | ( b >> 3) );
422 FORMAT size:
size=width*height*2;
YSIZE = size/2;
USIZE = size/4;
VSIZE = size/4;
YPOS=0;
UPOS=YPOS + size/2;
VPOS=UPOS + size/4;

--------------------------*/
int convertyuv422torgb565(unsigned char *inbuf,unsigned char *outbuf,int width,int height)
{
  int rows,cols,rowwidth;
  int y,u,v,r,g,b,rdif,invgdif,bdif;
  int size;
  unsigned char *YUVdata,*RGBdata;
  int YPOS,UPOS,VPOS;

  YUVdata = inbuf;
  RGBdata = outbuf;

  rowwidth = width>>1;
  size=width*height*2;
  YPOS=0;
  UPOS=YPOS + size/2;
  VPOS=UPOS + size/4;

  for(rows=0;rows<height;rows++)
  {
    for(cols=0;cols<width;cols++) 
    {
 u = YUVdata[UPOS] - 128;
 v = YUVdata[VPOS] - 128;

 rdif = v + ((v * 103) >> 8);
 invgdif = ((u * 88) >> 8) +((v * 183) >> 8);
 bdif = u +( (u*198) >> 8);

 r = YUVdata[YPOS] + rdif;
 g = YUVdata[YPOS] - invgdif;
 b = YUVdata[YPOS] + bdif;
 r=r>255?255:(r<0?0:r);
 g=g>255?255:(g<0?0:g);
 b=b>255?255:(b<0?0:b);
    
 *(RGBdata++) =( ((g & 0x1C) << 3) | ( b >> 3) );
 *(RGBdata++) =( (r & 0xF8) | ( g >> 5) );

 YPOS++;      
 
 if(cols & 0x01)
 {
    UPOS++;
    VPOS++;      
 } 
    }
    if((rows & 0x01)== 0)
    {
 UPOS -= rowwidth;
 VPOS -= rowwidth;
    }
  }
  return 1;
}

DWORD CCameraPdd::FillBuffer( ULONG ulModeType, PUCHAR pImage )
{
    PCS_VIDEOINFOHEADER pCsVideoInfoHdr = &m_pCurrentFormat[ulModeType].VideoInfoHeader;
	//RETAILMSG(1,(TEXT("FillBuffer----GetFrameBuffer=%x ,pImage=%x...\r\n"),m_pIspCtrl->GetFrameBuffer(),pImage));
    UINT biWidth        = pCsVideoInfoHdr->bmiHeader.biWidth;
    UINT biHeight       = pCsVideoInfoHdr->bmiHeader.biHeight;
    UINT biSizeImage    = pCsVideoInfoHdr->bmiHeader.biSizeImage;
    UINT biWidthBytes   = CS_DIBWIDTHBYTES (pCsVideoInfoHdr->bmiHeader);
    UINT biBitCount     = pCsVideoInfoHdr->bmiHeader.biBitCount;
    UINT LinesToCopy    = abs (biHeight);
    DWORD biCompression = pCsVideoInfoHdr->bmiHeader.biCompression;
	UINT i = 0;

    HBITMAP hbmp = NULL;
    HDC hdc = NULL;

//	static UINT dwCount = 0;
//	DWORD dwRet;
//	static HANDLE hFile = NULL;

	// wangchong 2010-9-6   
	DEBUGMSG(0,(_T("CCameraPdd::FillBuffer:\r\n biSize=%d,\r\nbiWidth=%d,\r\n 	biHeight=%d,\r\n biPlanes=%d,\r\nbiBitCount=%d,\r\nbiCompression=%d\r\n	biSizeImage=%d,\r\nbiXPelsPerMeter=%d,\r\nbiYPelsPerMeter=%d,\r\nbiClrUsed=%d,\r\nbiClrImportant=%d,\r\npImage=0x%08X\r\n"),
            pCsVideoInfoHdr->bmiHeader.biSize,
            pCsVideoInfoHdr->bmiHeader.biWidth, pCsVideoInfoHdr->bmiHeader.biHeight, 
            pCsVideoInfoHdr->bmiHeader.biPlanes,
            pCsVideoInfoHdr->bmiHeader.biBitCount,pCsVideoInfoHdr->bmiHeader.biCompression,
            pCsVideoInfoHdr->bmiHeader.biSizeImage,
            pCsVideoInfoHdr->bmiHeader.biXPelsPerMeter,pCsVideoInfoHdr->bmiHeader.biYPelsPerMeter,
            pCsVideoInfoHdr->bmiHeader.biClrUsed,pCsVideoInfoHdr->bmiHeader.biClrImportant, pImage));
/*
	if(FOURCC_YUY2 == (biCompression & ~BI_SRCPREROTATE))
		RETAILMSG(1,(TEXT("Judge fotmat :YUY2\r\n")));
	if(FOURCC_YV12 == (biCompression & ~BI_SRCPREROTATE))
		RETAILMSG(1,(TEXT("Judge fotmat :YV12\r\n")));
*/
//	YbrToYrb((unsigned char *)m_pIspCtrl->GetFrameBuffer());
	
	//RETAILMSG(1,(TEXT("WORD size = %d, uLong size = %d\r\n"),sizeof(WORD),sizeof(ULONG)));
	if ( (FOURCC_YUY2 == (biCompression & ~BI_SRCPREROTATE)) ||
         (FOURCC_YV12 == (biCompression & ~BI_SRCPREROTATE)) )
    {
   // 	DEBUGMSG(1,(_T("+YUVBufferFill \r\n")));

				//RETAILMSG(1,(TEXT("Write+\r\n")));
		//WriteYuy2((PUCHAR)pImage,biSizeImage);
		
		//RETAILMSG(1,(TEXT("Read+\r\n")));
		//ReadYuv2(pImage); 
		//UYVYtoYUY2((WORD *)m_pIspCtrl->GetFrameBuffer(), pImage,biSizeImage);	
		//memcpy(pImage,(PUCHAR)sample,biSizeImage);
		//memcpy(pImage,(PUCHAR)sample,biSizeImage);
		
//		memcpy2(pImage,(PUCHAR)m_pIspCtrl->GetFrameBuffer(),biSizeImage);
		memcpy(pImage,(PUCHAR)m_pIspCtrl->GetFrameBuffer(),biSizeImage);

#if 0
		if(dwCount == 0)
		{
			hFile = CreateFile(TEXT("\\Storage Card\\pic.yuv"), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
			if(hFile == INVALID_HANDLE_VALUE)
			{
				RETAILMSG(1, (L"cam:create file error\r\n"));
			}
		}

		if( hFile != INVALID_HANDLE_VALUE)
		{
			if(dwCount > 100)
			{
				if((dwCount%80 == 0) && dwCount < (100 + 800))
				{
					if(!WriteFile(hFile, pImage, biSizeImage, &dwRet, NULL))
					{
						RETAILMSG(1, (L"cam:write file failed\r\n"));
					}
				}
			}
		}
		dwCount++;
		if(dwCount > 100 + 800)
		{
			CloseHandle(hFile);
			RETAILMSG(1, (L"cam:collect pics finished\r\n"));
		}
#endif

		//WriteYuy2((PUCHAR)pImage,biSizeImage);
		/*
		RETAILMSG(1,(TEXT("@@@@@@@@@@@@@")));
		for(i=0;i<biSizeImage;i++)
			RETAILMSG(1,(TEXT("%x"),*(pImage + i)));

		RETAILMSG(1,(TEXT("@@@@@@@@@@@@@")));
		while(1);
		*/
    	return biSizeImage;
    }
	//too slow
	RETAILMSG(1,(TEXT("error XXXXXXXXXXXXXXXXXXXXXXXX\r\n")));
//    convertyuv422torgb565(pImage, (PUCHAR)m_pIspCtrl->GetFrameBuffer() ,640 ,480 );
    YUV2RGB565((PUCHAR)m_pIspCtrl->GetFrameBuffer(), pImage, biSizeImage); //wangwj del
    return biSizeImage;
}


void CCameraPdd :: HandleCaptureInterrupt( UINT ulModeTypeIn )
{
    ULONG ulModeType;
	DEBUGMSG(0,(_T("+HandleCaptureInterrupt ulModeTypeIn=%d\r\n"),ulModeTypeIn));
//	RETAILMSG(1,(_T("wangchong:+HandleCaptureInterrupt ulModeTypeIn=%d\r\n"),ulModeTypeIn));
	if( m_CapturestableFrameCount == 0 )//siscard one frame
    {
    	m_CapturestableFrameCount++;
        return;
    }
	
    if( ulModeTypeIn == CAPTURE)
    {
        ulModeType = CAPTURE;
    }
    else if ( m_ulCTypes == 3 && ulModeTypeIn == PREVIEW )
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


void CCameraPdd :: HandleStillInterrupt( UINT ulModeTypeIn )
{
    DEBUGMSG(0,(_T("+HandleStillInterrupt \r\n")));  
	RETAILMSG(1,(_T("wangchong:+HandleStillInterrupt \r\n")));  
	if(m_StillstableFrameCount <2)//to get stable picture,usually 3rd frame
    {
    	m_StillstableFrameCount++;
    	return;
	}
//	if(m_SensorProps[ENUM_FLASH].ulCurrentValue)
	{
		ModuleStrobeFlash(FALSE);//stop flash	
	}
	m_StillstableFrameCount = 0;	
    SetSensorState(STILL,CSSTATE_PAUSE);			
	//stop first then getting picture,not use stop!!!!,becuase stop will turn auto focus off,
	//it may take longer time,so plan a trick.
    m_CsState[STILL] = CSSTATE_STOP;
    MDD_HandleIO( m_ppModeContext[STILL], STILL );	//time is long
    SingleFocus(FALSE);
    m_bStillCapInProgress = false;		
    m_StillstableFrameCount = 0;
	m_CapturestableFrameCount = 0;					//return to capture,discard first frame
	//recover
	if(m_CsState[CAPTURE] == CSSTATE_PAUSE)
		SetSensorState(CAPTURE,CSSTATE_RUN);
	if(m_CsState[PREVIEW] == CSSTATE_PAUSE)
		SetSensorState(PREVIEW,CSSTATE_RUN);	
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
    // valid options are 2 or 3. Default to 2.
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


void WriteYuy2(unsigned char * lpBuffer, unsigned int size)
{
	HANDLE hFile;
	DWORD dwRet;

	HANDLE hFile2;
	//DWORD dwRet;
	//unsigned char lpBuffer[704*480*2];
	//unsigned char lpBuffer[2];
	unsigned int i ,j= 0;


	hFile = CreateFile(TEXT("\\yuv.yuv"), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

	if(hFile == INVALID_HANDLE_VALUE){
			RETAILMSG(1, (TEXT("CreateFile yuv 222ERROR(%d)\r\n"), GetLastError()));
		}
	if(!WriteFile(hFile, lpBuffer, size, &dwRet, NULL)){
			RETAILMSG(1, (TEXT("CreateFile yuv ERROR(%d)\r\n"), GetLastError()));
		}

	CloseHandle(hFile);


	
	
}
void ReadYuv2(PUCHAR sample)
{
	HANDLE hFile2;
	DWORD dwRet;
	//unsigned char lpBuffer[704*480*2];
	//unsigned char lpBuffer[2];
	unsigned int i ,j= 0;
	hFile2 = CreateFile(TEXT("\\yuv.yuv"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING  , 0, NULL);
	
	if(hFile2 == INVALID_HANDLE_VALUE){
			RETAILMSG(1, (TEXT("CreateFile yuv ERROR(%d)\r\n"), GetLastError()));
		}
	if(!ReadFile(hFile2, sample, 704*300*2, &dwRet, NULL)){	
			RETAILMSG(1, (TEXT("ReadFile yuv ERROR(%d)\r\n"), GetLastError()));
		}

	for(i=0; i<640*300*2; i++)	
		{
		j = i /640;				
		sample[i] = sample[i+j*64];
		
		}
	
	for(i=0; i<640*300*2; i+=2)	
		{
			sample[i]= sample[i+1]=0;//only display the Y color;
		}
		

	RETAILMSG(1,(TEXT("READ lpbuffer over!\r\n")));
	
	CloseHandle(hFile2);
}


