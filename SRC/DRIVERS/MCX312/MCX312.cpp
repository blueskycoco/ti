#include "MCX312.h"
#define REGISTER_PAGE_SIZE              256
DWORD g_dwMCX312Base;
HANDLE g_gpioH;
//DWORD GPIO_DIR = 54;//144
DWORD GPIO_OE=57;//145
#define IOREAD(o)					((USHORT)*((volatile USHORT *)(g_dwMCX312Base + (o))))
#define IOWRITE(o, d)				*((volatile USHORT *)(g_dwMCX312Base + (o))) = (USHORT)(d)
#if 0
//#define IOREAD(o) INREG16(g_dwMCX312Base+o)
//#define IOWRITE(o,d)  SETREG16(g_dwMCX312Base+o, d);
void wreg1(int axis,int wdata)
{
	IOWRITE(wr0,(axis<<8)+0xf);
	IOWRITE(wr1,wdata);
}
void wreg2(int axis,int wdata)
{
	IOWRITE(wr0,(axis<<8)+0xf);
	IOWRITE(wr2,wdata);
}
void wreg3(int axis,int wdata)
{
	IOWRITE(wr0,(axis<<8)+0xf);
	IOWRITE(wr3,wdata);
}
void command(int axis,int cmd)
{
	IOWRITE(wr0,(axis<<8)+cmd);
}
void range(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x00);
}
void acac(int axis,int wdata)
{
	IOWRITE(wr6,wdata);
	IOWRITE(wr0,(axis<<8)+0x01);
}
void acc(int axis,int wdata)
{
	IOWRITE(wr6,wdata);
	IOWRITE(wr0,(axis<<8)+0x02);
}
void dec(int axis,int wdata)
{
	IOWRITE(wr6,wdata);
	IOWRITE(wr0,(axis<<8)+0x03);
}
void startv(int axis,int wdata)
{
	IOWRITE(wr6,wdata);
	IOWRITE(wr0,(axis<<8)+0x04);
}
void speed(int axis,int wdata)
{
	IOWRITE(wr6,wdata);
	IOWRITE(wr0,(axis<<8)+0x05);
}
void pulse(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x06);
}
void decp(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x07);
}
void center(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x08);
}
void lp(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x09);
}
void ep(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x0a);
}
void compp(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x0b);
}
void compm(int axis,long wdata)
{
	IOWRITE(wr7,(wdata>>16)&0xffff);
	IOWRITE(wr6,wdata&0xffff);
	IOWRITE(wr0,(axis<<8)+0x0c);
}
long readep(int axis)
{
	long a;long d6;long d7;
	IOWRITE(wr0,(axis<<8)+0x11);
	GPIOClrBit(g_gpioH,GPIO_DIR);
	d6=IOREAD(rr6);
	d7=IOREAD(rr7);
	GPIOSetBit(g_gpioH,GPIO_DIR);
	return a;
}
void wait(int axis)
{
	GPIOClrBit(g_gpioH,GPIO_DIR);
	while(IOREAD(rr0)&axis);
	GPIOSetBit(g_gpioH,GPIO_DIR);
	RETAILMSG(1,(TEXT("wait\r\n")));
}
void next_wait()
{
	GPIOClrBit(g_gpioH,GPIO_DIR);
	while((IOREAD(rr0)&0x0200)==0);
	GPIOSetBit(g_gpioH,GPIO_DIR);
	RETAILMSG(1,(TEXT("next_wait\r\n")));
}
void bp_wait()
{
	GPIOClrBit(g_gpioH,GPIO_DIR);
	while((IOREAD(rr0)&0x6000)==0x6000);
	GPIOSetBit(g_gpioH,GPIO_DIR);
	RETAILMSG(1,(TEXT("bp_wait\r\n")));
}
void homesrch()
{
	DWORD tmp,i;
	//GPIOClrBit(g_gpioH,GPIO_OE);
	//GPIOClrBit(g_gpioH,GPIO_DIR);
	for(i=0;i<10;i++)
	{
	Sleep(10);
	tmp=IOREAD(rr0);
	RETAILMSG(1,(TEXT("0 RR0 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr1);
	RETAILMSG(1,(TEXT("0 RR1 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr2);
	RETAILMSG(1,(TEXT("0 RR2 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr3);
	RETAILMSG(1,(TEXT("0 RR3 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr4);
	RETAILMSG(1,(TEXT("0 RR4 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr5);
	RETAILMSG(1,(TEXT("0 RR5 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr6);
	RETAILMSG(1,(TEXT("0 RR6 is %x \r\n"),tmp));
	Sleep(10);
	tmp=IOREAD(rr7);
	RETAILMSG(1,(TEXT("0 RR7 is %x \r\n"),tmp));
		}
	GPIOSetBit(g_gpioH,GPIO_DIR);
	//Sleep(10);
	wreg1(0xf,0x0008);
	speed(0xf,2000);
	GPIOClrBit(g_gpioH,GPIO_DIR);
	//Sleep(10);
	tmp=IOREAD(rr4);
	RETAILMSG(1,(TEXT("1 RR4 is %x \r\n"),tmp));
	if((tmp&0x2)==0x2)
		{
			GPIOSetBit(g_gpioH,GPIO_DIR);
			command(0x1,0x23);			
		}
	GPIOClrBit(g_gpioH,GPIO_DIR);
	//Sleep(10);
	tmp=IOREAD(rr4);
	RETAILMSG(1,(TEXT("2 RR4 is %x \r\n"),tmp));
	if((tmp&0x200)==0x200)
		{
			GPIOSetBit(g_gpioH,GPIO_DIR);
			command(0x2,0x23);
		}
	//wait(0x3);	
	wreg1(0x3,0x00c);
	speed(0x3,50);
	command(0x3,0x22);
	//wait(0x3);
	wreg1(0x3,0x0000);
	speed(0x3,4000);
	pulse(0x3,100);
	command(0x3,0x21);
	//wait(0x3);
	lp(0x3,0);
	wreg2(0x3,0x0003);
	compp(0x1,100000);
	compm(0x1,-1000);
	compp(0x2,50000);
	compm(0x2,-500);
}
#endif
BOOL Virtual_Alloc()
{
   int i=0;
   BOOL bResult=FALSE;
   DWORD tmp;
   g_dwMCX312Base = (DWORD)VirtualAlloc((PVOID)0, 
                                                           (DWORD)REGISTER_PAGE_SIZE, 
                                                           (DWORD)MEM_RESERVE, 
                                                           (DWORD)PAGE_NOACCESS);
    if ((PVOID)(g_dwMCX312Base)==NULL)
    {
        RETAILMSG(0,(L"VirtualAlloc failed.\r\n"));
        return FALSE;
    }
    else {
        RETAILMSG(0,(L"VirtualAlloc at 0x%x.\r\n", g_dwMCX312Base));   
    }

    RETAILMSG(0,(L"ulIoBaseAddress = 0x%x.\r\n", g_dwMCX312Base));
    bResult= VirtualCopy((PVOID)g_dwMCX312Base,
                         (PVOID)(0x11000000 >> 8),
                         (DWORD)REGISTER_PAGE_SIZE,
                         (DWORD)(PAGE_READWRITE|PAGE_NOCACHE|PAGE_PHYSICAL));
    if (bResult == TRUE)
    {
   	 RETAILMSG(0,(L"ulIoBaseAddress = 0x%x.\r\n", g_dwMCX312Base));
	g_gpioH = GPIOOpen();
    	 //GPIOSetMode(g_gpioH,GPIO_DIR,GPIO_DIR_OUTPUT);
	 //GPIOSetBit(g_gpioH,GPIO_DIR);
	 GPIOSetMode(g_gpioH,GPIO_OE,GPIO_DIR_OUTPUT);    	 
        GPIOClrBit(g_gpioH,GPIO_OE);
	 //while(1);
	 //Sleep(10);
	 //while(1)
	#if 0
	 IOWRITE(wr0, 0x8000);
	 Sleep(20);
	 //command(0x3,0xf);
	 IOWRITE(wr1,0x0000);
	 IOWRITE(wr2,0x0000);
	 IOWRITE(wr3,0x0000);
	 IOWRITE(wr4,0x0000);
	 IOWRITE(wr5,0x0024);
	 //accofst(0x3,0);
	 range(0x3,800000);
	 acac(0x3,1010);
	 acc(0x3,100);
	 dec(0x3,100);
	 startv(0x3,100);
	 speed(0x3,4000);
	 pulse(0x3,100000);
	 lp(0x3,0);
	#endif
	//GPIOClrBit(g_gpioH,GPIO_DIR);
	//Sleep(10);
	/*while(1)
	{
		tmp=IOREAD(rr0);
		RETAILMSG(1,(TEXT("rr0 is %x \r\n"),tmp));
		tmp=IOREAD(rr1);
		RETAILMSG(1,(TEXT("rr1 is %x \r\n"),tmp));

		tmp=IOREAD(rr2);
		RETAILMSG(1,(TEXT("rr2 is %x \r\n"),tmp));
		tmp=IOREAD(rr3);
		RETAILMSG(1,(TEXT("rr3 is %x \r\n"),tmp));
		tmp=IOREAD(rr4);
		RETAILMSG(1,(TEXT("rr4 is %x \r\n"),tmp));
		tmp=IOREAD(rr5);
		RETAILMSG(1,(TEXT("rr5 is %x \r\n"),tmp));
		tmp=IOREAD(rr6);
		RETAILMSG(1,(TEXT("rr6 is %x \r\n"),tmp));
		tmp=IOREAD(rr7);
		RETAILMSG(1,(TEXT("rr7 is %x \r\n"),tmp));

		Sleep(1000);
	}*/
	// homesrch();
	 
	 return TRUE;
    }
    else
    {
   	 RETAILMSG(1,(L"Copy failed\r\n"));
        if (g_dwMCX312Base)
        {
            bResult = VirtualFree((PVOID)g_dwMCX312Base, 0UL, (DWORD)MEM_RELEASE);
            g_dwMCX312Base = 0UL;
        }
        return FALSE;
    }

}
void Virtual_Realease()
{
	//if(s2440IOP)
		//VirtualFree((PVOID)s2440IOP,sizeof(IOPreg), MEM_RELEASE);
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

BOOL MCX_Deinit(DWORD hDeviceContext)
{
	return TRUE;
} 
#if 0
DWORD UserKeyProcessThread(void)
{
	USHORT tmp = 0;
	while(1)
	{
		GPIOClrBit(g_gpioH,GPIO_DIR);
		tmp=IOREAD(rr0);
		RETAILMSG(1,(TEXT("rr0 is %x \r\n"),tmp));
		tmp=IOREAD(rr1);
		RETAILMSG(1,(TEXT("rr1 is %x \r\n"),tmp));
		tmp=IOREAD(rr2);
		RETAILMSG(1,(TEXT("rr2 is %x \r\n"),tmp));
		tmp=IOREAD(rr3);
		RETAILMSG(1,(TEXT("rr3 is %x \r\n"),tmp));
		tmp=IOREAD(rr4);
		RETAILMSG(1,(TEXT("rr4 is %x \r\n"),tmp));
		tmp=IOREAD(rr5);
		RETAILMSG(1,(TEXT("rr5 is %x \r\n"),tmp));
		tmp=IOREAD(rr6);
		RETAILMSG(1,(TEXT("rr6 is %x \r\n"),tmp));
		tmp=IOREAD(rr7);
		RETAILMSG(1,(TEXT("rr7 is %x \r\n"),tmp));
	 	GPIOSetBit(g_gpioH,GPIO_DIR);

		Sleep(5000);

		}
	return 0;
}
#endif
DWORD MCX_Init(DWORD dwContext)
{
	RETAILMSG(1,(TEXT("[	MCX312++]\r\n")));
	Virtual_Alloc();
	/*CreateThread(NULL,
                         0,
                         (LPTHREAD_START_ROUTINE)UserKeyProcessThread,
                         0,
                         0,
                         NULL);*/
	RETAILMSG(1,(TEXT("[	MCX312--]\r\n")));
	return TRUE;
}
//-----------------------------------------------------------------------------
BOOL MCX_IOControl(DWORD hOpenContext, 
				   DWORD dwCode, 
				   PBYTE pBufIn, 
				   DWORD dwLenIn, 
				   PBYTE pBufOut, 
				   DWORD dwLenOut, 
				   PDWORD pdwActualOut)
{	
	switch(dwCode)
	{
	case IO_CTL_START_BUTTON://GPG11
		if(pBufOut && dwLenOut>=sizeof(int))
		{
			int outValue;
			//outValue = (s2440IOP->rGPGDAT & (1<<11))?1:0;
			*(int*)pBufOut = outValue;
			if(pdwActualOut) *(int*)pdwActualOut=sizeof(int);
		}
		break;
	case IOCTL_MCX312_WRITE:
		//GPIOSetBit(g_gpioH,GPIO_DIR);
		IOWRITE(*(BYTE*)pBufIn,(*(BYTE*)(pBufOut+1)<<8)|*(BYTE*)(pBufOut));		
		//RETAILMSG(1,(TEXT("Write %x=%x\r\n"),*(BYTE*)pBufIn,(*(BYTE*)(pBufOut+1)<<8)|*(BYTE*)(pBufOut)));
		//Sleep(10);	
		break;
	case IOCTL_MCX312_READ:
		if(pBufOut && dwLenOut>=sizeof(USHORT))
		{
			USHORT outValue;
			//GPIOClrBit(g_gpioH,GPIO_DIR);
			*(USHORT*)pBufOut = IOREAD(*(BYTE*)pBufIn);
			//Sleep(10);
			if(pdwActualOut) *(int*)pdwActualOut=sizeof(USHORT);
		}

		break;
	case IO_CTL_BEEP_SET:		
		//EnableBeep(*(int*)pBufIn);
		break;
	default:
		break;		
	}
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD MCX_Open(DWORD hDeviceContext, DWORD AccessCode, DWORD ShareMode)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_Open\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
BOOL MCX_Close(DWORD hOpenContext)
{
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MCX_PowerDown(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_PowerDown\r\n")));
	
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void MCX_PowerUp(DWORD hDeviceContext)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_PowerUp\r\n")));

} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD MCX_Read(DWORD hOpenContext, LPVOID pBuffer, DWORD Count)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_Read\r\n")));
	return TRUE;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD MCX_Seek(DWORD hOpenContext, long Amount, DWORD Type)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_Seek\r\n")));
	return 0;
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
DWORD MCX_Write(DWORD hOpenContext, LPCVOID pSourceBytes, DWORD NumberOfBytes)
{
	//RETAILMSG(1,(TEXT("USERMUL: MCX_Write\r\n")));
	return 0;
}

