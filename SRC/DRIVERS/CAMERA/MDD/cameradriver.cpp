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
DWORD
CAM_Init(
    VOID * pContext
    )
{    
    CAMERADEVICE * pCamDev = NULL;
    DEBUGMSG(ZONE_INIT, (_T("CAM_Init: context %s\n"), pContext));
    
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
    DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_IOControl(%08x): IOCTL:0x%x, InBuf:0x%x, InBufLen:%d, OutBuf:0x%x, OutBufLen:0x%x)\r\n"), dwContext, Ioctl, pInBufUnmapped, InBufLen, pOutBufUnmapped, OutBufLen ) );

    UCHAR * pInBuf = NULL;
    UCHAR * pOutBuf = NULL;
    DWORD dwErr = ERROR_INVALID_PARAMETER;
    BOOL  bRc   = FALSE;
    
    //All buffer accesses need to be protected by try/except
    pInBuf = pInBufUnmapped;
    pOutBuf = pOutBufUnmapped;

    CAMERAOPENHANDLE * pCamOpenHandle = reinterpret_cast<CAMERAOPENHANDLE *>( dwContext );
    CAMERADEVICE     * pCamDevice     = pCamOpenHandle->pCamDevice;
    
    switch ( Ioctl )
    {
        // Power Management Support.
        case IOCTL_POWER_CAPABILITIES:
        case IOCTL_POWER_QUERY:
        case IOCTL_POWER_SET:
        case IOCTL_POWER_GET:
        {
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
            CSPROPERTY       * pCsProp        = reinterpret_cast<CSPROPERTY *>(pInBuf);

            if ( InBufLen < sizeof(CSPROPERTY) || NULL == pdwBytesTransferred )
            {
                SetLastError( dwErr );
                return bRc;
            }

            if ( NULL == pCsProp )
            {
                DEBUGMSG( ZONE_IOCTL|ZONE_ERROR, (_T("CAM_IOControl(%08x): Invalid Parameter.\r\n"), dwContext ) );
                return dwErr;
            }
    
            DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x): IOCTL_CS_PROPERTY\r\n"), dwContext ) );

            __try 
            {
                dwErr = pCamDevice->AdapterHandleCustomRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );

                if ( ERROR_NOT_SUPPORTED == dwErr )
                {
                    if ( TRUE == IsEqualGUID( pCsProp->Set, CSPROPSETID_Pin ) )
                    {   
                        dwErr = pCamDevice->AdapterHandlePinRequests( pInBuf, InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, CSPROPSETID_VERSION ) )
                    {
                        dwErr = pCamDevice->AdapterHandleVersion( pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_VIDEOPROCAMP ) )
                    {   
                        dwErr = pCamDevice->AdapterHandleVidProcAmpRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_CAMERACONTROL ) )
                    {   
                        dwErr = pCamDevice->AdapterHandleCamControlRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_VIDEOCONTROL ) )
                    {   
                        dwErr = pCamDevice->AdapterHandleVideoControlRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                    else if ( TRUE == IsEqualGUID( pCsProp->Set, PROPSETID_VIDCAP_DROPPEDFRAMES) )
                    {   
                        dwErr = pCamDevice->AdapterHandleDroppedFramesRequests( pInBuf,InBufLen, pOutBuf, OutBufLen, pdwBytesTransferred );
                    }
                }
            }
            __except ( EXCEPTION_EXECUTE_HANDLER )
            {
                DEBUGMSG( ZONE_IOCTL, ( _T("CAM_IOControl(%08x):Exception in IOCTL_CS_PROPERTY"), dwContext ) );
            }

            break;
        }

        default:
        {
            DEBUGMSG( ZONE_IOCTL, (_T("CAM_IOControl(%08x): Unsupported IOCTL code %u\r\n"), dwContext, Ioctl ) );
            dwErr = ERROR_NOT_SUPPORTED;

            break;
        }
    }
    
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

    UNREFERENCED_PARAMETER( ShareMode );
    UNREFERENCED_PARAMETER( Access );

    
    CAMERADEVICE     * pCamDevice     = reinterpret_cast<CAMERADEVICE *>( Context );
    CAMERAOPENHANDLE * pCamOpenHandle = NULL;
    HANDLE             hCurrentProc   = NULL;
    
    hCurrentProc = (HANDLE)GetCallerVMProcessId();

    ASSERT( pCamDevice != NULL );

    if ( pCamDevice->BindApplicationProc( hCurrentProc ) )
    {
        pCamOpenHandle = new CAMERAOPENHANDLE;

        if ( NULL == pCamOpenHandle )
        {
            DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_Open(%x): Not enought memory to create open handle\r\n"), Context ) );
        }
        else
        {
            pCamOpenHandle->pCamDevice = pCamDevice;
        }
    }
    else
    {
        SetLastError( ERROR_ALREADY_INITIALIZED );
    }

    return reinterpret_cast<DWORD>( pCamOpenHandle );
}


EXTERN_C
BOOL  
CAM_Close(
    DWORD Context
    ) 
{
    DEBUGMSG( ZONE_FUNCTION, ( _T("CAM_Close(%x)\r\n"), Context ) );
    
    PCAMERAOPENHANDLE pCamOpenHandle = reinterpret_cast<PCAMERAOPENHANDLE>( Context );

    pCamOpenHandle->pCamDevice->UnBindApplicationProc( );
    SAFEDELETE( pCamOpenHandle ) ;

    return TRUE;
}

