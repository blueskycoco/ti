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
/*++


Module Name:

    CameraDriver.cpp

Abstract:

    MDD Adapter implementation

Notes:
    

Revision History:

--*/

#include <windows.h>
#include <devload.h>
#include <nkintr.h>
#include <pm.h>

#include "Cs.h"
#include "Csmedia.h"

#include "CameraPDDProps.h"
#include "dstruct.h"
#define CAMINTERFACE
#include "dbgsettings.h"
#include "CameraDriver.h"

EXTERN_C
BOOL  
CAM_Close(
    DWORD Context
    ); 



EXTERN_C
DWORD
CAM_Init(
    VOID * pContext
    )
{    
    CAMERADEVICE * pCamDev = NULL;
    DEBUGMSG(ZONE_INIT, (_T("CAM_Init: context %s\n"), pContext));
	RETAILMSG(1, (_T("wangchong:CAM_Init---------------------------\r\n")));

    pCamDev = new CAMERADEVICE;
    
    if ( NULL != pCamDev )
    {
        // NOTE: real drivers would need to validate pContext before dereferencing the pointer
        if ( false == pCamDev->Initialize( pContext ) )
        {
            SetLastError( ERROR_INVALID_HANDLE );
            SAFEDELETE( pCamDev );
            DEBUGMSG( ZONE_INIT|ZONE_ERROR, ( _T("CAM_Init: Initialization Failed") ) );
        }
    }
    else
    {
        SetLastError( ERROR_OUTOFMEMORY );
    }

    DEBUGMSG( ZONE_INIT, ( _T("CAM_Init: returning 0x%08x\r\n"), reinterpret_cast<DWORD>( pCamDev ) ) );

    return reinterpret_cast<DWORD>( pCamDev );
}


EXTERN_C
BOOL
CAM_Deinit(
    DWORD dwContext
    )
{
    DEBUGMSG( ZONE_INIT, ( _T("CAM_Deinit\r\n") ) );
	RETAILMSG(1, (_T("wangchong:CAM_Deinit---------------------------\r\n")));

    CAMERADEVICE * pCamDevice = reinterpret_cast<CAMERADEVICE *>( dwContext );
    SAFEDELETE( pCamDevice );

    return TRUE;
}


EXTERN_C
BOOL
CAM_IOControl(
    DWORD   dwContext,
    DWORD   Ioctl,
    UCHAR * pInBufUnmapped,
    DWORD   InBufLen, 
    UCHAR * pOutBufUnmapped,
    DWORD   OutBufLen,
    DWORD * pdwBytesTransferred
   )
{
	int i;
  //  DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_IOControl(%08x): IOCTL:0x%x, InBuf:0x%x, InBufLen:%d, OutBuf:0x%x, OutBufLen:0x%x)\r\n"), dwContext, Ioctl, pInBufUnmapped, InBufLen, pOutBufUnmapped, OutBufLen ) );
//	RETAILMSG(1, ( _T("CAM_IOControl(%08x): IOCTL:0x%x, InBuf:0x%x, InBufLen:%d, OutBuf:0x%x, OutBufLen:0x%x)\r\n"), dwContext, Ioctl, pInBufUnmapped, InBufLen, pOutBufUnmapped, OutBufLen ) );
//	RETAILMSG(1, (_T("wangchong:CAM_IOControl---------------------------\r\n")));

    UCHAR * pInBuf = NULL;
    UCHAR * pOutBuf = NULL;
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    BOOL  bRc   = FALSE;
 //   RETAILMSG(1, (_T("wangchong:0------------000---------------\r\n")));
    if ( ( NULL == pInBufUnmapped )
         || ( InBufLen < sizeof ( CSPROPERTY ) )
         || ( NULL == pdwBytesTransferred ) )
    {
  //  	RETAILMSG(1,(TEXT("#wangwj#---pInBufUnmapped=%x,  INbufLen=%x, sizeof=%x,  pdwBytesTra=%x --------------------\r\n"),pInBufUnmapped,InBufLen ,sizeof ( CSPROPERTY ),pdwBytesTransferred));
        SetLastError( dwErr );

        return bRc;
    }
//	RETAILMSG(1, (_T("wangchong:0------------111---------------\r\n")));
    //All buffer accesses need to be protected by try/except
    pInBuf = pInBufUnmapped;

    pOutBuf = pOutBufUnmapped;

    CAMERAOPENHANDLE * pCamOpenHandle = reinterpret_cast<CAMERAOPENHANDLE *>( dwContext );
    CAMERADEVICE     * pCamDevice     = pCamOpenHandle->pCamDevice;
    CSPROPERTY       * pCsProp        = reinterpret_cast<CSPROPERTY *>(pInBuf);
  //  RETAILMSG(1, (_T("wangchong:0------------222---------------\r\n")));
    if ( NULL == pCsProp )
    {
  //     RETAILMSG(1, (_T("wangchong:0------------333---------------\r\n")));
        DEBUGMSG( ZONE_IOCTL|ZONE_ERROR, (_T("CAM_IOControl(%08x): Invalid Parameter.\r\n"), dwContext ) );
        return dwErr;
    }
  //  RETAILMSG(1, (_T("wangchong:0---------------------------\r\n")));
    switch ( Ioctl )
    {
        // Power Management Support.
        case IOCTL_POWER_CAPABILITIES:
        case IOCTL_POWER_QUERY:
        case IOCTL_POWER_SET:
        case IOCTL_POWER_GET:
        {
			//RETAILMSG(1, (_T("wangchong:1---------------------------\r\n")));
            DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x): Power Management IOCTL\r\n"), dwContext ) );
            __try 
            {
                dwErr = pCamDevice->AdapterHandlePowerRequests(Ioctl, pInBuf, InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
            }
            __except ( EXCEPTION_EXECUTE_HANDLER )
            {
                DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x):Exception in Power Management IOCTL"), dwContext ) );
            }
            break;
        }

        case IOCTL_CS_PROPERTY:
        {
		//	RETAILMSG(1, (_T("wangchong:2---------------------------\r\n")));
            DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x): IOCTL_CS_PROPERTY\r\n"), dwContext ) );

            __try 
            {
			//	RETAILMSG(1, (_T("wangchong:3---------------------------\r\n")));
                dwErr = pCamDevice->AdapterHandleCustomRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );

                if ( ERROR_NOT_SUPPORTED == dwErr )
                {
                    if ( TRUE == IsEqualGUID( pCsProp->Set, CSPROPSETID_Pin ) )
                    {   
			//			RETAILMSG(1, (_T("wangchong:4---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandlePinRequests( pInBuf, InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, CSPROPSETID_VERSION ) )
                    {
				//		RETAILMSG(1, (_T("wangchong:5---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandleVersion( pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_VIDEOPROCAMP ) )
                    {   
				//		RETAILMSG(1, (_T("wangchong:6---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandleVidProcAmpRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_CAMERACONTROL ) )
                    {   
				//		RETAILMSG(1, (_T("wangchong:7---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandleCamControlRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_VIDEOCONTROL ) )
                    {   
				//		RETAILMSG(1, (_T("wangchong:8---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandleVideoControlRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_DROPPEDFRAMES) )
                    {   
				//		RETAILMSG(1, (_T("wangchong:9---------------------------\r\n")));
                        dwErr = pCamDevice->AdapterHandleDroppedFramesRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                }
            }
            __except ( EXCEPTION_EXECUTE_HANDLER )
            {
          //  RETAILMSG(1, (_T("wangchong:10---------------------------\r\n")));
                DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x):Exception in IOCTL_CS_PROPERTY"), dwContext ) );
            }

            break;
        }

        default:
        {
		//	RETAILMSG(1, (_T("wangchong:11---------------------------\r\n")));
            DEBUGMSG( ZONE_IOCTL, (_T("CAM_IOControl(%08x): Unsupported IOCTL code %u\r\n"), dwContext, Ioctl ) );
            dwErr = ERROR_NOT_SUPPORTED;

            break;
        }
		//RETAILMSG(1, (_T("wangchong:12---------------------------\r\n")));
    }
    //RETAILMSG(1, (_T("wangchong:13---------------------------\r\n")));
    // pass back appropriate response codes
    SetLastError( dwErr );

    return ( ( dwErr == ERROR_SUCCESS ) ? TRUE : FALSE );
}


EXTERN_C
DWORD
CAM_Open(
    DWORD Context, 
    DWORD Access,
    DWORD ShareMode
    )
{
    DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_Open(%x, 0x%x, 0x%x)\r\n"), Context, Access, ShareMode ) );
	RETAILMSG(1, (_T("wangchong:CAM_Open---------------------------\r\n")));

    UNREFERENCED_PARAMETER( ShareMode );
    UNREFERENCED_PARAMETER( Access );

    
    CAMERADEVICE     * pCamDevice     = reinterpret_cast<CAMERADEVICE *>( Context );
    CAMERAOPENHANDLE * pCamOpenHandle = NULL;
    HANDLE             hCurrentProc   = NULL;
    
    hCurrentProc = (HANDLE)GetCallerVMProcessId();

    ASSERT( pCamDevice != NULL );
RETAILMSG(1, (_T("wangchong:CAM_Open----------------------111-----\r\n")));
    if ( pCamDevice->BindApplicationProc( hCurrentProc ) )
    {
        pCamOpenHandle = new CAMERAOPENHANDLE;
	RETAILMSG(1, (_T("wangchong:CAM_Open----------------------222-----\r\n")));
        if ( NULL == pCamOpenHandle )
        {
            DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_Open(%x): Not enought memory to create open handle\r\n"), Context ) );
        }
        else
        {
        	RETAILMSG(1, (_T("wangchong:CAM_Open----------------------333-----\r\n")));
            pCamOpenHandle->pCamDevice = pCamDevice;
        }
    }
    else
    {
    	RETAILMSG(1, (_T("wangchong:CAM_Open----------------------444-----\r\n")));
	//CAM_Close(Context);//wangwj added to test
        SetLastError( ERROR_ALREADY_INITIALIZED );
    }
	RETAILMSG(1, (_T("wangchong:CAM_Open-------------------over--------\r\n")));







	
    return reinterpret_cast<DWORD>( pCamOpenHandle );
}


EXTERN_C
BOOL  
CAM_Close(
    DWORD Context
    ) 
{
    DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_Close(%x)\r\n"), Context ) );
	RETAILMSG(1, (_T("wangchong:CAM_Close---------------------------\r\n")));
    
    PCAMERAOPENHANDLE pCamOpenHandle = reinterpret_cast<PCAMERAOPENHANDLE>( Context );

    pCamOpenHandle->pCamDevice->UnBindApplicationProc( );
    SAFEDELETE( pCamOpenHandle ) ;

    return TRUE;
}

