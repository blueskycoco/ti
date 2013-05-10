#include "FM24CL64.h"
#include "ceddkex.h"
#include "sdk_gpio.h"
#include "gpio_ioctls.h"
DEVICE_IFC_GPIO ifc;
HANDLE h;
HANDLE    m_hI2C;
HANDLE    m_hIntEvent;
HANDLE m_xin1_event;
INT   m_dwSysIntr_144;
#define FM24CL64_I2C_DEVICE_ADDR 0x50
#if 0
DWORD WINAPI GpioThread()
{
    // Loop until we are not stopped...
    for(;;)
    {   // Wait for event
        WaitForSingleObject(m_hIntEvent, INFINITE);
		SetEvent( m_xin1_event );
		RETAILMSG(1,(TEXT("Got Irq 144\n")));
		ifc.pfnInterruptDone(h,144,m_dwSysIntr_144);
    }
}
#endif
BOOL Virtual_Alloc()
{
    m_hI2C = I2COpen(SOCGetI2CDeviceByBus(3)); //nmcca: using I2C3_MEMBASE make sense doesnt return a good value... so nothing is opened. Using hardcoded value 3 for now     
    I2CSetSlaveAddress(m_hI2C,  FM24CL64_I2C_DEVICE_ADDR); //nmcca: there is nothing to check to see if the handle is null...
    I2CSetSubAddressMode(m_hI2C, I2C_SUBADDRESS_MODE_0);  
#if 0
	//create three thread to handle gpio144,145,146 interrupt
	DWORD pdwSDIOIrq;
	DWORD         threadID;
	m_xin1_event=CreateEvent(NULL,TRUE,FALSE,L"XIN1_EVENT");
	h = CreateFile(L"GIO1:",GENERIC_READ | GENERIC_WRITE,FILE_SHARE_READ | FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
	if(h!=NULL)
	{
		if (!DeviceIoControl(
			h, IOCTL_DDK_GET_DRIVER_IFC, (VOID*)&DEVICE_IFC_GPIO_GUIDDEVICE_IFC_GPIO_GUID,
			sizeof(DEVICE_IFC_GPIO_GUID), &ifc, sizeof(DEVICE_IFC_GPIO),
			NULL, NULL
		)) {
			CloseHandle(h);
			return FALSE;
		}
		
		dwGpioIrq=ifc.pfnGetSystemIrq(h,144);
		ifc.pfnSetMode(h,144,GPIO_DIR_INPUT | GPIO_INT_HIGH_LOW);

    if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR, &dwGpioIrq, sizeof(DWORD), &m_dwSysIntr_144, sizeof(DWORD), NULL))
    {
        CloseHandle(h);
		return FALSE;
    }

    // allocate the interrupt event for the SDIO/controller interrupt
    m_hIntEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    if ( !InterruptInitialize( m_dwSysIntr_144, m_hIntEvent, NULL, 0 ) )
    {
        CloseHandle(h);
		return FALSE;
    }
	CreateThread(NULL,
        0,
        GpioThread,
        this,
        0,
        &threadID);

	}
	#endif
    return TRUE;
}
void Virtual_Realease()
{
}


BOOL WINAPI  
DllEntry(HANDLE	hinstDLL, 
		 DWORD dwReason, 
		 LPVOID /* lpvReserved */)
{
	switch(dwReason)
	{
	case DLL_PROCESS_ATTACH:
		DEBUGREGISTER((HINSTANCE)hinstDLL);
		return TRUE;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
#ifdef UNDER_CE
	case DLL_PROCESS_EXITING:
		break;
	case DLL_SYSTEM_STARTED:
		break;
#endif
	}
	return TRUE;
}

BOOL FMC_Deinit(DWORD hDeviceContext)
{
	return TRUE;
} 
DWORD FMC_Init(DWORD dwContext)
{
	RETAILMSG(1,(TEXT("[	FM24CL64++]\r\n")));
	Virtual_Alloc();
	RETAILMSG(1,(TEXT("[	FM24CL64--]\r\n")));
	return TRUE;
}
BOOL WriteReg(UINT8* value,UINT8 size)
{
    BOOL rc = FALSE;
    if (m_hI2C)
    {
        DWORD len = I2CWrite(m_hI2C, 0, value, sizeof(UINT8)*size);
        if ( len != sizeof(UINT8)*size)
            ERRORMSG(ZONE_ERROR,(TEXT("Write FM24CL64 Failed!!\r\n")));
            else
               rc = TRUE;
        }
    return rc;
}
BOOL ReadReg(UINT8* data,UINT8 size)
{

    BOOL rc = FALSE;
    if (m_hI2C)
    {
        DWORD len = I2CRead(m_hI2C, 0,data, sizeof(UINT8)*size);
        if ( len != sizeof(UINT8)*size)
            ERRORMSG(ZONE_ERROR,(TEXT("Read FM24CL64 Failed!!\r\n")));
            else
               rc = TRUE;
        }
    return rc;

}

//-----------------------------------------------------------------------------
BOOL FMC_IOControl(DWORD hOpenContext, 
				   DWORD dwCode, 
				   PBYTE pBufIn, 
				   DWORD dwLenIn, 
				   PBYTE pBufOut, 
				   DWORD dwLenOut, 
				   PDWORD pdwActualOut)
{	
	int i;
	switch(dwCode)
	{
	case IOCTL_FM24CL64_WRITE:
		//GPIOSetBit(g_gpioH,GPIO_DIR);
		//IOWRITE(*(BYTE*)pBufIn,(*(BYTE*)(pBufOut+1)<<8)|*(BYTE*)(pBufOut));	
		WriteReg((BYTE *)pBufOut,dwLenOut);
		RETAILMSG(0,(TEXT("Write =\r\n")));
		for(i=0;i<dwLenOut;i++)
		RETAILMSG(0,(TEXT("%x\r\n"),*(BYTE*)(pBufOut+i)));
		break;
	case IOCTL_FM24CL64_READ:
		if(pBufOut && dwLenOut>=sizeof(BYTE))
		{
			//USHORT outValue;
			//GPIOClrBit(g_gpioH,GPIO_DIR);
			//*(USHORT*)pBufOut = IOREAD(*(BYTE*)pBufIn);
			RETAILMSG(0,(TEXT("Read =\r\n")));
			ReadReg((BYTE*)pBufOut,dwLenOut);
			//RETAILMSG(1,(TEXT("Read %x="),*(BYTE*)pBufIn));
			for(i=0;i<dwLenOut;i++)
			RETAILMSG(0,(TEXT("%x\r\n"),*(BYTE*)(pBufOut+i)));
			if(pdwActualOut) *(int*)pdwActualOut=sizeof(BYTE)*dwLenOut;
		}

		break;
	default:
		break;		
	}
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD FMC_Open(DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_Open\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL FMC_Close(DWORD hOpenContext)
{
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void FMC_PowerDown(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_PowerDown\r\n")));
	
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void FMC_PowerUp(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_PowerUp\r\n")));

} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD FMC_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_Read\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD FMC_Seek(DWORD hOpenContext, long Amount, DWORD Type)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_Seek\r\n")));
	return 0;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD FMC_Write(DWORD hOpenContext, LPCVOID pSourceBytes, DWORD NumberOfBytes)
{
	//RETAILMSG(1,(TEXT("USERMUL: FMC_Write\r\n")));
	return 0;
}

