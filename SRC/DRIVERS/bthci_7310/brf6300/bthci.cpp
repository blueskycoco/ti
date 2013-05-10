//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// Portions Copyright (c) Texas Instruments.  All rights reserved.
//
//------------------------------------------------------------------------------
//
#include <windows.h>
#include <pm.h>
#include <oal.h>
#include <ceddkex.h>
#include <bt_buffer.h>
#include <bt_hcip.h>
#include <bt_os.h>
#include <bt_debug.h>
#include <bt_tdbg.h>

#include "bthci.h"

#include <ceddk.h>
#include <nkintr.h>
//#include <ti_constants.h>

#include <initguid.h>
#include <sdk_gpio.h>
#include <omap3530.h>
#include <omap3530_irq.h>

#undef IFDBG
#define IFDBG(x) x
#undef DebugOut
#define DebugOut(x,y) RETAILMSG(x,y)

#define DbgPacket 0
#define HW_GPIO 0
#define _FM_   1
//------------------------------------------------------------------------------

#define PACKET_SIZE_R                   (64 * 1024 + 128)
#define PACKET_SIZE_W                   (64 * 1024) 

// Read/WritePacket should never fail, but just in case there is a hardware
// problem we will timeout after 1 second.
#define DEFAULT_PACKET_TIMEOUT            1000

//------------------------------------------------------------------------------

#define DEBUG_READ_BUFFER_HEADER        4
#define DEBUG_WRITE_BUFFER_HEADER       8
#define DEBUG_READ_BUFFER_TRAILER       1
#define DEBUG_WRITE_BUFFER_TRAILER      3

#define _FM_


#define UART2_CTS		67
#define UART2_RTS		68

#ifdef _FM_
#define COMMAND_COMPLETE_PARTIAL_HEADER_LEN 3
#endif
//------------------------------------------------------------------------------

DECLARE_DEBUG_VARS();

//------------------------------------------------------------------------------

typedef enum {
    BT_SLEEP = 0,
    BT_SLEEP_TO_AWAKE,
    BT_AWAKE_TO_SLEEP,
    BT_AWAKE
} BT_POWER_MODE;

typedef enum {
    REQUEST_NONE = 0,
    REQUEST_SEND,
    REQUEST_AWAKE,
    REQUEST_SLEEP,
    REQUEST_RELEASE_BUFFER
} REQUEST;

typedef struct {

    LPWSTR  comPortName;
    LPWSTR  startScript;
    DWORD   baud;
    DWORD   priority256;
    DWORD   wakeUpTimer;
    DWORD   cmdTimeout;
    DWORD   resetDelay;
    DWORD   specV10a;
    DWORD   specV11;
    DWORD   writeTimeout;
    DWORD   flags;
    DWORD   drift;
    DWORD   resetGpio;
    DWORD   driverTimeout;

    HANDLE  hCom;

    HANDLE  hGpio;
//    OMAP2430_SYSC_PADCONFS1_REGS *pPadConfig;
    BOOL    opened;
#ifdef _FM_
	//---------------------------------------
	//FM-specific
	DWORD   OpenedRefCount;  //Count the current driver's clients (FM,BT)
	BOOL    WaitForInitScript; //The BT init script should be send to the chip 
	CRITICAL_SECTION OpenCS; 
	CRITICAL_SECTION FakeCommandCS;
	BOOL    FMOpened; //The FM stack called to HCI_FMOpenConnection
	UCHAR*  pFMReadBuffer;//The FM stack read buffer
	DWORD	FMreadBufferSize;
	HANDLE	hFMReadBufferEvent;
	HANDLE	hFMReadPacketEvent;
	DWORD	FMreadPacketSize;
	BOOL    FakeResetComp;
	UINT16	lastFmCommandOpcode;

	//----------------------------------------
#endif

    BOOL    exitDispatchThread;
    HANDLE  hDispatchThread;

    BT_POWER_MODE btPowerMode;

    BOOL    startRequest;
    REQUEST request;
    HANDLE  hRequestDoneEvent;

    UCHAR*  pWritePacket;
    DWORD   writePacketSize;

    HANDLE  hReadBufferEvent;
    HANDLE  hReadPacketEvent;
    UCHAR*  pReadBuffer;
    DWORD   readBufferSize;
    DWORD   readPacketSize;

    CRITICAL_SECTION writeCS;
    CRITICAL_SECTION readCS;

    HCI_TransportCallback pfnCallback;
} HCI_CONTEXT;
#ifdef _FM_
//-------------------------------------------------------------------------------
//FM Specific functions
static VOID ProcessFMReceiveEventPacket( HCI_CONTEXT *pHCI);

static VOID ProcessReceiveEventPacket(HCI_CONTEXT *pHCI);

static DWORD FakeResetComplete();

//------------------------------------------------------------------------------
#endif
static HCI_CONTEXT g_hci;

//------------------------------------------------------------------------------
//  Device registry parameters
static const DEVICE_REGISTRY_PARAM g_deviceRegParams[] = {
    {
        L"Name", PARAM_STRING, FALSE, offset(HCI_CONTEXT, comPortName),
        0, L"COM1:"
    }, {
        L"Script", PARAM_STRING, FALSE, offset(HCI_CONTEXT, startScript),
        0, L"brf61_%d.%d.%d.bts"
    }, {
        L"Baud", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, baud),
        fieldsize(HCI_CONTEXT, baud), (VOID*)115200
    }, {
        L"Priority256", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, priority256),
        fieldsize(HCI_CONTEXT, priority256), (VOID*)120
    }, {
        L"WakeUpTimer", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, wakeUpTimer),
        fieldsize(HCI_CONTEXT, wakeUpTimer), (VOID*)2000
    }, {
        L"CmdTimeout", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, cmdTimeout),
        fieldsize(HCI_CONTEXT, cmdTimeout), (VOID*)8000
    }, {
        L"ResetDelay", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, resetDelay),
        fieldsize(HCI_CONTEXT, resetDelay), (VOID*)300
    }, {
        L"SpecV10a", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, specV10a),
        fieldsize(HCI_CONTEXT, specV10a), (VOID*)0
    }, {
        L"SpecV11", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, specV11),
        fieldsize(HCI_CONTEXT, specV11), (VOID*)0
    }, {
        // This is the WriteTimeout used by HCI. It is different from the one used by the driver.
        // A better name for this parameter might have been CommandTimeout.    
        L"WriteTimeout", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, writeTimeout),
        fieldsize(HCI_CONTEXT, writeTimeout), (VOID*)HCI_DEFAULT_WRITE_TIMEOUT
    }, {
        L"Flags", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, flags),
        fieldsize(HCI_CONTEXT, flags), (VOID*)0
    }, {
        L"Drift", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, drift),
        fieldsize(HCI_CONTEXT, drift), (VOID*)HCI_DEFAULT_DRIFT
    }, {
        L"ResetGpio", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, resetGpio),
        fieldsize(HCI_CONTEXT, resetGpio), (VOID*)-1
    }, {
        L"DriverTimeout", PARAM_DWORD, FALSE, offset(HCI_CONTEXT, driverTimeout),
        fieldsize(HCI_CONTEXT, driverTimeout), (VOID*)DEFAULT_PACKET_TIMEOUT
    }
};

//------------------------------------------------------------------------------
//  Function Definitions

//------------------------------------------------------------------------------
#if 0
void DumpBuffer(UCHAR *pBuffer, DWORD size)
{
    WCHAR szBuffer[64], *pszPos = szBuffer;
    DWORD count;

    count = 0;
    while (count < size) {
        if ((count & 0x0F) == 0) {
            wsprintf(szBuffer, L"%04x:", count);
            pszPos = &szBuffer[5];
        }            
        wsprintf(pszPos, L" %02x", pBuffer[count]);
        pszPos += 3;
        if ((count & 0x0F) == 0x0F) {
            wsprintf(pszPos, L"\r\n");
            OutputDebugString(szBuffer);
        }
        count++;
    }
    if ((count & 0x0F) != 0) {
        wsprintf(pszPos, L"\r\n");
        OutputDebugString(szBuffer);
    }
}
#endif


//------------------------------------------------------------------------------
static BOOL SetComPowerState(HANDLE hCom, CEDEVICE_POWER_STATE ps)
{
    BOOL rc = TRUE;
    
    RETAILMSG(0, (L"HCI SetComPowerState: COM Power D%d\r\n", ps));
	rc = DeviceIoControl(hCom, IOCTL_POWER_SET, NULL, 0, &ps, sizeof(ps), NULL, NULL);
	return rc;
}

//------------------------------------------------------------------------------
static BOOL SetComGPIOMode(HANDLE hCom, BOOL Mode)
{
    BOOL rc = TRUE;
    
#if HW_GPIO   
	if(Mode)
	{     		
 		//Set CTS to GPIO67 with Mode 3, Pulldown, enabled
 		MASKREG32(&(g_hci.pPadConfig->ulCONTROL_MMC2_DAT0), BIT28|BIT27|BIT26|BIT25|BIT24, BIT27|BIT25|BIT24); 
		RETAILMSG(0, (TEXT("SetComGPIOMode: Change UART2 CTS to GPIO\r\n")));
	}
	else
	{
	    //Give GPIO back to CTS with Mode 0, pulldown, enabled
         MASKREG32(&(g_hci.pPadConfig->ulCONTROL_MMC2_DAT0), BIT28|BIT27|BIT26|BIT25|BIT24, BIT27); 
		 RETAILMSG(0, (TEXT("SetComGPIOMode: Change UART2 GPIO to CTS\r\n")));

   	}
#endif

	return rc;
}




static void Uart2GpioMode(HCI_CONTEXT *pHCI)
{
        
#if HW_GPIO  
		RETAILMSG(TRUE, (L"HCI: ProcessSleepInd(): SetComPowerState D3 \r\n"));
		RETAILMSG(TRUE, (L"HCI: Uart2GpioMode(): %d, \r\n",BT_CTS_WAKEUP_SysIntr));
		// Enable GPIO Interrupt
		InterruptMask(BT_CTS_WAKEUP_SysIntr, FALSE);

     		 	
       //Set RTS to GPIO68 with Mode 3, pulldown, enabled
       MASKREG32(&(g_hci.pPadConfig->ulCONTROL_UART2_RTS), BIT4|BIT3|BIT2|BIT1|BIT0, BIT3|BIT1|BIT0);
       if(g_hci.hGpio)
       { 
         GPIOSetMode(g_hci.hGpio, UART2_RTS, GPIO_DIR_OUTPUT );
         GPIOSetBit(g_hci.hGpio, UART2_RTS);
       }

	   	GPIOSetMode(g_hci.hGpio, UART2_CTS, GPIO_DIR_INPUT | GPIO_INT_HIGH | GPIO_INT_LOW_HIGH );	//GPIO_INT_LOW_HIGH
	
	  	// Mux Uart CTS-> GPIO 67
		SetComGPIOMode(pHCI->hCom, TRUE);
	 	RETAILMSG(FALSE, (L"HCI: ProcessSleepInd(): SetComGPIOMode GPIO \r\n")); 
 // Set serial port to D3...

	   SetComPowerState(pHCI->hCom, D3);  
#endif
}

static void Gpio2UartMode(HCI_CONTEXT *pHCI)
{

#if HW_GPIO	
		// Set UART to Power D0 - awake
		SetComPowerState(g_hci.hCom, D0);
RETAILMSG(TRUE, (L"HCI: Gpio2UartMode(): SetComPowerState, \r\n")); 


		// Disable Interrupt
		InterruptMask(BT_CTS_WAKEUP_SysIntr, TRUE);
		
		
	   if(g_hci.hGpio) GPIOClrBit(g_hci.hGpio, UART2_RTS);
       //Give GPIO back to RTS with Mode 0, pulldown, enabled     	
       MASKREG32(&(g_hci.pPadConfig->ulCONTROL_UART2_RTS), BIT4|BIT3|BIT2|BIT1|BIT0, BIT3);
   
	   	// Mux GPIO 67 -> UART CTS
		SetComGPIOMode(pHCI->hCom, FALSE);
#endif	
		
}


//------------------------------------------------------------------------------
static void BT_WakeupInterruptThread()
{
	DWORD TimeOut=INFINITE;
		
	while (!BT_ISRTerminating)
    {
    	DWORD dwWaitObject;
    
		dwWaitObject = WaitForSingleObject(BT_CTS_WAKEUP_EVENT, TimeOut);
		if(BT_WakeupEnabled)
		{
	        RETAILMSG(TRUE, (L"***Wakeup IRQ***\r\n")); 	
			Gpio2UartMode(&g_hci);
			BT_WakeupEnabled = FALSE;
		}
		else 
		{
			RETAILMSG(TRUE, (L"***UART Mode :Wakeup IRQ***\r\n")); 
			Sleep(10);
		}
		InterruptDone(BT_CTS_WAKEUP_SysIntr);
	}

}

//------------------------------------------------------------------------------
static BOOL BT_InitInterruptThread(void)
{
	BT_CTS_WAKEUP_EVENT = CreateEvent( NULL, FALSE, FALSE, NULL);
	BT_ISRTerminating = FALSE;

  
	RETAILMSG(0, (L"+HCI BT_InitInterruptThread\r\n"));
	DWORD i = BT_CTS_WAKEUP_IRQ;
	if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &i, sizeof(i), 
            				&BT_CTS_WAKEUP_SysIntr, sizeof(BT_CTS_WAKEUP_SysIntr), NULL)) 
    {
        RETAILMSG(TRUE, (L"BT_IntThread: IRQ -> SYSINTR translation failed\r\n"));
        return FALSE;
    }
	if (BT_CTS_WAKEUP_SysIntr == SYSINTR_UNDEFINED)
	{ 
		RETAILMSG(TRUE, (L"BT_IntThread: IRQ -> SYSINTR failed: get SYSINTR_UNDEFINED\r\n"));
		return FALSE;
	}
	//Enable wakeup from BRF
	if(KernelIoControl(IOCTL_HAL_ENABLE_WAKE, &BT_CTS_WAKEUP_SysIntr, sizeof( BT_CTS_WAKEUP_SysIntr ),
            		NULL, 0, NULL)) RETAILMSG(TRUE, (L"set BRF as Wakeup source\r\n"));
    
    InterruptInitialize(BT_CTS_WAKEUP_SysIntr, BT_CTS_WAKEUP_EVENT, NULL, 0);
    BT_CTS_WAKEUP_THREAD  = CreateThread((LPSECURITY_ATTRIBUTES)NULL,
                                            0,
                                            (LPTHREAD_START_ROUTINE)BT_WakeupInterruptThread,
                                            NULL,
                                            0,
                                            NULL);
   	if(BT_CTS_WAKEUP_THREAD == NULL)
   	{
   		RETAILMSG(TRUE, (L"BT_CTS_WAKEUP_THREAD failed\r\n")); 
   		return FALSE;
   	}
            
   	//RETAILMSG(TRUE, (L"-HCI BT_InitInterruptThread\r\n"));
   	return TRUE;
}

//------------------------------------------------------------------------------
static BOOL BT_ReleaseInterruptThread(void)
{
	BT_ISRTerminating = TRUE;
	if(BT_CTS_WAKEUP_THREAD != NULL)
	{ 
		CloseHandle(BT_CTS_WAKEUP_THREAD);
		BT_CTS_WAKEUP_THREAD = NULL;
	}
	if(BT_CTS_WAKEUP_EVENT != NULL)
	{ 
		CloseHandle(BT_CTS_WAKEUP_EVENT);
		BT_CTS_WAKEUP_EVENT = NULL;
	}
	if (BT_CTS_WAKEUP_SysIntr != 0) 
    {
        KernelIoControl(IOCTL_HAL_RELEASE_SYSINTR, &BT_CTS_WAKEUP_SysIntr, sizeof(BT_CTS_WAKEUP_SysIntr), 
            				NULL, 0, NULL);
        BT_CTS_WAKEUP_SysIntr = 0;
    }
    return TRUE;
}

//------------------------------------------------------------------------------
static BOOL
WriteData(
    HCI_CONTEXT *pHCI,
    UCHAR *pBuffer,
    DWORD size
    )
{
    DWORD count;

    while (size > 0)
        {
        if (!WriteFile(pHCI->hCom, pBuffer, size, &count, NULL)) break;
        pBuffer += count;
        size -= count;
        }

    // Did we write all data?
    return (size == 0);
}

//------------------------------------------------------------------------------
static BOOL 
ReadData(
    HCI_CONTEXT *pHCI,
    UCHAR *pBuffer,
    DWORD size
    )
{
    DWORD count, mask;

    while (size > 0)
        {
        if (!ReadFile(pHCI->hCom, pBuffer, size, &count, NULL)) break;
        // If there is no data to be read wait for read event...
        if (count == 0)
            {
            // We must get read event...
            do 
                {
                WaitCommEvent(pHCI->hCom, &mask, NULL);
                }
            while (mask == 0);
            } 
        else 
            {
            pBuffer += count;
            size -= count;
            }
        }
    
    // Did we read all data?
    return (size == 0);
}

//------------------------------------------------------------------------------
//
//  Function:  SendPacketRequest
//
//  This function process REQUEST_SEND request. First it make sure that BRF6300
//  is awake. Then it sends packet data to BRF6300.
//
static VOID
SendPacketRequest(
    HCI_CONTEXT *pHCI
    )
{
    DWORD size;
    UCHAR *pPacket, cmd;

    // Before we send packet BRF6300 must be awake
    if (pHCI->btPowerMode == BT_SLEEP)
        {
        BT_WakeupEnabled = FALSE;	//disable ISR
  		Gpio2UartMode(pHCI);
        // Set new mode...
        pHCI->btPowerMode = BT_SLEEP_TO_AWAKE;
        // Send packet...
        cmd = HCI_WAKE_UP_IND;
        if (!WriteData(pHCI, &cmd, sizeof(cmd)))
        {            
            pHCI->exitDispatchThread = FALSE;
	        RETAILMSG(1, (L"HCI: SendPacketRequest(): Send WakeUpInd failed\r\n"));
        }

        RETAILMSG(0, (L"HCI: SendPacketRequest(): HCILL is BT_SLEEP_TO_AWAKE, Send WakeUpInd finished\r\n"));

        }
    else if (pHCI->btPowerMode != BT_AWAKE)
        {
        // Sooner or later we should get ACK from BRF6300
        Sleep(1);
			RETAILMSG(0, (L"HCI: SendPacketRequest(): HCILL is Not BT_AWAKE, NOT BT_SLEEP, sleep(1)\r\n")); 		
        }
    else 
        {
			RETAILMSG(0, (L"HCI: SendPacketRequest(): HCILL is BT_AWAKE, send data\r\n")); 		
        
        // Send packet over serial port
        pPacket = pHCI->pWritePacket;
        size = pHCI->writePacketSize;
        if (!WriteData(pHCI, pPacket, size))
        {
            pHCI->exitDispatchThread = FALSE;
        }
        
			RETAILMSG(0, (L"HCI: SendPacketRequest(): Send %02x\r\n", *pPacket));
       
        // We can release request
        pHCI->startRequest = FALSE;
        pHCI->request = REQUEST_NONE;
        pHCI->pWritePacket = NULL;
        pHCI->writePacketSize = 0;
			RETAILMSG(0, (L"HCI: SendPacketRequest(): SetEvent(hRequestDoneEvent)\r\n"));
        SetEvent(pHCI->hRequestDoneEvent);
        }
}

//------------------------------------------------------------------------------
//
//  Function:  WakeUpRequest
//
//  This function starts BRF6300 wakeup sequence.
//
static VOID
WakeUpRequest(
    HCI_CONTEXT *pHCI
    )
{
    UCHAR cmd;
    DWORD size, count;

    switch (pHCI->btPowerMode)
        {
        case BT_AWAKE:
            // We are done...
            pHCI->startRequest = FALSE;
            pHCI->request = REQUEST_NONE;
            RETAILMSG(0, (L"HCI: WakeUpRequest(): SetEvent(hRequestDoneEvent)\r\n"));
            SetEvent(pHCI->hRequestDoneEvent);
            break;
        case BT_SLEEP:
            BT_WakeupEnabled = FALSE;	//disable ISR
          	Gpio2UartMode(pHCI);
            // Set new mode...
            pHCI->btPowerMode = BT_SLEEP_TO_AWAKE;
            // Send packet...
            cmd = HCI_WAKE_UP_IND;
            size = sizeof(cmd);
            while (size > 0)
                {
                if (!WriteFile(pHCI->hCom, &cmd, size, &count, NULL)) break;
                size -= count;
                }

            RETAILMSG(0, (L"HCI: WakeUpRequest(): Send WakeUpInd\r\n"));

            // We start request...
            pHCI->startRequest = FALSE;
            break;
        case BT_AWAKE_TO_SLEEP:
        case BT_SLEEP_TO_AWAKE:
            // Sooner or later we should get ACK from BRF6300
            Sleep(1);
            break;
        }
}

//------------------------------------------------------------------------------
//
//  Function:  SleepRequest
//
//  This function starts BRF6300 sleep sequence.
//
static VOID
SleepRequest(
    HCI_CONTEXT *pHCI
    )
{
    UCHAR cmd;
    DWORD size, count;

    switch (pHCI->btPowerMode)
        {
        case BT_SLEEP:
            // We are done...
            pHCI->startRequest = FALSE;
            pHCI->request = REQUEST_NONE;
            RETAILMSG(0, (L"HCI:SleepRequest(): SetEvent(hRequestDoneEvent)\r\n"));
            SetEvent(pHCI->hRequestDoneEvent);
            break;
        case BT_AWAKE:
            // Set new mode...
            pHCI->btPowerMode = BT_AWAKE_TO_SLEEP;
            // Send packet...
            cmd = HCI_GO_TO_SLEEP_IND;
            size = sizeof(cmd);
            while (size > 0)
                {
                if (!WriteFile(pHCI->hCom, &cmd, size, &count, NULL)) break;
                size -= count;
                }
            
            RETAILMSG(0, (L"HCI:SleepRequest(): Send GoToSleepInd\r\n"));

            // We start request...
            pHCI->startRequest = FALSE;
            break;
        case BT_AWAKE_TO_SLEEP:
        case BT_SLEEP_TO_AWAKE:
            // Sooner or later we should get ACK from BRF6300
            Sleep(1);
            break;
        }
}

//------------------------------------------------------------------------------
//
//  Function:  ReleaseBufferRequest
//
//  This function return zero packet if there is pending read request. It
//  is used from HCI_CloseConnection to release pending HCI_ReadPacket.
//
static VOID
ReleaseBufferRequest(
    HCI_CONTEXT *pHCI
    )
{
    if (WaitForSingleObject(pHCI->hReadBufferEvent, 100) == WAIT_OBJECT_0)
        {
        pHCI->readPacketSize = 0;
        pHCI->pReadBuffer = NULL;
        pHCI->readBufferSize = 0;
        PulseEvent(pHCI->hReadPacketEvent);
        }
#ifdef _FM_
	if(!pHCI->FMOpened)
	{
#endif
    // We are done...
    pHCI->startRequest = FALSE;
    pHCI->request = REQUEST_NONE;
    RETAILMSG(0, (L"HCI:ReleaseBufferRequest(): SetEvent(hRequestDoneEvent)\r\n"));
    SetEvent(pHCI->hRequestDoneEvent);
#ifdef _FM_
   }
#endif
}

//------------------------------------------------------------------------------
//
//  Function:  ProcessReceivePacket
//
//  This function receives packet from BRF6300. When packet doesn't fit to
//  buffer (or buffer isn't ready) packet is trashed.
//
static VOID
ProcessReceivePacket(
    HCI_CONTEXT *pHCI,
    UCHAR packetType,
    DWORD headerSize
    )
{
    UCHAR *pBuffer;
    DWORD bufferSize, bodySize, packetSize;
    HCI_PACKET_HEADER *pHeader, header;
    DWORD size, count;
    BOOL ignorePacket = FALSE;


    // Start with zero packet length
    packetSize = 0;

    RETAILMSG(0, (L"HCI:ProcessReceivePacket(): waiting for pHCI->hReadBufferEvent \r\n"));
    // We must wait for read buffer
    WaitForSingleObject(pHCI->hReadBufferEvent, INFINITE);
    
    // We get buffer for packet
    pBuffer = pHCI->pReadBuffer;
    bufferSize = pHCI->readBufferSize;

    // Read packet header, use our header when buffer is too small
    if (headerSize > bufferSize)
        {
        pBuffer = (UCHAR*)&header;
        bufferSize = sizeof(header);
        ignorePacket = TRUE;
        }

    // We already have HCI packet type
    pHeader = (HCI_PACKET_HEADER*)pBuffer;
    pHeader->type = packetType;
    pBuffer += sizeof(packetType);

    // Read header
    if (!ReadData(pHCI, pBuffer, headerSize)) goto cleanUp;
    pBuffer += headerSize;

    // Get packet body size from header
    switch (packetType)
        {
        case EVENT_PACKET:
            bodySize = pHeader->event.length;
            break;
        case DATA_PACKET_SCO:
            bodySize = pHeader->sco.length;
            break;
        case DATA_PACKET_ACL:
            bodySize = pHeader->acl.length;
            break;
        default:
            // This should not happen...
            goto cleanUp;
        }

    // Check if packet fits to buffer
    size = sizeof(packetType) + headerSize + bodySize;
    if (ignorePacket || size > bufferSize)
        {
        UCHAR trash[16];
        while (bodySize > 0)
            {
            count = bodySize > sizeof(trash) ? sizeof(trash) : bodySize;
            if (!ReadData(pHCI, pBuffer, bodySize)) goto cleanUp;
            bodySize -= count;
            }
        goto cleanUp;
        }

    // Read packet body
    if (!ReadData(pHCI, pBuffer, bodySize)) goto cleanUp;

    // This is final packet size
    packetSize = sizeof(packetType) + headerSize + bodySize;

cleanUp:
    // Signal received packet
    if (!ignorePacket)
        {
        pHCI->readPacketSize = packetSize;
        pHCI->pReadBuffer = NULL;
        pHCI->readBufferSize = 0;
        SetEvent(pHCI->hReadPacketEvent);
        }
}

//------------------------------------------------------------------------------
//
//  Function:  ProcessWakeUpInd
//
//  This function process HCI_WAKE_UP_IND pseudo-packet. BRF6300 indicates
//  it desided awake.
//
static VOID
ProcessWakeUpInd(
				 HCI_CONTEXT *pHCI
    )
{
	if ((pHCI->btPowerMode == BT_SLEEP) ||
        (pHCI->btPowerMode == BT_SLEEP_TO_AWAKE))
        {
        UCHAR reply;
       
        // Ack indication to BRF6300
        reply = HCI_WAKE_UP_ACK;
        if (!WriteData(pHCI, &reply, sizeof(reply)))
            {
            pHCI->exitDispatchThread = TRUE;
            ASSERT(FALSE);
            goto cleanUp;
            }
        
        RETAILMSG(0, (L"HCI: ProcessWakeUpInd(): Send WakeUpAck\r\n"));

        
    
        // Done...
        pHCI->btPowerMode = BT_AWAKE;
        // If somebody wait for SLEEP, let him know
        if (pHCI->request == REQUEST_AWAKE)
            {
            pHCI->request = REQUEST_NONE;
            RETAILMSG(0, (L"HCI: ProcessWakeUpInd(): SetEven(hRequestDoneEvent)\r\n"));
            SetEvent(pHCI->hRequestDoneEvent);
            }
        }
    else
        {
	ASSERT(FALSE);
	}
    
cleanUp:
    return;
}

//------------------------------------------------------------------------------
//
//  Function:  ProcessWakeUpAck
//
//  This function process HCI_WAKE_UP_ACK pseudo-packet. It indicates
//  BRF6300 accepted HCI_WAKE_UP_IND.
//
static VOID
ProcessWakeUpAck(
    HCI_CONTEXT *pHCI
    )
{
    ASSERT(pHCI->btPowerMode == BT_SLEEP_TO_AWAKE);

    pHCI->btPowerMode = BT_AWAKE;
    // If somebody wait for AWAKE, let him know
    if (pHCI->request == REQUEST_AWAKE)
        {
        pHCI->request = REQUEST_NONE;
        RETAILMSG(0, (L"HCI: ProcessWakeUpAck(): SetEven(hRequestDoneEvent)\r\n"));
        SetEvent(pHCI->hRequestDoneEvent);
        }
}

//------------------------------------------------------------------------------
//
//  Function:  ProcessSleepInd
//
//  This function process HCI_GO_TO_SLEEP_IND pseudo-packet. BRF6300 indicates
//  it desided go to sleep mode.
//
static VOID
ProcessSleepInd(HCI_CONTEXT *pHCI)
{
	UCHAR reply;

    if ((pHCI->btPowerMode == BT_AWAKE) ||
        (pHCI->btPowerMode == BT_AWAKE_TO_SLEEP))
        {
                      
        if (pHCI->btPowerMode == BT_AWAKE)
        {
	       	// Ack indication to BRF6300
        	reply = HCI_GO_TO_SLEEP_ACK;
        	if (!WriteData(pHCI, &reply, sizeof(reply)))
            {
            	pHCI->exitDispatchThread = TRUE;
            	//IFDBG(DebugOut(DEBUG_ERROR, L"ERROR!HCI: ProcessSleepInd(): failed to call WriteData()\r\n"));
            	ASSERT(FALSE);
            	goto cleanUp;
            }
       
        RETAILMSG(0, (L"HCI: ProcessSleepInd(): Send GoToSleepAck\r\n"));

        }
        // Done...
        pHCI->btPowerMode = BT_SLEEP;
		
		Uart2GpioMode(pHCI);   
		RETAILMSG(TRUE, (L"HCI: ProcessSleepInd(): SetComPowerState D3 \r\n"));
      
		BT_WakeupEnabled = TRUE;	//enable ISR
        RETAILMSG(TRUE, (L"HCI: ProcessSleepInd(): 	BT_WakeupEnabled = TRUE \r\n"));
	  
        // If somebody wait for SLEEP, let him know
        if (pHCI->request == REQUEST_SLEEP)
            {
            pHCI->request = REQUEST_NONE;
            RETAILMSG(0, (L"HCI: ProcessSleepInd(): SetEvent(hRequestDoneEvent)\r\n"));
            SetEvent(pHCI->hRequestDoneEvent);
            }
        }
    else
        {
        ASSERT(FALSE);
        }
    
cleanUp:
    return;
}

//------------------------------------------------------------------------------
//
//  Function:  ProcessSleepAck
//
//  This function process HCI_GO_TO_SLEEP_ACK pseudo-packet. It indicates
//  BRF6300 accepted HCI_GO_TO_SLEEP_IND.
//
static VOID
ProcessSleepAck(
    HCI_CONTEXT *pHCI
    )
{
    ASSERT(pHCI->btPowerMode == BT_AWAKE_TO_SLEEP);

    // Set BRF6300 power state
    pHCI->btPowerMode = BT_SLEEP;

    // If somebody wait for AWAKE, let him know
    if (pHCI->request == REQUEST_SLEEP)
        {
        pHCI->request = REQUEST_NONE;
        RETAILMSG(0, (L"HCI: ProcessSleepAck(): SetEvent(hRequestDoneEvent)\r\n"));
        SetEvent(pHCI->hRequestDoneEvent);
        }
}

//------------------------------------------------------------------------------
//
//  Function:  DispatchThread
//
//  This is dispatch thread function. All communication with BRF6300 is made
//  from this thread. It serialize read/write communication with BRF6300. But
//  because of buffered serial port driver there should not be a penalty at
//  all. On other side it simplify 4-wire BRF6300 protocol implementation.
//
static DWORD
DispatchThread(
    VOID* pContext
    )
{
    HCI_CONTEXT *pHCI = (HCI_CONTEXT*)pContext;
    UCHAR packetType;
    DWORD count, mask = 0;


    // While we are not asked to exit thread
    while (!pHCI->exitDispatchThread)
        {
        RETAILMSG(0,(TEXT("HCI: +DispatchThread\r\n")));

        // If there is pending request process it
        if (pHCI->startRequest)
            {
            switch (pHCI->request)
                {
                case REQUEST_SEND:
                    RETAILMSG(0, (L"HCI:DispatchThread: REQUEST_SEND\r\n"));				
                    SendPacketRequest(pHCI);
                    break;
                case REQUEST_AWAKE:
                    RETAILMSG(0, (L"HCI:DispatchThread: REQUEST_AWAKE\r\n"));				
                    WakeUpRequest(pHCI);
                    break;
                case REQUEST_SLEEP:
                    RETAILMSG(0, (L"HCI:DispatchThread: REQUEST_SLEEP\r\n"));						
                    SleepRequest(pHCI);
                    break;
                case REQUEST_RELEASE_BUFFER:
                    RETAILMSG(0, (L"HCI:DispatchThread: REQUEST_RELEASE_BUFFER\r\n"));								
                    ReleaseBufferRequest(pHCI);
                    break;
                default:
                    pHCI->startRequest = FALSE;
                    pHCI->request = REQUEST_NONE;
                    RETAILMSG(0, (L"HCI:DispatchThread: REQUEST_NONE\r\n"));													
                    break;
                }
            }

       	RETAILMSG(0, (L"HCI:DispatchThread: pHCI->startRequest %d, pHCI->request %d\r\n",pHCI->startRequest,pHCI->request));													

        do 
            {
            // Try read packet type
            count = sizeof(packetType);
            if (!ReadFile(pHCI->hCom, &packetType, count, &count, NULL))
                {
                count = 0;
                break;
                }                

            // When we don't get packet type, wait for event
            if (count == 0) WaitCommEvent(pHCI->hCom, &mask, NULL);

            // If mask is zero, thread is wakeup by request
            if (mask == 0) break;

            }
        while (count < sizeof(packetType));

        // If there is no packet type it is request...
		if (count == 0) continue;
        
        // Depending on packet type
        switch (packetType)
            {
            case HCI_WAKE_UP_IND:
                
                RETAILMSG(0, (L"HCI:DispatchThread: Recv WakeUpInd\r\n"));

                ProcessWakeUpInd(pHCI);
                break;
            case HCI_WAKE_UP_ACK:
                
                RETAILMSG(0, (L"HCI:DispatchThread: Recv WakeUpAck\r\n"));

                ProcessWakeUpAck(pHCI);
                break;
            case HCI_GO_TO_SLEEP_IND:
               
                RETAILMSG(0, (L"HCI:DispatchThread: Recv GoToSleepInd\r\n"));

                ProcessSleepInd(pHCI);
                break;
            case HCI_GO_TO_SLEEP_ACK:
                
                RETAILMSG(0, (L"HCI:DispatchThread: Recv GoToSleepAck\r\n"));

                ProcessSleepAck(pHCI);
                break;
            case EVENT_PACKET:
                
                RETAILMSG(0, (L"HCI:DispatchThread: Recv %02x\r\n", packetType));

#ifdef _FM_
				//In order to save unnecessary copies,if only one stack is opened, 
				//call to specific function.
				if(pHCI->OpenedRefCount == 1)
				{
					if(pHCI->FMOpened)
					{
						//Call to the FM process event packet
						ProcessFMReceiveEventPacket(pHCI);
					}
					else
					{
						//Call to the original BT function
						ProcessReceivePacket(
							pHCI, packetType, sizeof(HCI_EVENT_PACKET_HEADER));
					}
				}
				else
				{	
					//Both stacks are opened.
					ProcessReceiveEventPacket(pHCI);	
				}
             
#else
                ProcessReceivePacket(
                    pHCI, packetType, sizeof(HCI_EVENT_PACKET_HEADER)
                    );
#endif
                break;
            case DATA_PACKET_SCO:
               
                RETAILMSG(0, (L"HCI:DispatchThread: Recv %02x\r\n", packetType));

                ProcessReceivePacket(
                    pHCI, packetType, sizeof(HCI_SCO_PACKET_HEADER)
                    );
                break;
            case DATA_PACKET_ACL:
                
                RETAILMSG(0, (L"HCI:DispatchThread: Recv %02x\r\n", packetType));

                ProcessReceivePacket(
                    pHCI, packetType, sizeof(HCI_ACL_PACKET_HEADER)
                    );
                break;
            default:
                ASSERT(FALSE);
                
                RETAILMSG(0, (L"ERROR! HCI:DispatchThread: Recv %02x\r\n", packetType));               

                pHCI->exitDispatchThread = TRUE;
                break;
            }

        }

    return ERROR_SUCCESS;
}

//------------------------------------------------------------------------------
static BOOL
ExecuteRequest(
    HCI_CONTEXT *pHCI,
    REQUEST request
    )
{
    // One request at time...
    EnterCriticalSection(&pHCI->writeCS);

    ASSERT(!pHCI->startRequest && pHCI->request == REQUEST_NONE);

    // Set request
    pHCI->request = request;
    pHCI->startRequest = TRUE;

    // This should release WaitCommMask
    SetCommMask(pHCI->hCom, EV_RXCHAR);

    // Wait until request is done (should we timeout?)...
    WaitForSingleObject(pHCI->hRequestDoneEvent, INFINITE);

    // We are done...
    LeaveCriticalSection(&pHCI->writeCS);

    // Done
    return TRUE;
}

//------------------------------------------------------------------------------
static BOOL
WritePacket(
    HCI_CONTEXT *pHCI,
    UCHAR *pBuffer,
    DWORD size,
    DWORD timeout
    )
{
    BOOL rc = FALSE;

    EnterCriticalSection(&pHCI->writeCS);

    // Let do some checks
    ASSERT(pHCI->pWritePacket == NULL);
    ASSERT(!pHCI->startRequest && pHCI->request == REQUEST_NONE);

    // Submit request to dispatch thread
    pHCI->pWritePacket = pBuffer;
    pHCI->writePacketSize = size;
    pHCI->request = REQUEST_SEND;
    pHCI->startRequest = TRUE;

    // This should release WaitCommMask
    SetCommMask(pHCI->hCom, EV_RXCHAR);

    //DumpBuffer( pBuffer, size ); 		
	
    // Wait until request is done...
    DWORD dwWait = WaitForSingleObject(pHCI->hRequestDoneEvent, timeout);
    

    if (dwWait != WAIT_OBJECT_0)
        {
        switch (dwWait) 
            {
            case WAIT_FAILED:
                IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!WritePacket: "
                    L"WaitForSingleObject failed on hRequestDoneEvent (0x%08x)\r\n",
                    GetLastError())
                    ));
                break;
            case WAIT_TIMEOUT:
                IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!WritePacket: "
                    L"WaitForSingleObject timeout on hRequestDoneEvent\r\n")
                    ));
                break;
            }
    
        goto cleanUp;
        }

    // Done...
    rc = TRUE;

cleanUp:
    LeaveCriticalSection(&pHCI->writeCS);
    return rc;
}

//------------------------------------------------------------------------------
static DWORD
ReadPacket(
    HCI_CONTEXT *pHCI,
    UCHAR *pBuffer, 
    DWORD size, 
    DWORD timeout
    )
{
    DWORD rc;

    EnterCriticalSection(&pHCI->readCS);
#ifdef _FM_
	EnterCriticalSection(&pHCI->FakeCommandCS);
#endif
    // Submit packet to receive thread
    ASSERT(pHCI->pReadBuffer == NULL);
    pHCI->pReadBuffer = pBuffer;
    pHCI->readBufferSize = size;
    if (!SetEvent(pHCI->hReadBufferEvent))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!ReadPacket: "
            L"SetEvent failed on hReadBufferEvent (0x%08x)\r\n",
            GetLastError())
            ));
#ifdef _FM_
		LeaveCriticalSection(&pHCI->FakeCommandCS);
#endif  
        goto cleanUp;
        }
     RETAILMSG(0,(TEXT("HCI ReadPacket(): set pHCI->hReadBufferEvent\r\n")));
#ifdef _FM_
	if(pHCI->FakeResetComp)
	{
		//fake a reset complete event for the BT stack. 
		FakeResetComplete();
		pHCI->FakeResetComp = FALSE;

	}
	LeaveCriticalSection(&pHCI->FakeCommandCS);

#endif
    // Wait for packet
    rc = WaitForSingleObject(pHCI->hReadPacketEvent, timeout);
    switch (rc)
        {
        case WAIT_FAILED:
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!ReadPacket: "
                L"WaitForSingleObject failed on hReadPacketEvent (0x%08x)\r\n",
                GetLastError())
                ));
            size = 0;
            break;
        case WAIT_TIMEOUT:
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!ReadPacket: "
                L"WaitForSingleObject timeout on hReadPacketEvent \r\n")
                ));
            size = 0;
            break;
        case WAIT_OBJECT_0:
            // Get received packet size
            size = pHCI->readPacketSize;
            break;
        }

    // Clear buffer from HCI context (just to catch errors)
    pHCI->pReadBuffer = NULL;
    pHCI->readBufferSize = 0;
    pHCI->readPacketSize = 0;

cleanUp:
    LeaveCriticalSection(&pHCI->readCS);
    return size;
}

//------------------------------------------------------------------------------
static BOOL
ExecuteCommand(
    HCI_CONTEXT *pHCI,
    UCHAR *pPacket,
    DWORD size,
    DWORD timeout
    )
{
    BOOL rc = FALSE;
    UCHAR reply[255 + 4];
    HCI_PACKET_HEADER *pCommand;
    HCI_PACKET_COMMAND_COMPLETE_EVENT *pEvent;

    // Set helper pointers
    pCommand = (HCI_PACKET_HEADER*)pPacket;
    pEvent = (HCI_PACKET_COMMAND_COMPLETE_EVENT *)reply;

    ASSERT(pCommand->type == COMMAND_PACKET);

    // Write command
    if (!WritePacket(pHCI, pPacket, size, timeout))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!ExecuteCommand: "
            L"WritePacket type 0x%02x length %d failed\r\n",
            pPacket[0], size)
            ));
        goto cleanUp;
        }

    // Wait for ack
    while (TRUE)
        {
        // Read packet
        if (ReadPacket(pHCI, reply, sizeof(reply), timeout) == 0)
            {
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!ExecuteCommand: "
                L"ReadPacket failed\r\n")
                ));
            goto cleanUp;
            }

        // Ignore all out-off sync packets
        pEvent = (HCI_PACKET_COMMAND_COMPLETE_EVENT *)reply;
        if (pEvent->type != EVENT_PACKET) continue;
        if (pEvent->eventCode != COMMAND_COMPLETE_EVENT_CODE) continue;
        if (pEvent->commandOpcode != pCommand->command.code) continue;

        // Check result...
        rc = (pEvent->status == 0);
        if (!rc)
            {
            IFDBG(DebugOut(DEBUG_WARN, (L"WARN: HCI!ExecuteCommand: "
                L"Command 0x%04x return status 0x%02x\r\n",
                pCommand->command.code, pEvent->status)
                ));
            }

        // Done...
        break;
    }

cleanUp:
    return rc;
}

//------------------------------------------------------------------------------
static BOOL
RunScript(
    HCI_CONTEXT *pHCI,
    LPCWSTR fileName
    )
{
    BOOL rc = FALSE;
    HANDLE hFile= INVALID_HANDLE_VALUE;
    SCRIPT_HEADER header;
    SCRIPT_ACTION_HEADER action;
    ACTION_SERIAL_PORT_PARAMS *pSerialParams;
    ACTION_DELAY_PARAMS *pDelayParams;
    DCB dcb;
    UCHAR buffer[MAX_PATH * sizeof(WCHAR)];
    DWORD count;

    RETAILMSG(1,(TEXT("+RunScript:%s\n"),fileName));
    // Add path to file name and convert to unicode...
    if (FAILED(StringCbPrintf((LPWSTR)buffer, sizeof(buffer), L"Windows\\%s",
            fileName)))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
            L"Failed extend file name '%s'\r\n", fileName)
            ));
        goto cleanUp;
        }

    // Open script file, exit when it is missing
    hFile = CreateFile(
        (LPWSTR)buffer, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL
        );
    if (hFile == INVALID_HANDLE_VALUE)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
            L"Failed open script file name '%s'\r\n", buffer)
            ));
		RETAILMSG(1,(TEXT("Failed open script file name '%s'\r\n"),buffer));
        goto cleanUp;
        }

    // Read script header
    if (!ReadFile(hFile, &header, sizeof(header), &count, NULL) ||
        (count < sizeof(header)))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
            L"Failed read header from script file\r\n")
            ));
		RETAILMSG(1,(TEXT("Failed read header from script file\r\n")));
        goto cleanUp;
        }

    // Header magic must match
    if (header.magicNumber != SCRIPT_HEADER_MAGIC)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
            L"Script magic number/cookie %08x doesn't match\r\n")
            ));
        goto cleanUp;
        }

    while (TRUE)
        {
        // Read action header
        if (!ReadFile(hFile, &action, sizeof(action), &count, NULL))
            {
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                L"Failed read action from script file\r\n")
                ));
			RETAILMSG(1,(TEXT("Failed read action from script file \r\n")));
            goto cleanUp;
            }

        // If we didn't get full action header assume end of script
        if (count < sizeof(action)) 
		{ RETAILMSG(1,(TEXT("HCI: RunScript(): reach the end of script\r\n"))); break;}

        // Make sure that parameters fit to buffer
        if (action.size > sizeof(buffer))
            {
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                L"Action parameters size too big (%d)\r\n", action.size)
                ));
            goto cleanUp;
            }

        // Read buffer
        if (!ReadFile(hFile, buffer, action.size, &count, NULL) ||
            (count < action.size))
            {
            IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                L"Failed read action parameters (read %d from %d)\r\n",
                count, action.size)
                ));
			RETAILMSG(1,(TEXT("Failed read action parameters  \r\n")));
            goto cleanUp;
            }

		switch (action.code)
            {
            case ACTION_SEND_COMMAND:
                // Write packet
                RETAILMSG(0,(TEXT("HCI: RunScript(): ACTION_SEND_COMMAND\r\n")));
                if (!WritePacket(pHCI, buffer, count, pHCI->driverTimeout))
                    {
                       	IFDBG(DebugOut(DEBUG_ERROR,( L"ERROR: HCI!RunScript: " L"Failed write HCI packet to Bluetooth device\r\n")));
                    	RETAILMSG(1,(TEXT("Failed write HCI packet to Bluetooth device  \r\n")));
                    	goto cleanUp;
                    }
                count = ReadPacket(pHCI, buffer, sizeof(buffer), pHCI->driverTimeout);
                if (count == 0)
                    {
                    	IFDBG(DebugOut(DEBUG_ERROR,( L"ERROR: HCI!RunScript: "L"Failed read HCI packet from Bluetooth device\r\n")));
                    RETAILMSG(1,(TEXT("Failed read HCI packet from Bluetooth device  \r\n")));
                    	goto cleanUp;
                    }
                RETAILMSG(0,(TEXT("HCI RunScript(): Response from BT module:")));
		   		//DumpBuffer( buffer, count);
                break;

            case ACTION_SERIAL_PORT:
                // Check action parameters
                RETAILMSG(0,(TEXT("HCI: RunScript(): ACTION_SERIAL_PORT\r\n")));               
                if (count < sizeof(ACTION_SERIAL_PORT_PARAMS))
                    {
                    IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                        L"Parameters for ACTION_SERIAL_PORT too small (%d)\r\n",
                        count)
                        ));
					RETAILMSG(1,(TEXT("Parameters for ACTION_SERIAL_PORT too small  \r\n")));
					
                    goto cleanUp;
                    }
                pSerialParams = (ACTION_SERIAL_PORT_PARAMS*)&buffer[0];
                // Get actual serial port parameters
                dcb.DCBlength = sizeof(dcb);
                if (!GetCommState(pHCI->hCom, &dcb))
                    {
                    IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                        L"Failed get serial port parameters\r\n")
                        ));
                    goto cleanUp;
                    }
                // Update the communication parameters structure
                if (pSerialParams->baudRate != DONT_CHANGE)
                    {
                    dcb.BaudRate = pSerialParams->baudRate;
                    }
                if (pSerialParams->flowControl != DONT_CHANGE)
                    {
                    
                    switch (pSerialParams->flowControl)
                        {
                        case FCT_NONE:
                            dcb.fOutxCtsFlow = 0;
                            dcb.fRtsControl = RTS_CONTROL_ENABLE;
                            break;
                        case FCT_HARDWARE:
                            dcb.fOutxCtsFlow = 1;
                            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
                            break;
                        
                        }
                    
                    }
                // Update
                if (!SetCommState(pHCI->hCom, &dcb))
                    {
                    IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                        L"Failed set serial port parameters\r\n")
                        ));
			RETAILMSG(1,(TEXT("Failed set serial port parameters \r\n")));
			RETAILMSG(1,(TEXT("Failed set serial port parameters \r\n")));
				
                    goto cleanUp;
                    }
                break;

            case ACTION_DELAY:
                // Check action parameters
                RETAILMSG(0,(TEXT("HCI: RunScript(): ACTION_DELAY\r\n")));               
                if (count < sizeof(ACTION_DELAY_PARAMS))
                    {
                    IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI!RunScript: "
                        L"Parameters for ACTION_SERIAL_PORT too small (%d)\r\n",
                        count)
                        ));
                        RETAILMSG(1,(TEXT("Parameters for ACTION_SERIAL_PORT too small\r\n")));
				
                    goto cleanUp;
                    }
                pDelayParams = (ACTION_DELAY_PARAMS*)&buffer[0];
                // Do it...
                Sleep(pDelayParams->timeToWait);
                break;

            case ACTION_RUN_SCRIPT:
                // Convert name to unicode
                RETAILMSG(0,(TEXT("HCI: RunScript(): ACTION_RUN_SCRIPT\r\n")));       
                StringCbPrintf(
                    (LPWSTR)(buffer + action.size), 
                    sizeof(buffer) - action.size, L"%hs", buffer
                    );
                // Recourse
                if (!RunScript(pHCI, (LPWSTR)(buffer + action.size)))
                    {
                    goto cleanUp;
                    }
                break;

            case ACTION_REMARKS:
            case ACTION_WAIT_EVENT:
            default:
                RETAILMSG(0,(TEXT("HCI: RunScript(): ACTION_REMARKS or ACTION_WAIT_EVENT or others \r\n")));       		
                break;
            }
        }

    // Done
    rc = TRUE;

cleanUp:
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);

    RETAILMSG(1,(TEXT("HCI: -RunScript(): %d\r\n"), rc));
    return rc;
}

//------------------------------------------------------------------------------
static BOOL
StartScript(
    HCI_CONTEXT *pHCI
    )
{
    BOOL rc = FALSE;
    HCI_PACKET_HEADER header;
    UCHAR buffer[32];
    WCHAR fileName[64];
    HCI_PACKET_HEADER *pHeader = (HCI_PACKET_HEADER*)buffer;
    USHORT manufacturerID, version;
    USHORT projectType, minorVersion, majorVersion;
    DWORD size;


    // First get BT version
    header.type = COMMAND_PACKET;
    header.command.code = HCI_READ_LOCAL_VERSION_INFORMATION;
    header.command.length = 0;
    if (!WritePacket(
            pHCI, (UCHAR*)&header,
            sizeof(UCHAR) + sizeof(HCI_COMMAND_PACKET_HEADER), pHCI->driverTimeout
            ))
        {
        goto cleanUp;
        }

    // Get response...
    if (((size = ReadPacket(pHCI, buffer, sizeof(buffer), pHCI->driverTimeout)) == 0) ||
        (pHeader->type != EVENT_PACKET) ||
        (pHeader->event.code != COMMAND_COMPLETE_EVENT_CODE) ||
        (pHeader->event.length < 12))
        {
        goto cleanUp;
        }

    // Check for TI's id
    manufacturerID = MAKEWORD(buffer[11], buffer[12]);
    if (manufacturerID != TI_MANUFACTURER_ID)
        {
        goto cleanUp;
        }

    // Get project & versions
    version = MAKEWORD(buffer[13], buffer[14]);
    projectType  = (version & 0x7C00) >> 10;
    minorVersion = (version & 0x007F);
    majorVersion = (version & 0x0380) >> 7;
    if ((version & 0x8000) != 0) majorVersion |= 0x0008;

    // Create file name
    StringCbPrintf(
        fileName, sizeof(fileName), pHCI->startScript, 
        projectType, majorVersion, minorVersion
        );

    // Run script engine...
    if (!RunScript(pHCI, fileName)) 
       goto cleanUp;

    // Wait for while to receive "HCILL_GOTO_SLEEP_IND_MSG" from BRF
    Sleep(3);		// if the delays is too long, OS will send HCI_RESET cmd to reset HCI trans layer
    	
    // Done
    rc = TRUE;

cleanUp:
    return rc;
}

//------------------------------------------------------------------------------
int HCI_StartHardware(void)
{
	RETAILMSG(1, (L"\r\n BT Start HW\r\n"));
	if(g_hci.hGpio != NULL)
	{
		GPIOSetMode(g_hci.hGpio, BT_GPIO_SHUTDOWN, GPIO_DIR_OUTPUT );
		GPIOSetBit(g_hci.hGpio, BT_GPIO_SHUTDOWN);
   		Sleep(150);
   	}
   	return TRUE;
}

//------------------------------------------------------------------------------
int HCI_StopHardware(void)
{
	RETAILMSG(1, (L"\r\n BT Stop HW\r\n"));
	if(g_hci.hGpio != NULL)
	{
		GPIOSetMode(g_hci.hGpio, BT_GPIO_SHUTDOWN, GPIO_DIR_OUTPUT );
		GPIOClrBit(g_hci.hGpio, BT_GPIO_SHUTDOWN);
		Sleep(150);
	}
	return TRUE;
}


//------------------------------------------------------------------------------
int
HCI_SetCallback(
    HCI_TransportCallback pfnCallback
    )
{
    g_hci.pfnCallback = pfnCallback;

    if (pfnCallback != NULL)
        {
        DebugInit();
        }
    else 
        {
        DebugDeInit();
        }
    return ERROR_SUCCESS;
}

//------------------------------------------------------------------------------
int
HCI_OpenConnection(
    )
{
    int rc = FALSE;
    COMMTIMEOUTS commTimeOuts;
    DCB dcb;
#if HW_GPIO   
    PHYSICAL_ADDRESS pa;
#endif

    IFDBG(DebugOut(DEBUG_HCI_INIT,( L"+HCI_OpenConnection\r\n")));
    RETAILMSG(TRUE, (L" +++ HCI_OpenConnection +++ 000000\r\n"));
#ifdef _FM_
	EnterCriticalSection(&g_hci.OpenCS);

	g_hci.FakeResetComp = FALSE;
	
	//Check if the chip is already intilized, if so we don't have to do any thing.
	if(g_hci.opened)
	{
		g_hci.OpenedRefCount++;
		rc = TRUE;
		goto cleanUp;
	}
	
	g_hci.OpenedRefCount++;
#endif
    // Read parameters from registry
    if (GetDeviceRegistryParams(
            L"Software\\Microsoft\\Bluetooth\\Transports\\BuiltIn\\1", &g_hci,
            dimof(g_deviceRegParams), g_deviceRegParams
            ) != ERROR_SUCCESS)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"Failed read HCI registry parameters\r\n")
            ));
        goto cleanUp;
        }


    IFDBG(DebugOut(DEBUG_HCI_INIT,
        (L"Opening port %s (rate %d) for I/O with unit\n",
        g_hci.comPortName, g_hci.baud
        )));

    g_hci.hCom = CreateFile(
        g_hci.comPortName, GENERIC_READ|GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
        );

    if (g_hci.hCom == INVALID_HANDLE_VALUE)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"Failed open UART HCI interface (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        g_hci.hCom = NULL;
        goto cleanUp;
        }
 
//#if HW_GPIO
#if 1
    //Open GPIO device and start hardware
    /*
    pa.QuadPart = OMAP2430_SYSC1_PADCONFS1_REGS_PA;
    g_hci.pPadConfig = (OMAP2430_SYSC_PADCONFS1_REGS *)MmMapIoSpace(pa, sizeof(OMAP2430_SYSC_PADCONFS1_REGS), FALSE);
    if(g_hci.pPadConfig == NULL) 
	{
    	RETAILMSG(1, (L" OMAP2430 BRF6300! OMAP2430_PADCONFS1_REGS:"
                    L"Failed to map PADCONFS1 registers (pa = 0x%08x)\r\n", pa.LowPart
        ));
        goto cleanUp; 
    }

    //set MUX register
   MASKREG32(&(g_hci.pPadConfig->ulCONTROL_SYS_CLKREQ), BIT28|BIT27|BIT26|BIT25|BIT24, BIT27); // Mode 0, Pulldown, enabled
    */
    g_hci.hGpio = GPIOOpen();
    if(g_hci.hGpio == NULL)
    {
    	RETAILMSG(1, (L"ERROR: HCI_OpenConnection: "L"Failed open GPIO interface (GetLastError() = 0x%08x)\r\n",GetLastError()));
        goto cleanUp;
    }

    HCI_StartHardware(); 
#else
g_hci.hGpio = NULL;
#endif
    // Purge any information in the buffer
    if (!PurgeComm(
            g_hci.hCom, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR
        ))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"PurgeComm failed on HCI UART (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }
	
    if (g_hci.hCom == INVALID_HANDLE_VALUE)
    {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"Failed open UART HCI interface (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
		}
    

    memset(&dcb, 0, sizeof(dcb));
    
    if (!GetCommState(g_hci.hCom, &dcb))
    {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"GetCommState failed on HCI UART (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
    }
    
    dcb.BaudRate = g_hci.baud;		//g_hci.baud --> 115200
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;

#if 0

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = g_hci.baud;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    dcb.fOutxCtsFlow = TRUE;
    dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fTXContinueOnXoff = FALSE;
    dcb.fErrorChar = FALSE;
    dcb.fNull = FALSE;
    dcb.fAbortOnError = TRUE;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.XonChar = 0x11;
    dcb.XoffChar = 0x13;
    dcb.XonLim = 3000;
    dcb.XoffLim = 9000;
#endif
    if (!SetCommState(g_hci.hCom, &dcb))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"SetCommState failed on HCI UART (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

 
    // Set serial port timeouts. We want don't want to wait
    // in ReadFile (instead we will use WaitCommEvent).
    commTimeOuts.ReadIntervalTimeout = MAXDWORD;
    commTimeOuts.ReadTotalTimeoutMultiplier = MAXDWORD;
    commTimeOuts.ReadTotalTimeoutConstant = 0;
    commTimeOuts.WriteTotalTimeoutMultiplier = 1;
    commTimeOuts.WriteTotalTimeoutConstant = 100;
    if (!SetCommTimeouts(g_hci.hCom, &commTimeOuts))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"SetCommTimeouts failed on HCI UART (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

    // Dispatch thread uses comm events
    SetCommMask(g_hci.hCom, EV_RXCHAR);

    RETAILMSG(0, (L"HCI: HCI_OpenConnection: after SetCommMask ();\r\n"));
	
#if HW_GPIO
    //Setup wakeup mode
    BT_InitInterruptThread();
#endif
	
    // No read buffer, write packet or request
    g_hci.pReadBuffer = NULL;
    g_hci.readBufferSize = 0;
    g_hci.readPacketSize = 0;
    g_hci.pWritePacket = NULL;
    g_hci.writePacketSize = 0;
    g_hci.startRequest = FALSE;
    g_hci.request = REQUEST_NONE;

    // We expect BT is awake now
    g_hci.btPowerMode = BT_AWAKE;

    InitializeCriticalSection(&g_hci.writeCS);
    InitializeCriticalSection(&g_hci.readCS);
#ifdef _FM_
	InitializeCriticalSection(&g_hci.FakeCommandCS);
#endif
    g_hci.hReadBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_hci.hReadBufferEvent == NULL)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"CreateEvent failed (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

    g_hci.hReadPacketEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_hci.hReadPacketEvent == NULL)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"CreateEvent failed (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

    g_hci.hRequestDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_hci.hRequestDoneEvent == NULL)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"CreateEvent failed (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

    // Start dispatch thread
    g_hci.exitDispatchThread = FALSE;
    g_hci.hDispatchThread = CreateThread(
        NULL, 0, DispatchThread, &g_hci, 0, NULL
        );
    if (g_hci.hDispatchThread == NULL)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_OpenConnection: "
            L"CreateThread failed (GetLastError() = 0x%08x)\r\n",
            GetLastError())
            ));
        goto cleanUp;
        }

    // Set read thread priority
    CeSetThreadPriority(g_hci.hDispatchThread, g_hci.priority256);

    // Done
    rc = TRUE;
    g_hci.opened = TRUE;
#ifdef _FM_
	g_hci.WaitForInitScript = TRUE;
#endif
cleanUp:
#ifdef _FM_
	LeaveCriticalSection(&g_hci.OpenCS);
#endif
    if (!rc) HCI_CloseConnection();
    RETAILMSG(TRUE, (L" --- HCI_OpenConnection ---\r\n"));
    IFDBG(DebugOut(DEBUG_HCI_INIT, (L"-HCI_OpenConnection(%d)\r\n", rc)));
    return rc;
}

//------------------------------------------------------------------------------
VOID
HCI_CloseConnection(
    )
{
    RETAILMSG(TRUE, (L" +++ HCI_CloseConnection +++\r\n"));

    // Dispatch thread must have higher priority
    ASSERT(CeGetThreadPriority(GetCurrentThread()) > (int)g_hci.priority256);

    IFDBG(DebugOut(DEBUG_HCI_INIT,( L"+HCI_CloseConnection\r\n")));

    // If hComm is NULL we don't open connection at all...
    if (g_hci.hCom == NULL) goto cleanUp;

#ifdef _FM_
	EnterCriticalSection(&g_hci.OpenCS);
	if(g_hci.OpenedRefCount == 0)
	{
		goto cleanUp;
	}
	
	if(g_hci.OpenedRefCount > 1)
	{
		g_hci.OpenedRefCount--;
		if(g_hci.FMOpened)
		{
			ReleaseBufferRequest(&g_hci);
		}
		goto cleanUp;
	}
#endif

#if 1  //release the reader thread -@CY
   SetEvent(g_hci.hReadPacketEvent);
   SetEvent(g_hci.hRequestDoneEvent);
#endif
 
    // If connection is opened, we need to start close connection actions
    if (g_hci.opened)
    {
        // Send request for buffer release, it will release HCI_ReadPacket
        ExecuteRequest(&g_hci, REQUEST_RELEASE_BUFFER);
    }

#if HW_GPIO
	//close wakeup mode
	BT_ReleaseInterruptThread();
#endif

    // Set exit flag before we close COM port
    g_hci.exitDispatchThread = TRUE;

    // Close COM port
    CloseHandle(g_hci.hCom);
    g_hci.hCom = NULL;

    // If thread exist wait until it exists
    if (g_hci.hDispatchThread != NULL)
        {
        WaitForSingleObject(g_hci.hDispatchThread, INFINITE);
        CloseHandle(g_hci.hDispatchThread);
        g_hci.hDispatchThread = NULL;
        }
    if (g_hci.hReadBufferEvent != NULL)
        {
        CloseHandle(g_hci.hReadBufferEvent);
        g_hci.hReadBufferEvent = NULL;
        }
    if (g_hci.hReadPacketEvent != NULL)
        {
        CloseHandle(g_hci.hReadPacketEvent);
        g_hci.hReadPacketEvent = NULL;
        }
    if (g_hci.hRequestDoneEvent != NULL)
        {
        CloseHandle(g_hci.hRequestDoneEvent);
        g_hci.hRequestDoneEvent = NULL;
        }
    DeleteCriticalSection(&g_hci.writeCS);
    Sleep(100);
    DeleteCriticalSection(&g_hci.readCS);
#ifdef _FM_
	DeleteCriticalSection(&g_hci.FakeCommandCS);
#endif
	//stop hardware and close GPIO
	HCI_StopHardware();

	if(g_hci.hGpio != NULL)
	{
		GPIOClose(g_hci.hGpio);
    	g_hci.hGpio = NULL;
    }
//	MmUnmapIoSpace((VOID *)g_hci.pPadConfig, sizeof(OMAP2430_SYSC_PADCONFS1_REGS));

g_hci.opened = FALSE;		
#ifdef _FM_
g_hci.OpenedRefCount--;
#endif

cleanUp:
#ifdef _FM_
LeaveCriticalSection(&g_hci.OpenCS);
#endif
    RETAILMSG(TRUE, (L" --- HCI_CloseConnection ---\r\n"));
    IFDBG(DebugOut(DEBUG_HCI_INIT, (L"-HCI_CloseConnection\r\n")));
}

//------------------------------------------------------------------------------
int
HCI_ReadHciParameters(
    HCI_PARAMETERS *pParms
    )
{
    BOOL rc = FALSE;

    // Check size
    if (pParms->uiSize < sizeof (*pParms)) goto cleanUp;
    memset(pParms, 0, sizeof(*pParms));

    pParms->uiSize             = sizeof(*pParms);
    pParms->fInterfaceVersion  = HCI_INTERFACE_VERSION_1_1;
    pParms->iMaxSizeRead       = PACKET_SIZE_R;
    pParms->iMaxSizeWrite      = PACKET_SIZE_W;
    pParms->iWriteBufferHeader = 4;
    pParms->iReadBufferHeader  = 4;
    pParms->uiWriteTimeout     = g_hci.writeTimeout;
    pParms->uiResetDelay       = g_hci.resetDelay;
    pParms->uiFlags            = g_hci.flags;
    pParms->uiDriftFactor      = g_hci.drift;
    if (g_hci.specV10a)
        {
        pParms->fHardwareVersion = HCI_HARDWARE_VERSION_V_1_0_A;
        }
    else if (g_hci.specV11)
        {
        pParms->fHardwareVersion = HCI_HARDWARE_VERSION_V_1_1;
        }
    else
        {
        pParms->fHardwareVersion = HCI_HARDWARE_VERSION_V_1_0_B;
        }
#if defined (DEBUG) || defined (_DEBUG)
    pParms->iReadBufferHeader   = DEBUG_READ_BUFFER_HEADER;
    pParms->iReadBufferTrailer  = DEBUG_READ_BUFFER_TRAILER;
    pParms->iWriteBufferHeader  = DEBUG_WRITE_BUFFER_HEADER;
    pParms->iWriteBufferTrailer = DEBUG_WRITE_BUFFER_TRAILER;
#endif

    // Done
    rc = TRUE;

cleanUp:
    return rc;
}

#ifdef _FM_
//------------------------------------------------------------------------------
static DWORD
FakeResetComplete(
        )
{
	DWORD rc = FALSE;
	HCI_PACKET_COMMAND_COMPLETE_EVENT *pPacket;

	//We must wait for read buffer
    if(WaitForSingleObject(g_hci.hReadBufferEvent, 0) != WAIT_OBJECT_0)
	{
		return rc;
	}

	if(g_hci.readBufferSize < sizeof(HCI_PACKET_COMMAND_COMPLETE_EVENT))
	{
		goto cleanUp;
	}

	pPacket  = (HCI_PACKET_COMMAND_COMPLETE_EVENT*)g_hci.pReadBuffer;

	//fill the reset complete event packet
	pPacket->type = EVENT_PACKET;
	pPacket->eventCode = COMMAND_COMPLETE_EVENT_CODE;
	pPacket->length = 4;
	pPacket->commandOpcode = HCI_RESET;
	pPacket->status = 0;
	pPacket->commandPackets = 0x1;

	rc = TRUE;

cleanUp:
	g_hci.pReadBuffer = NULL;
	g_hci.readBufferSize = 0;
	
	if (rc == TRUE)
	{
        g_hci.readPacketSize =  sizeof(HCI_PACKET_COMMAND_COMPLETE_EVENT);
	}
	else
	{
		g_hci.readPacketSize = 0;
	}
	
	SetEvent(g_hci.hReadPacketEvent);
	
    return TRUE;
}

#endif
//------------------------------------------------------------------------------
int
HCI_WritePacket(
    HCI_TYPE eType, 
    BD_BUFFER *pPacket
    )
{
    int rc = FALSE;

#if DbgPacket
    IFDBG(DebugOut(DEBUG_HCI_TRANSPORT,
        (L"HCI_WritePacket type 0x%02x length %d\r\n",
        eType, BufferTotal(pPacket))
        ));
#endif
    //DumpBuffer(pPacket->pBuffer + pPacket->cStart, BufferTotal(pPacket));

#if defined(DEBUG) || defined(_DEBUG)
    ASSERT(pPacket->cStart == DEBUG_WRITE_BUFFER_HEADER);
    ASSERT(pPacket->cSize - pPacket->cEnd >= DEBUG_WRITE_BUFFER_TRAILER);
#endif

    ASSERT ((pPacket->cStart & 0x3) == 0);

    // Make sure that hardware is started...
    if (!g_hci.opened)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"HCI_WritePacket - not active\r\n")));
        goto cleanUp;
        }

    // Check if we get packet with right size
    if (((int)BufferTotal(pPacket) > PACKET_SIZE_W) || (pPacket->cStart < 1))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_WritePacket: "
            L"Packet too big (%d, max %d), or no space for header!\r\n",
            BufferTotal(pPacket), PACKET_SIZE_W)
            ));
        goto cleanUp;
        }


    // Set packet type to buffer
    pPacket->pBuffer[--pPacket->cStart] = (UCHAR)eType;
#ifdef _FM_
	//If The FM stack already send reset command we have to avoid sending it again 
	//from the BT stack. We have to complete the command without sending it to the chip and 
	//to fake a RESET_COMPLETE answer to the BT stack.

	if (g_hci.FMOpened)
	{
		HCI_PACKET_HEADER * pHCICmd =(HCI_PACKET_HEADER*)(pPacket->pBuffer + pPacket->cStart);
		if((pHCICmd->type == COMMAND_PACKET) && (pHCICmd->command.code == HCI_RESET))
		{
			IFDBG(RETAILMSG(DEBUG_HCI_TRANSPORT,( L"HCI_WritePacket: "
				L"Bt stack sent Reset command\r\n")));			
			EnterCriticalSection(&g_hci.FakeCommandCS);
			if(!FakeResetComplete())
			{
			  g_hci.FakeResetComp = TRUE;
			}
			//ToDo we have to emulate reset complete event.
			rc = TRUE;
			LeaveCriticalSection(&g_hci.FakeCommandCS);
			goto cleanUp;
rc = TRUE;
	goto cleanUp;
		}
	}
#endif
    // Write packet...
    if (!WritePacket(
            &g_hci, pPacket->pBuffer + pPacket->cStart,
            BufferTotal(pPacket), g_hci.driverTimeout
            ))
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_WritePacket: "
            L"WritePacket type 0x%02x length %d failed\r\n",
            eType, BufferTotal(pPacket))
            ));
        goto cleanUp;
        }
#if DbgPacket
    IFDBG(DebugOut(DEBUG_HCI_TRANSPORT, (L"HCI_WritePacket: "
        L"DONE type 0x%02x length %d\r\n", eType, BufferTotal(pPacket))
        ));
#endif
    // Done
    rc = TRUE;

cleanUp:
    return rc;
}

//------------------------------------------------------------------------------
int
HCI_ReadPacket(
    HCI_TYPE *peType,
    BD_BUFFER *pPacket
    )
{
    int rc = FALSE;
    UCHAR *pBuffer;
    HCI_PACKET_COMMAND_COMPLETE_EVENT *pEventPacket;
    DWORD size;

#if DbgPacket
    IFDBG(DebugOut(DEBUG_HCI_TRANSPORT, (L"+HCI_ReadPacket\r\n")));
#endif

    if (!g_hci.opened)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_ReadPacket: "
            L"Connection is not active\r\n")
            ));
        goto cleanUp;
        }

    pPacket->cStart = 3;
    pPacket->cEnd = pPacket->cSize + 3;
    if (BufferTotal(pPacket) < 257)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_ReadPacket: "
            L"Buffer too small (%d bytes, should be 260)\r\n",
            BufferTotal(pPacket))
            ));
        goto cleanUp;
        }

#if DbgPacket
    RETAILMSG(TRUE, (
        L"HCI_ReadPacket  - buffer size %3d start %3d end %d\r\n", 
        pPacket->cEnd - pPacket->cStart, pPacket->cStart, pPacket->cEnd 
    ));
#endif

    // Read packet
    size = ReadPacket(
        &g_hci, pPacket->pBuffer + pPacket->cStart,
        BufferTotal(pPacket), INFINITE
        );

    // If returned size is zero, something bad happen...
    if (size == 0)
        {
        IFDBG(DebugOut(DEBUG_ERROR, (L"ERROR: HCI_ReadPacket: "
            L"ReadPacket return zero length packet\r\n")
            ));
        goto cleanUp;
        }

    // Fix packet start & end
    pPacket->cEnd = pPacket->cStart + size;
    pPacket->cStart++;

    // We need detect EVENT packet which is response
    // for HCI_RESET command packet.
    pBuffer = &pPacket->pBuffer[pPacket->cStart - 1];
#ifdef _FM_  
	if(g_hci.WaitForInitScript)
	{
#endif
    pEventPacket = (HCI_PACKET_COMMAND_COMPLETE_EVENT*)pBuffer;
    if ((pEventPacket->type == EVENT_PACKET) &&
        (pEventPacket->eventCode == COMMAND_COMPLETE_EVENT_CODE) &&
        (pEventPacket->commandOpcode == HCI_RESET))
        {
        // Run initialization script
			if( !StartScript(&g_hci) )
            {
				RETAILMSG(TRUE, (L" --- StartScript Failed For Reset ---\r\n"));
				goto cleanUp;
            }
#ifdef _FM_  
	
			g_hci.WaitForInitScript = FALSE;
			RETAILMSG(TRUE, (L" --- HCI Reset ---\r\n"));
        }
#endif
        }

    // Set packet type
    *peType = (HCI_TYPE)pBuffer[0];

#if DbgPacket
    RETAILMSG(TRUE, (
        L"HCI_ReadPacket  - type %02x size %3d start %3d end %d\r\n", 
        *peType, pPacket->cEnd - pPacket->cStart, pPacket->cStart, pPacket->cEnd 
    ));
    //DumpBuffer(pPacket->pBuffer + pPacket->cStart, BufferTotal(pPacket));
#endif

    // We are done
    rc = TRUE;

#if DbgPacket
    IFDBG(DebugOut(DEBUG_HCI_TRANSPORT, (L"-HCI_ReadPacket rc:%d\r\n", rc)));
#endif

cleanUp:
    return rc;
}

bool HCI_GetConnectStatus(void)
{
	return g_hci.opened;
}

#ifdef _FM_
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//FM Stack support
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//
//  Function:  ProcessReceivePacket
//
// This function called when BT and FM stacks are active. This function receives event packet 
//from BRF6300, it verifies if the packet is FM packet or BT packet and set it in the right stack's buffer. 
//
static VOID
ProcessReceiveEventPacket(
					   HCI_CONTEXT *pHCI
					   )
{
	UCHAR *pBuffer;
	DWORD  packetSize,count,bodySize,headerSize,Totalsize;
    HCI_PACKET_HEADER *pHeader;
	HCI_PACKET_COMMAND_COMPLETE_EVENT header;
	DWORD * pReadPacketSize, *pReadBufferSize;
	UCHAR ** pReadBuffer;
	HANDLE phReadBufferEvent,phReadPacketEvent; 
	BOOL FMPacket = FALSE;
	int rc = FALSE;
	
    // Start with zero packet length
    packetSize = 0;

	//Use a local buffer to read the packet header
	pBuffer = (UCHAR*)&header;
    pHeader = (HCI_PACKET_HEADER*)pBuffer;
    pHeader->type = EVENT_PACKET;

	headerSize = sizeof(pHeader->type) + sizeof(HCI_EVENT_PACKET_HEADER);

	++pBuffer;

    // Read the packet header
    if (!ReadData(pHCI, pBuffer, sizeof(HCI_EVENT_PACKET_HEADER)))
	{
		goto cleanUp;
	}
    
	//update the current packet size
	packetSize = headerSize;

	//check if the packet is FM event packet
	if(pHeader->event.code == HCE_FM_EVENT)
	{
		ASSERT(pHeader->event.length == 0);	
		FMPacket = TRUE;
		bodySize = 0;
	}

	pBuffer += sizeof(HCI_EVENT_PACKET_HEADER);
	bodySize = pHeader->event.length;

	//check if the event packet is  command complete event
	if(pHeader->event.code == COMMAND_COMPLETE_EVENT_CODE)
	{
		//read part of the command complete header and check if it is one of
		// the FM commands
		 if (!ReadData(pHCI, pBuffer, COMMAND_COMPLETE_PARTIAL_HEADER_LEN))
		 {
			 goto cleanUp;
		 }

		 packetSize +=COMMAND_COMPLETE_PARTIAL_HEADER_LEN;
		 bodySize -=COMMAND_COMPLETE_PARTIAL_HEADER_LEN  ;

		 //check if it is one of the FM command opcodes
		 if(header.commandOpcode == pHCI->lastFmCommandOpcode) 
		 {
			 FMPacket = TRUE;
		 }
	}
    
	if(FMPacket)
	{
		//The packet is FM packet.
		pReadBuffer = &pHCI->pFMReadBuffer;
		pReadBufferSize = &pHCI->FMreadBufferSize;
		pReadPacketSize = &pHCI->FMreadPacketSize;
		phReadBufferEvent = pHCI->hFMReadBufferEvent;
		phReadPacketEvent = pHCI->hFMReadPacketEvent;
	}
	else
	{
		//The packet is BT packet
		pReadBuffer = &pHCI->pReadBuffer;
		pReadBufferSize = &pHCI->readBufferSize;
		pReadPacketSize = &pHCI->readPacketSize;
		phReadBufferEvent = pHCI->hReadBufferEvent;
		phReadPacketEvent = pHCI->hReadPacketEvent;
		
	}

	//wait for a read request for the client stack
	WaitForSingleObject(phReadBufferEvent, INFINITE);
    
	// Check if packet fits to buffer
    Totalsize = packetSize + bodySize;

	if (Totalsize > *pReadBufferSize)
	{
		//The buffer is too small, throw the packet
        UCHAR trash[16];
		
        while (bodySize > 0)
		{
            count = bodySize > sizeof(trash) ? sizeof(trash) : bodySize;
            if (!ReadData(pHCI, trash, bodySize))
			{
				goto cleanUp;
			}
            bodySize -= count;
		}
        goto cleanUp;
	}

	//copy the packet's header to the stack's buffer
	memcpy(*pReadBuffer,(UCHAR*)&header,packetSize);

    // Read the packet body
    if (!ReadData(pHCI, (*pReadBuffer) + packetSize, bodySize))
	{
		goto cleanUp;
	}

	rc = TRUE;
cleanUp:
#if DBG_DUMPBUFFERS        
	DumpBuffer(*pReadBuffer, packetSize);
#endif
    // Signal received packet
    

	*pReadBuffer = NULL;
	*pReadBufferSize = 0;

	if (rc == TRUE)
	{
        *pReadPacketSize = Totalsize;
       	
	}
	else
	{
		*pReadPacketSize = 0;
	}
	SetEvent(phReadPacketEvent);

}


//------------------------------------------------------------------------------
//
//  Function:  ProcessFMReceiveEventPacket
//
// This function called when FM stack is the only active stack. This function receives event packet 
//from BRF6300. When packet doesn't fit to buffer packet is trashed.
//
static VOID
ProcessFMReceiveEventPacket(
					   HCI_CONTEXT *pHCI
					   )
{
    UCHAR *pBuffer;
    DWORD bufferSize, bodySize, packetSize;
    HCI_PACKET_HEADER *pHeader, header;
    DWORD count;
    BOOL ignorePacket = FALSE;
	BOOL rc = FALSE;
	
    // Start with zero packet length
    packetSize = 0;
	
    // We must wait for read buffer
    WaitForSingleObject(pHCI->hFMReadBufferEvent, INFINITE);
	
    // We get buffer for packet
    pBuffer = pHCI->pFMReadBuffer;
    bufferSize = pHCI->FMreadBufferSize;
	
    // Read packet header, use our header when buffer is too small
    if ((sizeof(pHeader->type) +sizeof(HCI_EVENT_PACKET_HEADER)) > bufferSize)
	{
        pBuffer = (UCHAR*)&header;
        bufferSize = sizeof(header);
        ignorePacket = TRUE;
	}
	
    // We already have HCI packet type
    pHeader = (HCI_PACKET_HEADER*)pBuffer;
    pHeader->type = EVENT_PACKET;
    pBuffer += sizeof(pHeader->type);
	
    // Read header
    if (!ReadData(pHCI, pBuffer, sizeof(HCI_EVENT_PACKET_HEADER)))
	{
		goto cleanUp;
	}
    pBuffer += sizeof(HCI_EVENT_PACKET_HEADER);
	bodySize = pHeader->event.length;
	
    //This is final packet size. Check if packet fits to buffer
    packetSize = sizeof(pHeader->type) + sizeof(HCI_EVENT_PACKET_HEADER) + bodySize;
    if (ignorePacket || packetSize > bufferSize)
	{
        UCHAR trash[16];
        while (bodySize > 0)
		{
            count = bodySize > sizeof(trash) ? sizeof(trash) : bodySize;
            if (!ReadData(pHCI, trash, bodySize)) goto cleanUp;
            bodySize -= count;
		}
        goto cleanUp;
	}
	
    // Read packet body
    if (!ReadData(pHCI, pBuffer, bodySize)) 
	{
		goto cleanUp;
	}
	
	rc = TRUE;
	
cleanUp:
#if DBG_DUMPBUFFERS        
	DumpBuffer(pBuffer, packetSize);
#endif
    // Signal received packet    
	pHCI->pFMReadBuffer = NULL;
    pHCI->FMreadBufferSize = 0;

	if (rc == TRUE)
	{
        pHCI->FMreadPacketSize = packetSize;
        
		
	}
	else
	{
		pHCI->FMreadPacketSize = 0;
	}
	SetEvent(pHCI->hFMReadPacketEvent);
}


//------------------------------------------------------------------------------
//			   
//  Function:  ReleaseFMReadBufferRequest
//
//  This function return zero packet if there is pending read request. It
//  is used from HCI_FMCloseConnection to release pending HCI_FMReadPacket.
//
static VOID
ReleaseFMReadBufferRequest(
    HCI_CONTEXT *pHCI
    )
{
    if (WaitForSingleObject(pHCI->hFMReadBufferEvent, 0) == WAIT_OBJECT_0)
	{
		pHCI->pFMReadBuffer = NULL;
        pHCI->FMreadBufferSize = 0;		
		pHCI->FMreadPacketSize = 0;
        SetEvent(pHCI->hFMReadPacketEvent);
	}
}


//------------------------------------------------------------------------------
//			   
//  Function:  FMReadPacket
//
//This function submit FM buffer to the recive thread inorder to fill it with a recived packet.
static DWORD
FMReadPacket(
    HCI_CONTEXT *pHCI,
    UCHAR *pBuffer, 
    DWORD size, 
    DWORD timeout
    )
{
    DWORD rc;

    // Submit packet to receive thread
    ASSERT(pHCI->pFMReadBuffer == NULL);
    pHCI->pFMReadBuffer = pBuffer;
    pHCI->FMreadBufferSize = size;
	//Signal to the recive thread that new buffer is ready
    if (!SetEvent(pHCI->hFMReadBufferEvent))
        {
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI!FMReadPacket: "
            L"SetEvent failed on hFMReadBufferEvent (0x%08x)\r\n",
            GetLastError()
            )));
        goto cleanUp;
        }

    // Wait for packet
    rc = WaitForSingleObject(pHCI->hFMReadPacketEvent, timeout);
    switch (rc)
	{
	case WAIT_FAILED:
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI!FMReadPacket: "
			L"WaitForSingleObject failed on hFMReadPacketEvent (0x%08x)\r\n",
			GetLastError()
			)));
#if 1
		RETAILMSG(TRUE, (L"HCI!FMReadPacket WAIT_FAILED\r\n"));
#endif
		size = 0;
		break;
	case WAIT_TIMEOUT:
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI!FMReadPacket: "
			L"WaitForSingleObject timeout on hFMReadPacketEvent\r\n"
			)));
#if 1
		RETAILMSG(TRUE, (L"HCI!FMReadPacket WAIT_TIMEOUT\r\n"));
#endif
		size = 0;
		break;
	case WAIT_OBJECT_0:
		// Get received packet size
		size = pHCI->FMreadPacketSize;
		break;
	}
	
    // Clear buffer from HCI context (just to catch errors)
    pHCI->pFMReadBuffer = NULL;
    pHCI->FMreadBufferSize = 0;
    pHCI->FMreadPacketSize = 0;
	
cleanUp:
    return size;
}

int DummyTransportCallback(HCI_EVENT eEvent, void *pEvent){return 0;}

//------------------------------------------------------------------------------
//			   
//  Function:  HCI_FMCloseConnection
//
//This function called when the FM stack close the FM connection.
//It releases the FMread buffres and calls to HCI_CloseConnection.
VOID
HCI_FMCloseConnection(
					  )
{
	IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"+HCI_FMCloseConnection\r\n")));
    RETAILMSG(TRUE, (L" +++ HCI_FMCloseConnection +++\r\n"));
	
	EnterCriticalSection(&g_hci.OpenCS);

	if(g_hci.FMOpened)
	{
		// Send request for buffer release, it will release HCI_FMReadPacket
		ReleaseFMReadBufferRequest(&g_hci);
		//call to HCI_CloseConnection to decrease the refernce count or to shutdown the chip
		g_hci.FMOpened = FALSE;
		HCI_CloseConnection();
	}

	if (g_hci.hFMReadBufferEvent != NULL)
	{
        CloseHandle(g_hci.hFMReadBufferEvent);
        g_hci.hFMReadBufferEvent = NULL;
	}
    if (g_hci.hFMReadPacketEvent != NULL)
	{
        CloseHandle(g_hci.hFMReadPacketEvent);
        g_hci.hFMReadPacketEvent = NULL;
	}

	RETAILMSG(TRUE, (L" --- HCI_FMCloseConnection ---\r\n"));
    IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"-HCI_FMCloseConnection\r\n")));

	if (g_hci.pfnCallback == DummyTransportCallback)
	{
		HCI_SetCallback(0);
	}

	 LeaveCriticalSection(&g_hci.OpenCS);
}


//------------------------------------------------------------------------------
//			   
//  Function:  HCI_FMOpenConnection
//
//This function called when the FM stack open the FM connection.
//
int
HCI_FMOpenConnection(
					 )
{
	int rc = FALSE;
	HCI_PACKET_HEADER ResetCommand;
	UCHAR ReadBuffer[257];
	BD_BUFFER Packet={sizeof ReadBuffer,4,sizeof ReadBuffer,0,1,ReadBuffer};
	HCI_TYPE eType;
	BOOL AfterOpenConn = FALSE;

	if (!g_hci.pfnCallback)
	{
		// We don't really use this callback, but HCI_SetCallback initializes
		// the BT debugger, so if HCI_SetCallback is not called, the DebugOut()
		// and DumpBuff() are null pointers.
		HCI_SetCallback(DummyTransportCallback);
	}

	IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"+HCI_FMOpenConnection\r\n")));
    RETAILMSG(TRUE, (L" +++ HCI_FMOpenConnection +++\r\n"));
	
	EnterCriticalSection(&g_hci.OpenCS);

	
	if(g_hci.FMOpened)
	{
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
            L"HCI_FMOpenConnection already open\r\n")));
		
		goto cleanUp;
	}

	//Clear the the read buffer
    g_hci.pFMReadBuffer = NULL;
    g_hci.FMreadBufferSize = 0;
    g_hci.FMreadPacketSize = 0;
	
	//Initialize the read events
	g_hci.hFMReadBufferEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_hci.hFMReadBufferEvent == NULL)
	{
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
            L"CreateEvent failed (GetLastError() = 0x%08x)\r\n",
            GetLastError()
            )));
        goto cleanUp;
	}
	
    g_hci.hFMReadPacketEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_hci.hFMReadPacketEvent == NULL)
	{
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
            L"CreateEvent failed (GetLastError() = 0x%08x)\r\n",
            GetLastError()
            )));
        goto cleanUp;
	}

	//Check if the chip is already intilized, if so we don't have to do any thing.
	if(g_hci.opened)
	{
		g_hci.OpenedRefCount++;
		g_hci.FMOpened = TRUE;
		rc = TRUE;
		goto cleanUp;
	}
	
	//Initilize the chip for the first time
	rc = HCI_OpenConnection();
	if(rc == FALSE)
	{
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
			L"Failed HCI_OpenConnection\r\n"
			)));
		goto cleanUp;
	}
	

	AfterOpenConn = TRUE;

	//Make and send a reset command in order to trigger the reset complete command and to send the 
	//BT Init script instead of the BT stack.
	ResetCommand.type = COMMAND_PACKET;
	ResetCommand.command.code = HCI_RESET;
	ResetCommand.command.length = 0;
	
	// send the reset packet...
	if (!WritePacket(
		&g_hci, (UCHAR*)&ResetCommand,
		sizeof(ResetCommand.command)+sizeof(ResetCommand.type), g_hci.driverTimeout
		))
	{
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
			L"WritePacket reset packet failed\r\n"
			)));		
		goto cleanUp;
	}

	//Send a read packet confirms that we succeed in sending the init Script

	if(!HCI_ReadPacket(&eType, &Packet) )
	{
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
			L"HCI_ReadPacket reset complete failed\r\n"
			)));
		goto cleanUp;
	}

	if(g_hci.WaitForInitScript)
	{
		// We failed to send the Init script
		IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMOpenConnection: "
			L"HCI_ReadPacket failed to send the Init script\r\n"
			)));

		goto cleanUp;
	}

	g_hci.FMOpened = TRUE;
	rc = TRUE;

cleanUp:
	LeaveCriticalSection(&g_hci.OpenCS);
    
	if (!rc)
	{
		if(AfterOpenConn)
		{
			// If the operation failed, but HCI_OpenConnection was successful,
			// then we set FMOpened to true, so that HCI_CloseConnection will
			// be called from within HCI_FMCloseConnection. Otherwise,
			// HCI_FMCloseConnection will perform all other cleanup.
			g_hci.FMOpened = TRUE;
		}

		HCI_FMCloseConnection();
	}
	RETAILMSG(TRUE, (L" --- HCI_FMOpenConnection ---\r\n"));
    IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"-HCI_FMOpenConnection(%d)\r\n", rc)));
    return rc;
}



//------------------------------------------------------------------------------
//			   
//  Function:  HCI_FMWritePacket
//
//This function called when the FM stack write packet to the chip.
//
int
HCI_FMWritePacket(
				  HCI_TYPE eType, 
				  BD_BUFFER *pPacket
				  )
{
    int rc = FALSE;
	
	IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"+HCI_FMWritePacket\r\n")));
    RETAILMSG(FALSE, (L" +++ HCI_FMWritePacket +++\r\n"));
	
	if (eType == COMMAND_PACKET)
	{
		HCI_COMMAND_PACKET_HEADER* pHCICmd =(HCI_COMMAND_PACKET_HEADER*)(pPacket->pBuffer + pPacket->cStart);
		g_hci.lastFmCommandOpcode = pHCICmd->code;
	}

	//call to HCI_WritePacket
	rc = HCI_WritePacket(eType,pPacket);
	
	RETAILMSG(FALSE, (L" --- HCI_FMWritePacket ---\r\n"));
    IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"-HCI_FMWritePacket(%d)\r\n", rc)));
	
	return rc;
}



//------------------------------------------------------------------------------
//			   
//  Function:  HCI_FMReadPacket
//
//This function called when the FM stack tries to read packet from the chip.
//
int
HCI_FMReadPacket(
				 HCI_TYPE *peType,
				 BD_BUFFER *pPacket
				 )
{
	int rc = FALSE;
	DWORD size;
	
	IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"+HCI_FMReadPacket\r\n")));
    RETAILMSG(FALSE, (L" +++ HCI_FMReadPacket +++\r\n"));
	
    if (!g_hci.FMOpened)
	{
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMReadPacket: "
            L"FM Stack is not active\r\n"
            )));
        goto cleanUp;
	}
	
	//Initialize the buffer size and the start offset
    pPacket->cStart = 3;
    pPacket->cEnd = pPacket->cSize + 3;
    if (BufferTotal(pPacket) < 257)
	{
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMReadPacket: "
            L"Buffer too small (%d bytes, should be 260)\r\n",
            BufferTotal(pPacket)
            )));
        goto cleanUp;
	}
	
#if DBG_DUMPBUFFERS
    RETAILMSG(TRUE, (
        L"HCI_FMReadPacket  - buffer size %3d start %3d end %d\r\n", 
        pPacket->cEnd - pPacket->cStart, pPacket->cStart, pPacket->cEnd 
		));
#endif
	
    // Read packet
    size = FMReadPacket(
        &g_hci, pPacket->pBuffer + pPacket->cStart,
        BufferTotal(pPacket), INFINITE
        );
	
    // If returned size is zero, something bad happen...
    if (size == 0)
	{
        IFDBG(RETAILMSG(DEBUG_ERROR,( L"ERROR: HCI_FMReadPacket: "
            L"ReadPacket return zero length packet\r\n"
            )));
        goto cleanUp;
	}
	
    // Fix packet start & end
    pPacket->cEnd = pPacket->cStart + size;
    pPacket->cStart++;
	
    // Set packet type
    *peType = (HCI_TYPE)pPacket->pBuffer[pPacket->cStart-1];
	
#if DBG_DUMPBUFFERS
    RETAILMSG(TRUE, (
        L"HCI_FMReadPacket  - type %02x size %3d start %3d end %d\r\n", 
        *peType, pPacket->cEnd - pPacket->cStart, pPacket->cStart, pPacket->cEnd 
		));
    DumpBuffer(pPacket->pBuffer + pPacket->cStart, BufferTotal(pPacket));
#endif
	
    // We are done
    rc = TRUE;
	
cleanUp:
	
	RETAILMSG(FALSE, (L" --- HCI_FMReadPacket ---\r\n"));
	IFDBG(RETAILMSG(DEBUG_HCI_INIT,( L"-HCI_FMReadPacket(%d)\r\n", rc)));

    return rc;
}
//-----------------------------------------------------------------------------
#endif
