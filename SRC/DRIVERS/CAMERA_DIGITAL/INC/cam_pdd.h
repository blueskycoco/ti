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
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#ifndef _OMAPCAMPDD_H
#define _OMAPCAMPDD_H

#ifdef __cplusplus
extern "C" {
#endif

typedef MMRESULT (WINAPI *FNTIMEKILLEVENT)(UINT);
typedef MMRESULT (WINAPI *FNTIMESETEVENT)(UINT, UINT, LPTIMECALLBACK, DWORD, UINT );

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// YUV specific buffer filler 

UINT YUVBufferFill( PUCHAR pImage, PCS_VIDEOINFOHEADER pCsVideoInfoHdr, bool FlipHorizontal, LPVOID lpParam );

// YUY2 specific defines
#define MACROPIXEL_RED 0xf0525a52
#define MACROPIXEL_GRN 0x22913691
#define MACROPIXEL_WHITE 0x80EB80EB
#define MACROPIXEL_BLACK 0x80108010

// YV12 specific defines
#define Y_WHITE 235
#define Y_BLACK 16
#define VU_BOTH 128

// Misc YUV filler flags
#define BOXWIDTHDIVIDER 8
#define BOXHEIGHTDIVIDER 8
#define LOCATIONWIDTHMASK 0xFF
#define LOCATIONHEIGHTMASK 0xFF
#define LOCATIONSHIFT 8
// divide the initial tick count by four to make the box move 2x faster.
#define SPEEDSHIFT 4

//  Clock port interface 
#define CPI_POLCLK_POSITIVE 1
#define CPI_POLCLK_NEGATIVE 2
#define CPI_DATA_SWAPBYTES  4
#define CPI_POLRESET_POSITIVE 8
#define CPI_POLRESET_NEGATIVE 0x10
#define CPI_SAMPLE_MASK             0x00000001
#define CPI_SAMPLE_RISINGEDGE       0x00000000
#define CPI_SAMPLE_FALLINGEDGE      0x00000001
#define CPI_HS_POL_MASK             0x00000004
#define CPI_HS_POL_ACTIVEHIGH       0x00000000
#define CPI_HS_POL_ACTIVELOW        0x00000004
#define CPI_VS_POL_MASK             0x00000008
#define CPI_VS_POL_ACTIVEHIGH       0x00000000
#define CPI_VS_POL_ACTIVELOW        0x00000008
#define CPI_DATA_BIT_MASK           0x00000010
#define CPI_DATA_BIT_8              0x00000000
#define CPI_DATA_BIT_10             0x00000010

typedef enum {

  CSI2 = 0,
  PARALLEL,

} SENSORTYPE;

typedef enum {
  RGB = 0,
  RAW,
  YUV,
  JPEG
} SENSOR_DATATYPE;

typedef enum {
  DIRECT2MEMORY = 0,
  DATA2ISP,
} DATA_PATH;


typedef enum
{
    QQVGA=0,
    QCIF,
    QVGA,
    CIF,
    VGA,
    DNTSC,
    DPAL,
    SVGA,
    XGA,
    SXGA,
    UXGA,
    QXGA,
    MEGA5,
    MEGA6,
    MEGA8,
    N_MAX_RES,
    CAMRES_INVALID=0xffffffff
} CAMRES;


typedef enum
{
    SEPIA_SPEFFECT=0,
    BLUISH_SPEFFECT,
    GREENISH_SPEFFECT,
    REDDISH_SPEFFECT,
    YELLOWISH_SPEFFECT,
    BANDW_SPEFFECT,
    NEGATIVE_SPEFFECT,
    NORMAL_SPEFFECT
} SPEFFECT;



//
// Global structures and variable definitions
//


struct FormatTable_t
{
    LONG    width;
    LONG    height;
    DWORD   bitcount;
    DWORD   format;
    DWORD   postprocinfo;
    DWORD   skipframes;
    DWORD   fps;
    DWORD   max_exposure;
    DWORD   avgtimeperframe; // 100ns units
};

static struct FormatTable_t
s_tblVideoFormat[N_MAX_RES] =
{
// {width, height, bitcount, format, postproinfo, skipframes, fps, max_exposure, avgtimeperframe}
    {160, 120, 16,   QQVGA, 0, 0, 30, 180, 0x51615},
    {176, 144, 16,   QCIF,  0, 0, 30, 190, 0x51615},
    {320, 240, 16,   QVGA,  0, 0, 30, 248, 0x51615},
    {352, 288, 16,   CIF,   0, 0, 30, 382, 0x51615},
    {640, 480, 16,   VGA,   0, 0, 30, 498, 0x51615},
    {720, 480, 16,   DNTSC, 0, 0, 30, 498, 0x51615},
    {720, 576, 16,   DPAL,  0, 0, 30, 498, 0x51615},    
    {800, 600, 16,   SVGA,  0, 0, 15, 498, 0xa2c2a},
    {1024,768, 16,   XGA,   0, 0, 30, 498, 0x51615},
    {1280,960, 16,   SXGA,  0, 0, 15, 998, 0xa2c2a},
    {1600,1200,16,   UXGA,  0, 0, 15, 1610,0xa2c2a},
	{2592,1944,16,   QXGA,  0, 0, 15, 1800,0xa2c2a},
    {2560,1920,16,   MEGA5, 0, 0, 30, 2000,0xa2c2a},
    {3264,2448,16,   MEGA8, 0, 0, 30, 2500,0xa2c2a}
};

/* Common structure for populating sensor clock info */
typedef struct 
{
    // list of clock speeds available by 
    // camera port interface for ext. camera 
    DWORD *pExClockList;
    // caps flags set by camera port controller
    DWORD  Caps;
    
    // QueryCameraPortInfo asks camera control 
    DWORD DesiredExClock;
    DWORD ActualExClock;
    
    DWORD DesiredCaps;
    DWORD ActualCaps;

}CameraPortInfo_t;

typedef struct 
{
    BOOL bJpegEncEnabled;
    BOOL bH3AEnabled;
    BOOL bHWJpegEnc;
    BOOL bSensorStateNotification;
}CameraConfig_t;

typedef struct 
{
    ULONG ulMipiClock;
    ULONG ulUBoundHsSettle;
    ULONG ulLBoundHsSettle;
}CSI2PhysCfg_t;

typedef class CameraCtrl CameraCtrl_t;

typedef class CameraSensor
{
public:
   CameraSensor(){}

   virtual ~CameraSensor(){}

   virtual DWORD Init(PVOID MDDContext, CameraCtrl_t *pCameraCtrlContext) = 0;

   virtual DWORD Deinit() = 0;
   
   virtual BOOL GetCSI2PHYCFGValues(CSI2PhysCfg_t *pConfig) = 0;
   
   virtual DWORD SetCameraConfig(CameraConfig_t *pCameraConfig) = 0;

#ifdef BSP_CAMERA_H3A   
   virtual DWORD SetAnalogGain(UINT16 iGain) = 0;
   
   virtual DWORD SetIntegrationTime(UINT16 iExpTimeH) = 0;

   virtual DWORD SetFocus(UINT16 iCapFocus) = 0;
#endif /* end of BSP_CAMERA_H3A */   
    
   virtual DWORD GetAdapterInfo( PADAPTERINFO pAdapterInfo ) = 0;
   
   virtual DWORD HandleVidProcAmpChanges( DWORD dwPropId, LONG lFlags, LONG lValue ) = 0;
   
   virtual DWORD HandleCamControlChanges( DWORD dwPropId, LONG lFlags, LONG lValue ) = 0;
   
   virtual DWORD HandleVideoControlCapsChanges( LONG lModeType ,ULONG ulCaps ) = 0;
   
   virtual DWORD SetPowerState( CEDEVICE_POWER_STATE PowerState ) = 0;

   virtual DWORD GetPinTypes() = 0;

   virtual DWORD InitSensorMode( ULONG ulModeType, LPVOID ModeContext ) = 0;

   virtual DWORD DeInitSensorMode( ULONG ulModeType ) = 0;

   virtual DWORD SetSensorState( ULONG lModeType, CSSTATE csState ) = 0;

   virtual DWORD TakeStillPicture( LPVOID pBurstModeInfo ) = 0;

   virtual DWORD GetSensorModeInfo( ULONG ulModeType, PSENSORMODEINFO pSensorModeInfo ) = 0;

   virtual DWORD SetSensorModeFormat( ULONG ulModeType, PCS_DATARANGE_VIDEO pCsDataRangeVideo ) = 0;

   virtual BOOL QueryCameraPortInfo( CameraPortInfo_t *pinfo ) = 0;

   virtual BOOL SetCameraPortInfo( CameraPortInfo_t *pinfo ) = 0;

   virtual DWORD IsCaptureMode() = 0;

   virtual BOOL IsPreviewMode() = 0;

   virtual BOOL SetCaptureCmd() = 0;

   virtual BOOL GetJpegSize(ULONG *pJpegSize) = 0;

   static CameraSensor *CameraPDD_New();

}CameraSensor_t;


typedef class CameraCtrl
{
public:
    
    friend class CCameraDevice;

    CameraCtrl(){}

    virtual ~CameraCtrl(){}

    virtual DWORD Init(
        PVOID MDDContext, 
        CameraSensor *pSensorContext
        ) = 0;

    virtual DWORD DeInit(
        PVOID MDDContext
        ) = 0;
    
    virtual DWORD Open( 
        PVOID MDDOpenContext
        ) = 0;

    virtual DWORD Close( 
        PVOID MDDOpenContext
        ) = 0;

    virtual BOOL SetCameraSensorClock(
        BOOL bEnable
        ) = 0;

    virtual DWORD GetAdapterInfo( 
        PADAPTERINFO pAdapterInfo 
        ) = 0;

    virtual DWORD HandleVidProcAmpChanges(
        DWORD dwPropId, 
        LONG lFlags, 
        LONG lValue
        ) = 0;
    
    virtual DWORD HandleCamControlChanges( 
        DWORD dwPropId, 
        LONG lFlags, 
        LONG lValue 
        ) = 0;

    virtual DWORD HandleVideoControlCapsChanges(
        LONG lModeType ,
        ULONG ulCaps 
        ) = 0;

    virtual DWORD SetPowerState(
        CEDEVICE_POWER_STATE PowerState 
        ) = 0;
    
    virtual DWORD HandleAdapterCustomProperties(
        PUCHAR pInBuf, 
        DWORD  InBufLen, 
        PUCHAR pOutBuf, 
        DWORD  OutBufLen, 
        PDWORD pdwBytesTransferred 
        ) = 0;

    virtual DWORD InitSensorMode(
        ULONG ulModeType, 
        LPVOID ModeContext,
        DWORD dwPinTypes
        ) = 0;
    
    virtual DWORD DeInitSensorMode( 
        ULONG ulModeType 
        ) = 0;

    virtual DWORD SetSensorState( 
        ULONG lPinId, 
        CSSTATE csState 
        ) = 0;

    virtual DWORD TakeStillPicture(
        LPVOID pBurstModeInfo ) = 0;

    virtual DWORD GetSensorModeInfo( 
        ULONG ulModeType, 
        PSENSORMODEINFO pSensorModeInfo 
        ) = 0;

    virtual DWORD SetSensorModeFormat( 
        ULONG ulModeType, 
        PCS_DATARANGE_VIDEO pCsDataRangeVideo 
        ) = 0;

    virtual PVOID AllocateBuffer(
        ULONG ulModeType 
        ) = 0;

    virtual DWORD DeAllocateBuffer( 
        ULONG ulModeType, 
        PVOID pBuffer
        ) = 0;

    virtual DWORD RegisterClientBuffer(
        ULONG ulModeType, 
        PVOID pBuffer 
        ) = 0;

    virtual DWORD UnRegisterClientBuffer( 
        ULONG ulModeType, 
        PVOID pBuffer 
        ) = 0;

    virtual DWORD FillBuffer( 
        ULONG ulModeType, 
        PUCHAR pImage ) = 0;

    virtual DWORD HandleSensorModeCustomProperties( 
        ULONG ulModeType, 
        PUCHAR pInBuf, 
        DWORD  InBufLen, 
        PUCHAR pOutBuf, 
        DWORD  OutBufLen, 
        PDWORD pdwBytesTransferred 
        ) = 0;

    virtual DWORD GetMetadata(
        DWORD dwPropId, 
        PUCHAR pOutBuf, 
        DWORD OutBufLen, 
        PDWORD pdwBytesTransferred
        ) = 0;

    virtual VOID EnqueueBuffer(
        PVOID pBuffer,
        ULONG ulPinId
        ) = 0;

    virtual VOID SetUsedIndex(
        DWORD dwIndex
        ) = 0;
    virtual DWORD SetJpegQuality(
        LONG lQuality
        ) = 0;
    
    virtual VOID AllowAutoIdle(
        BOOL bEnable 
        ) = 0;
    /* Function for allocating derived class */
    static CameraCtrl *CameraPDD_New();



private:

    void 
    HandleCaptureInterrupt( UINT uTimerID );

    void 
    HandleStillInterrupt( UINT uTimerID );

    DWORD 
    YUVBufferFill( ULONG ulModeType, PUCHAR pImage );

    bool 
    ReadGlobalsFromRegistry();



}CameraCtrl_t;




class CCamPdd
{
public:
  
    CCamPdd();

    ~CCamPdd();

   CameraCtrl_t      *pCamCtrl;
   CameraSensor_t    *pCamSensor;
   
};



#ifdef __cplusplus
}
#endif

#endif
