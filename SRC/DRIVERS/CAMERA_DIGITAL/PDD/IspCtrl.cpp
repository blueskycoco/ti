// Portions Copyright (c) 2009 BSQUARE Corporation. All rights reserved.
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
// rqs:
//	1.ISPInit() provides clocks,power,and initiates isp.~CIspCtrl() does reverse
//	
//
//------------------------------------------------------------------------------
//

#include <windows.h>
#include <pm.h>
#include <omap3530.h>
//#include <omap3430.h>

#include <ceddk.h>
#include <ceddkex.h>

#include "dbgsettings.h"
#include "Cs.h"
#include "csmedia.h"
#include "CameraPDDProps.h"
#include "dstruct.h"
#include "cam_pdd.h"
#include "CameraDriver.h"

#include "oal_io.h"
#include "wchar.h"
#include "util.h"
#include "ispctrl.h"
#include "module.h"
#include <initguid.h>

#include "sdk_gpio.h"
#include <triton.h>
#include <twl.h>
#include "tps659xx_internals.h"
#include "omap_bus.h"

//internal variable
OMAP_GPIO_REGS 			*m_pISPPWDN;				//PWDN PIN
OMAP_GPIO_REGS 			*m_pISPRESET;				//RESET PIN
OMAP_ISP_RSZ_REGS   	*m_pCAMRSZ;



//------------------------------------------------------------------------------
//  Variable define
#define GPIO_MODE_MASK_EVENPIN  0xfffffff8
#define GPIO_MODE_MASK_ODDPIN   0xfff8ffff
#define RSZ_ENABLE	1		//ftm changed to 0				//resizer enable or disable

//	#ifdef DEBUGMSG
//	#undef DEBUGMSG
//	#define DEBUGMSG RETAILMSG
//	#endif
//	
//	#ifdef ZONE_FUNCTION
//	#undef ZONE_FUNCTION
//	#define ZONE_FUNCTION 0
//	#endif

/* Private structures and defines */
static BYTE LSCGainTable[] = {
#include "ISPCONFIG/rDrvCcdcLscGainTable.txt"
};
static ULONG CFACoeffTable[] = {
  #include "ISPCONFIG/rDrvPrevCfaCoefTable.txt"
};

static ULONG RedGammaTable[] = {
  #include "ISPCONFIG/rDrvPrevRedGammaTable.txt"
};

static ULONG GreenGammaTable[] = {
  #include "ISPCONFIG/rDrvPrevGreenGammaTable.txt"
};

static ULONG BlueGammaTable[] = {
  #include "ISPCONFIG/rDrvPrevBlueGammaTable.txt"
};

static ULONG NoiseFilter[] = {
  #include "ISPCONFIG/rDrvPrevNoiseFilterTable.txt"
};

static ULONG LumaEnhanceTable[] = {
  #include "ISPCONFIG/rDrvPrevLumaEnhanceTable.txt"
};
#if 1
static UINT16 Rsz4TapModeHorzFilterTable[32] = {
  #include "ISPCONFIG/rDrvRsz4TapHorzTable.txt" 
};

static UINT16 Rsz4TapModeVertFilterTable[32] = {
  #include "ISPCONFIG/rDrvRsz4TapVertTable.txt" 
};

static UINT16 Rsz7TapModeHorzFilterTable[28] = {
  #include "ISPCONFIG/rDrvRsz7TapHorzTable.txt" 
};

static UINT16 Rsz7TapModeVertFilterTable[28] = {
  #include "ISPCONFIG/rDrvRsz7TapVertTable.txt" 
};
#else
static UINT16 Rsz4TapModeHorzFilterTable[32] = {

		0x0027, 0x00B2, 0x00B2, 0x0027,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
		0x0027, 0x00B2, 0x0027, 0x00B2,
	};

static UINT16 Rsz4TapModeVertFilterTable[32] = {
  0x0000, 0x0100, 0x0000, 0x0000,
		0x03FA, 0x00F6, 0x0010, 0x0000,
		0x03F9, 0x00DB, 0x002C, 0x0000,
		0x03FB, 0x00B3, 0x0053, 0x03FF,
		0x03FD, 0x0082, 0x0084, 0x03FD,
		0x03FF, 0x0053, 0x00B3, 0x03FB,
		0x0000, 0x002C, 0x00DB, 0x03F9,
		0x0000, 0x0010, 0x00F6, 0x03FA
	};

static UINT16 Rsz7TapModeHorzFilterTable[28] = {
  0x0004, 0x0023, 0x0023, 0x005A,
		0x005A, 0x0058, 0x0058, 0x0004,
		0x0023, 0x0023, 0x005A, 0x005A,
		0x0058, 0x0058, 0x0004, 0x0023,
		0x0023, 0x005A, 0x005A, 0x0058,
		0x0058, 0x0004, 0x0023, 0x0023,
		0x005A, 0x005A, 0x0058, 0x0058
	};

static UINT16 Rsz7TapModeVertFilterTable[28] = {
		0x0004, 0x0023, 0x005A, 0x0058,
		0x0023, 0x0004, 0x0000, 0x0002,
		0x0018, 0x004d, 0x0060, 0x0031,
		0x0008, 0x0000, 0x0001, 0x000f,
		0x003f, 0x0062, 0x003f, 0x000f,
		0x0001, 0x0000, 0x0008, 0x0031,
		0x0060, 0x004d, 0x0018, 0x0002
	};
#endif
/*
 * Resizer Constants
 */
#define MAX_IN_WIDTH_MEMORY_MODE		4095

#define MAX_IN_WIDTH_ONTHEFLY_MODE		1280
#define MAX_IN_WIDTH_ONTHEFLY_MODE_ES2	4095
#define MAX_IN_HEIGHT					4095
#define MINIMUM_RESIZE_VALUE			64
#define MAXIMUM_RESIZE_VALUE			1024
#define MID_RESIZE_VALUE				512

#define MAX_7TAP_HRSZ_OUTWIDTH			1280
#define MAX_7TAP_VRSZ_OUTWIDTH			640

#define MAX_7TAP_HRSZ_OUTWIDTH_ES2		3300
#define MAX_7TAP_VRSZ_OUTWIDTH_ES2		1650

#define DEFAULTSTPIXEL					0
#define DEFAULTSTPHASE					1
#define DEFAULTHSTPIXEL4TAPMODE			3
#define FOURPHASE						4
#define EIGHTPHASE						8
#define RESIZECONSTANT					256
#define SHIFTER4TAPMODE					0
#define SHIFTER7TAPMODE					1
#define DEFAULTOFFSET					7
#define OFFSETVERT4TAPMODE				4
#define OPWDALIGNCONSTANT				0xFFFFFFF0



//can't clear about:include OMAP_GPIO_REGS's header file in .h,it creates errors.
//so places here
//Just be convienient
void CAM_GpioSet(OMAP_GPIO_REGS* pGpio,BYTE pBit,BYTE Value)
{
	CLRREG32(&pGpio->OE, (1<<(pBit%32)));	
	if(Value)												//high
	{
		SETREG32(&pGpio->DATAOUT, (1<<(pBit%32)));
	}
	else
	{
		CLRREG32(&pGpio->DATAOUT, (1<<(pBit%32)));
	}	
}

CIspCtrl::CIspCtrl()
{   
    m_pIspConfigRegs	= NULL;
    m_pCCDCRegs			= NULL;
    m_pYUVDMAAddr		= NULL;
    m_pYUVVirtualAddr	= NULL;
    m_pYUVPhysicalAddr	= NULL;
	m_sensorState		= NULL;
	hTWL				= NULL;
	m_hRootBus			= NULL;	
}


CIspCtrl::~CIspCtrl()
{
	UINT32 setting = 0;
	DWORD dwClock = OMAP_DEVICE_CAMERA;
	CE_BUS_DEVICE_SOURCE_CLOCKS clockInfo;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ISPInit\r\n")));

	clockInfo.devId    = dwClock;
	clockInfo.count    = 1;
	clockInfo.rgSourceClocks[0] = kDPLL4_CLKOUT_M5X2;
	
	if(m_sensorState != 0)
		DisableCamera();
	//release clock
	DeviceIoControl(m_hRootBus,IOCTL_BUS_RELEASE_CLOCK,&clockInfo,
						sizeof(CE_BUS_DEVICE_SOURCE_CLOCKS),
						NULL,
						0,
						NULL,
						NULL);
	
	if (m_hGPIO)
	{
		GPIOClose(m_hGPIO);
		m_hGPIO = NULL;
	}
	UnMapCameraReg();

	// Free camera buffer
    DeAllocBuffer();
	
	ModuleDeinit();	
	DEBUGMSG(ZONE_FUNCTION,(TEXT("~CIspCtrl\r\n")));
}

//-----------------------------------------------------------------------------
//
//  Function:       GetPhysFromVirt
//
//  Maps the Virtual address passed to a physical address.
//
//  returns a physical address with a page boundary size.
//
LPVOID
CIspCtrl::GetPhysFromVirt(
    ULONG ulVirtAddr
    )
{
    ULONG aPFNTab[1];
    ULONG ulPhysAddr;

    if (LockPages((LPVOID)ulVirtAddr, UserKInfo[KINX_PAGESIZE], aPFNTab,
                   LOCKFLAG_QUERY_ONLY))
        {
         // Merge PFN with address offset to get physical address         
         ulPhysAddr= ((*aPFNTab << UserKInfo[KINX_PFN_SHIFT]) & UserKInfo[KINX_PFN_MASK])|(ulVirtAddr & 0xFFF);
        } else {
        ulPhysAddr = 0;
        }
    return ((LPVOID)ulPhysAddr);
}
//------------------------------------------------------------------------------
//
//  Function:  MapCameraReg
//
//  Read data from register
//      
BOOL CIspCtrl::MapCameraReg()
{
    PHYSICAL_ADDRESS pa;
    UINT32 *pAddress = NULL;

	DEBUGMSG(ZONE_FUNCTION,(TEXT("+MapCameraReg\r\n")));

	m_hRootBus = CreateFile((L"BUS1:"), 0, 0, NULL, 0, 0, 0 );
    if(m_hRootBus == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed open Bus1!\r\n")));
		RETAILMSG(1, (TEXT("wangchong:Failed open Bus1!!!\r\n")));
        return FALSE;
    }
        
    //To map Camera ISP register
    pa.QuadPart = CAM_ISP_CONFIG_BASE_ADDRESS;
    m_pIspConfigRegs = (CAM_ISP_CONFIG_REGS *)MmMapIoSpace(pa, sizeof(CAM_ISP_CONFIG_REGS), FALSE);
    if (m_pIspConfigRegs == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera ISP CONFIG physical address to virtual address!\r\n")));
        return FALSE;
    }
    
    //To map Camera ISP CCDC register
    pa.QuadPart = CAM_ISP_CCDC_BASE_ADDRESS;
    m_pCCDCRegs = (CAM_ISP_CCDC_REGS *)MmMapIoSpace(pa, sizeof(CAM_ISP_CCDC_REGS), FALSE);
    if (m_pCCDCRegs == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera ISP CCDC physical address to virtual address!\r\n")));
        return FALSE;
    }
	//To map Camera ISP RESIZER register
    pa.QuadPart = 0x480BD000;
    m_pCAMRSZ = (OMAP_ISP_RSZ_REGS *)MmMapIoSpace(pa, sizeof(OMAP_ISP_RSZ_REGS), FALSE);
    if (m_pCAMRSZ == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera ISP RESIZER physical address to virtual address!\r\n")));
        return FALSE;
    }
	
	//Reset GPIO126
	pa.QuadPart = OMAP_GPIO4_REGS_PA;
    m_pISPRESET = (OMAP_GPIO_REGS *)MmMapIoSpace(pa, sizeof(OMAP_GPIO_REGS), FALSE);
    if (m_pISPRESET == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera interface physical address to virtual address!\r\n")));
        return FALSE;
    }
	//PWDN(GPIO167)
	pa.QuadPart = OMAP_GPIO6_REGS_PA;
    m_pISPPWDN = (OMAP_GPIO_REGS *)MmMapIoSpace(pa, sizeof(OMAP_GPIO_REGS), FALSE);
    if (m_pISPPWDN == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera interface physical address to virtual address!\r\n")));
        return FALSE;
    }
    DEBUGMSG(ZONE_FUNCTION,(TEXT("-MapCameraReg\r\n")));
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  UnMapCameraReg
//
//  Read data from register
//      
void CIspCtrl::UnMapCameraReg()
{
	if(m_hRootBus)
	{
		CloseHandle(m_hRootBus);
		m_hRootBus = NULL;
	}
    if(m_pIspConfigRegs)
    {
    	MmUnmapIoSpace((PVOID)m_pIspConfigRegs,sizeof(CAM_ISP_CONFIG_REGS));
		m_pIspConfigRegs = NULL;
    }
    if(m_pCCDCRegs)
    {
    	MmUnmapIoSpace((PVOID)m_pCCDCRegs,sizeof(CAM_ISP_CCDC_REGS));
		m_pCCDCRegs = NULL;
    }
	if(m_pCAMRSZ)
    {
    	MmUnmapIoSpace((PVOID)m_pCAMRSZ,sizeof(OMAP_ISP_RSZ_REGS));
		m_pCCDCRegs = NULL;
    }
	if(m_pISPRESET)
    {
    	MmUnmapIoSpace((PVOID)m_pISPRESET,sizeof(OMAP_GPIO_REGS));
		m_pISPRESET = NULL;
    }
	if(m_pISPPWDN)
    {
    	MmUnmapIoSpace((PVOID)m_pISPPWDN,sizeof(OMAP_GPIO_REGS));
		m_pISPPWDN = NULL;
    }
}

//------------------------------------------------------------------------------
//
//  Function:  ConfigGPIO4MDC
//
//  Set GPIO58  mode4 output  0->1 (reset)
//  Set GPIO134 mode4 output  0 
//  Set GPIO136 mode4 output  1 
//  Set GPIO54  mode4 output  1         
//  Set GPIO139 mode4 input 
//  PWDN 167
//  RESET 126
BOOL CIspCtrl::ConfigGPIO4MDC()
{
    DEBUGMSG(ZONE_FUNCTION,(TEXT("+ConfigGPIO4MDC\r\n")));
    UINT32 *pAddress = NULL;

    // GPIO setup is handled by XLDR/EBOOT
	PHYSICAL_ADDRESS pa;
    OMAP_SYSC_PADCONFS_REGS   *pConfig;

    // map in pad control registers for camera GPIO pins
    pa.QuadPart = OMAP_SYSC_PADCONFS_REGS_PA;
    pConfig = (OMAP_SYSC_PADCONFS_REGS *)MmMapIoSpace(pa, sizeof(OMAP_SYSC_PADCONFS_REGS), FALSE);
    if (pConfig == NULL)
    {
        ERRORMSG(ZONE_ERROR,(TEXT("Failed map Camera interface physical address to virtual address!\r\n")));
        return FALSE;
    }
	#define PADCONF_PIN_NOT_USED         		(OFF_ENABLE | OFFOUT_DISABLE | OFFPULLUD_ENABLE | \
                                         		INPUT_ENABLE | PULL_DOWN | MUX_MODE_7)	
	/*CAMERA*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_HS , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));  		/*CAM_HS*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_VS , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_VS*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_XCLKA, (INPUT_DISABLE | PULL_INACTIVE | MUX_MODE_0));   	/*CAM_XCLKA*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_PCLK, (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));     	/*CAM_PCLK*/
//    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_FLD,	PADCONF_PIN_NOT_USED);       						/*CAM_FLD*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_FLD, (INPUT_ENABLE | PULL_UP | MUX_MODE_0));       		/*CAM_FLD*/
	OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D0 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D0*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D1 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D1*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D2 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D2*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D3 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D3*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D4 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D4*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D5 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D5*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D6 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D6*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D7 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D7*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D8 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D8*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D9 , (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D9*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D10, (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D10*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CAM_D11, (INPUT_ENABLE | PULL_INACTIVE | MUX_MODE_0));       /*CAM_D11*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_XCLKB, PADCONF_PIN_NOT_USED);    						/*CAM_XCLKB*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_WEN, (INPUT_DISABLE | PULL_INACTIVE | MUX_MODE_4));      /*CAM_WEN PWDN*/
    OUTREG16(&pConfig->CONTROL_PADCONF_CAM_STROBE,(INPUT_DISABLE | PULL_INACTIVE | MUX_MODE_4));   	/*CAM_STROBE CAM_REST*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CSI2_DX0, PADCONF_PIN_NOT_USED);   							/*CSI2_DX0 (not used)*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CSI2_DY0, PADCONF_PIN_NOT_USED);   							/*CSI2_DY0 (not used)*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CSI2_DX1, PADCONF_PIN_NOT_USED);   							/*CSI2_DX1 (not used)*/
    //OUTREG16(&pConfig->CONTROL_PADCONF_CSI2_DY1, PADCONF_PIN_NOT_USED);   							/*CSI2_DX1 (not used)*/
	return TRUE;
}       

//------------------------------------------------------------------------------
//
//  Function:  CCDCInitSYNC
//
//  Init. ISPCCDC_SYN_MODE register 
//
/*
Smart Sensor:
We can use pipeline:
Sensor==>CCDC==>Resizer==>Memory or Sensor==>CCDC==>Memory

1  For YUV data, CCDC_SYN_MODE [13:12] INPMODE = 1 or 2 && CCDC_REC656IF [0] 
REC656ON = 1   
2  For YUV data, DCSubtract should be disabled, by setting CCDC_DCSUB [0:13] to zero 
3  For YUV data, black-level compensation must be disabled by setting the black-
compensation register values CCDC_BLKCMP register, fields R_YE, GR_CY, GB_G, 
B_MG to zero 
4  For YUV data, the faulty-pixel correction operation is not applicable and must be 
disabled/bypassed:  CCDC_FPC [15] FPCEN = 0 
5  For YUV data, the data formatter, lens-shading compensation, and video-port interface must 
be bypassed: CCDC_FMTCFG [15] VPEN = 0x0 and CCDC_SYN_MODE [18] VP2SDR = 
0x0 
6  For YUV data, the Low Pass Filter (LPF) must be disabled: CCDC_SYN_MODE [14] LPF = 
0x0 
7  For YUV data, A-Law compression should not be used: CCDC_ALAW [3] CCDTBL = 0 
8  To send the CCDC output to the resizer module: CCDC_SYN_MODE [19] SDR2RSZ =1 
9 Enable CCDC: CCDC_PCR[0] ENABLE = 0x1 

*/
//USE: Sensor==>CCDC==>Memory first to test image
//THEN Sensor==>CCDC==>Resizer==>Memory,for future use.
BOOL CIspCtrl::CCDCInitSYNC()
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+CCDCInitSYNC\r\n")));
	UINT32 syn_mode = 0 ;	
#if RSZ_ENABLE
	syn_mode |= ISPCCDC_SYN_MODE_SDR2RSZ;			// Video data to resizer 
#else
	syn_mode |= ISPCCDC_SYN_MODE_WEN;				// Video data to memory 
	syn_mode |= ISPCCDC_SYN_MODE_VDHDEN;			// Enable timing generator
#endif        
	syn_mode |= ISPCCDC_SYN_MODE_DATSIZ_10;
	syn_mode |= ISPCCDC_SYN_MODE_INPMOD_YCBCR16;	//Set input mode:Interlanced, YCbCr 8

	//syn_mode |=ISPCCDC_SYN_MODE_PACK8; //pack 8-bit in memory
	//ISP_OutReg32(&m_pCCDCRegs->CCDC_REC656IF, ISPCCDC_REC656IF_R656ON); //enable BT656

	
	ISP_OutReg32(&m_pCCDCRegs->CCDC_SYN_MODE, syn_mode); 
	return TRUE;                
}       
//------------------------------------------------------------------------------
//
//  Function:  CCDCInitCFG
//
//  Init. Camera CCDC   
//
BOOL CIspCtrl::CCDCInitCFG()
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+CCDCInitCFG\r\n")));
	// Request Init.
	UINT32 setting = 0 ;
	ISP_InReg32(&m_pCCDCRegs->CCDC_CFG, &setting);
	setting |= (ISPCCDC_CFG_VDLC 					//Enalbe Latching,OMAP Must Set!!!
			| ISPCCDC_CFG_Y8POS						//Even or odd
			);	
	//maybe need Y8POS
	ISP_OutReg32(&m_pCCDCRegs->CCDC_CFG, setting);
	// This is sensor specific BLC value for Omnivision 3640
	// OUTREG32(&m_pCCDCRegs->CCDC_DCSUB, 0x10);//turn light low
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-CCDCInitCFG\r\n")));
	return TRUE;        
}       
//------------------------------------------------------------------------------
//
//  Function:  ConfigOutlineOffset
//
//  Configures the output line offset when stored in memory.
//  Configures the num of even and odd line fields in case of rearranging
//  the lines
//  offset: twice the Output width and aligned on 32byte boundary.
//  oddeven: odd/even line pattern to be chosen to store the output
//  numlines: Configure the value 0-3 for +1-4lines, 4-7 for -1-4lines
//
BOOL CIspCtrl::ConfigOutlineOffset(UINT32 offset, UINT8 oddeven, UINT8 numlines)
{       
     DEBUGMSG(ZONE_FUNCTION, (TEXT("+ConfigOutlineOffset\r\n")));
     UINT32 setting = 0;    
     
    // Make sure offset is multiple of 32bytes. ie last 5bits should be zero 
    setting = offset & ISP_32B_BOUNDARY_OFFSET;
    ISP_OutReg32(&m_pCCDCRegs->CCDC_HSIZE_OFF, setting);

    // By default Donot inverse the field identification 
    ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
    setting &= (~ISPCCDC_SDOFST_FINV);
    ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);

    // By default one line offset
    ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
    setting &= ISPCCDC_SDOFST_FOFST_1L;
    ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);

    switch (oddeven) {
    case EVENEVEN:      /*even lines even fields*/
        ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
        setting |= ((numlines & 0x7) << ISPCCDC_SDOFST_LOFST0_SHIFT);
        ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);
        break;
    case ODDEVEN:       /*odd lines even fields*/
        ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
        setting |= ((numlines & 0x7) << ISPCCDC_SDOFST_LOFST1_SHIFT);
        ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);
        break;
    case EVENODD:       /*even lines odd fields*/
        ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
        setting |= ((numlines & 0x7) << ISPCCDC_SDOFST_LOFST2_SHIFT);
        ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);
        break;
    case ODDODD:        /*odd lines odd fields*/
        ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
        setting |= ((numlines & 0x7) << ISPCCDC_SDOFST_LOFST3_SHIFT);
        ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, setting);
        break;
    default:
        break;
    }
    return TRUE;
}       
//------------------------------------------------------------------------------
//
//  Function:  CCDCSetOutputAddress
//
//  Configures the memory address where the output should be stored.
//
BOOL CIspCtrl::CCDCSetOutputAddress(ULONG SDA_Address)
{       
        //DEBUGMSG(ISP_MSG_FLAG, (TEXT("+TVPReadReg(SlaveAddress0x81: 0x%x)\r\n"), reg_value));
                
        ULONG addr = (SDA_Address) ;
		
        addr = addr & ISP_32B_BOUNDARY_BUF;
        ISP_OutReg32(&m_pCCDCRegs->CCDC_SDR_ADDR, addr);		
        return TRUE;            
}   
//------------------------------------------------------------------------------
//
//  Function:  CCDCEnable
//
//  Enables the CCDC module.
//
BOOL CIspCtrl::CCDCEnable(BOOL bEnable)
{       
        DEBUGMSG(ZONE_FUNCTION, (TEXT("+CCDCEnable\r\n"))); 
        BOOL rc = FALSE;
        UINT32 setting = 0 ;
        
        ISP_InReg32(&m_pCCDCRegs->CCDC_PCR, &setting);  
        if (bEnable)
            setting |= (ISPCCDC_PCR_EN);
        else
            setting &= ~(ISPCCDC_PCR_EN);
            
        rc = ISP_OutReg32(&m_pCCDCRegs->CCDC_PCR, setting);
        return rc;          
}   

//------------------------------------------------------------------------------
//
//  Function:  AllocBuffer
//
//  AllocBuffer for video input and format transfer out 
//
BOOL CIspCtrl::AllocBuffer()
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+AllocBuffer\r\n")));
    if(m_pYUVDMAAddr)
        return TRUE;
    DWORD dwSize;								// = IMAGE_CAMBUFF_SIZE;
    PHYSICAL_ADDRESS   pCamBufferPhys; 
    DMA_ADAPTER_OBJECT camBuffer;	
	//dwSize = 2048*1536*2; //according to the max,prevent failure!!!!!!
	dwSize = 2592*1944*4; 
    camBuffer.ObjectSize = sizeof(camBuffer);
    camBuffer.InterfaceType = Internal;
    camBuffer.BusNumber = 0;        
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+AllocBuffer:0x%x\r\n"),dwSize));
    m_pYUVDMAAddr = (PBYTE)HalAllocateCommonBuffer(&camBuffer, dwSize, &pCamBufferPhys, FALSE );
    
    if (m_pYUVDMAAddr == NULL)
    {   
        ERRORMSG(ZONE_ERROR, (TEXT("HalAllocateCommonBuffer failed !!!\r\n")));
        return FALSE;
    }
	m_pYUVVirtualAddr = (PBYTE)VirtualAlloc(NULL,dwSize, MEM_RESERVE,PAGE_NOACCESS);
	if (m_pYUVVirtualAddr == NULL)
	{
		ERRORMSG(ZONE_ERROR, (TEXT("Sensor buffer memory alloc failed !!!\r\n")));
		return FALSE;
	}
	VirtualCopy(m_pYUVVirtualAddr, (VOID *) (pCamBufferPhys.LowPart >> 8), dwSize, PAGE_READWRITE | PAGE_PHYSICAL | PAGE_NOCACHE  );
	m_pYUVPhysicalAddr = GetPhysFromVirt((ULONG)m_pYUVVirtualAddr);
	if(!m_pYUVPhysicalAddr)
	{
		ERRORMSG(ZONE_ERROR,(_T("GetPhysFromVirt 0x%08X failed: \r\n"), m_pYUVVirtualAddr));
		return FALSE;
	}
	DEBUGMSG(MASK_DMA, (TEXT("m_pYUVVirtualAddr=0x%x\r\n"),m_pYUVVirtualAddr));
	DEBUGMSG(MASK_DMA, (TEXT("m_pYUVPhysicalAddr=0x%x\r\n"),m_pYUVPhysicalAddr));
	DEBUGMSG(MASK_DMA, (TEXT("m_pYUVDMAAddr=0x%x\r\n"),m_pYUVDMAAddr));
	memset(m_pYUVVirtualAddr,0x00,dwSize); 		//clear the frame buffer
	return TRUE;      
}   
//------------------------------------------------------------------------------
//
//  Function:  DeAllocBuffer
//
//  DeAllocBuffer for video input and format transfer out   
//
BOOL CIspCtrl::DeAllocBuffer()
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+DeAllocBuffer\r\n")));
    //Do not free memory to prevent the memory fragment.
  
    if(m_pYUVDMAAddr == NULL)
        return TRUE;
    if(!VirtualFree( m_pYUVDMAAddr, 0, MEM_RELEASE ))
        DEBUGMSG(1,(_T("CIspCtrl::DeAllocBuffer failed \r\n")));
        
    m_pYUVVirtualAddr	= NULL;
    m_pYUVPhysicalAddr	= NULL;
    m_pYUVDMAAddr		= NULL;
	return TRUE;    
}


//------------------------------------------------------------------------------
//
//  Function:  CCDCConfigSize
//
//
// Configures CCDC HORZ/VERT_INFO registers to decide the start line
// stored in memory.
//
// output_w : output width from the CCDC in number of pixels per line
// output_h : output height for the CCDC in number of lines
//
//
BOOL CIspCtrl::CCDCConfigSize(ULONG width,ULONG height)
{

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+ISPConfigSize\r\n")));
	
	//ConfigOutlineOffset(UINT32 offset, UINT8 oddeven, UINT8 numlines);
	/* Set the HSIZE_OFFSET register */
    ISP_OutReg32(&m_pCCDCRegs->CCDC_HSIZE_OFF, (width*2)&ISP_32B_BOUNDARY_OFFSET);
	/* Set the SDOFST register */
    ISP_OutReg32(&m_pCCDCRegs->CCDC_SDOFST, 0);
	/* Set the horizontal info */
	ISP_OutReg32(&m_pCCDCRegs->CCDC_HORZ_INFO, (0 << ISPCCDC_HORZ_INFO_SPH_SHIFT)|((width-1) << 0));
	ISP_OutReg32(&m_pCCDCRegs->CCDC_VERT_START, 0 << ISPCCDC_VERT_START_SLV0_SHIFT);       
    /* Set the vertical lines */
    ISP_OutReg32(&m_pCCDCRegs->CCDC_VERT_LINES, (height-1) << ISPCCDC_VERT_LINES_NLV_SHIFT);

//	ISP_OutReg32(&m_pCCDCRegs->CCDC_VDINT, height-1);
	ISP_OutReg32(&m_pCCDCRegs->CCDC_VDINT, ((height-1)<<ISPCCDC_VDINT_0_SHIFT));
   
 //   ConfigOutlineOffset(width*2, 0, 0x582);  
    return TRUE;        
}

//------------------------------------------------------------------------------
//
//  Function:  CCDCInit
//
//  Init. Camera CCDC
//
//
BOOL CIspCtrl::CCDCInit(ULONG width,ULONG height)
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+CCDCInit\r\n")));
	CCDCInitCFG();
	CCDCInitSYNC();
	CCDCConfigSize(width,height);// Init. video size   
	//Set CCDC_SDR address
	CCDCSetOutputAddress((ULONG) m_pYUVPhysicalAddr);
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-CCDCInit\r\n")));
	return TRUE;        
} 
// LTC3555的使能 En1(GPIO159), En2(GPIO158), En3(GPIO156)

BOOL
CIspCtrl::SensorPowerUpSequence()
{
    BOOL bRet = TRUE;
	UINT32 setting = 0;	 
	UINT16 val = 0;
	UINT8  val_temp = 0;

	/* provide 2.8V power */
	val = (UINT16)((TWL_PROCESSOR_GRP1 << 13) | (TWL_VAUX2_RES_ID << 4) | TWL_RES_ACTIVE);
	val_temp = (UINT8)(val >> 8);
    TWLWriteRegs(hTWL, TWL_PB_WORD_MSB, &val_temp,1);
	val_temp = (UINT8)val;
    TWLWriteRegs(hTWL, TWL_PB_WORD_LSB, &val_temp,1);

    /* Put sensor in reset state */
    GPIOClrBit(m_hGPIO, m_dwCamGpioReset);

    /* Put sensor out of pwr down PwrDown = 1, */
 //   GPIOClrBit(m_hGPIO, m_dwCamGpioShutdown);
    GPIOClrBit(m_hGPIO, m_dwCamGpioPwrDown);

    /* Enable the input clk to the sensor */
    //Enable Clock Output,216/9=24MHz
	ISP_InReg32(&m_pIspConfigRegs->TCTRL_CTRL, &setting);
	setting &= 0xffffffe0;
	setting |= (9<<0);
	ISP_OutReg32(&m_pIspConfigRegs->TCTRL_CTRL, setting);
    Sleep(1);
    /* Put sensor out of reset state */
    GPIOSetBit(m_hGPIO, m_dwCamGpioReset);

    return bRet;
}

BOOL
CIspCtrl::SensorPowerDownSequence()
{
	BOOL bRet = TRUE;
	UINT32 setting = 0;	
	UINT16 val = 0;
	UINT8  val_temp = 0;
    /* Disable the input clk to the sensor */
    //Disable Clock Output,216/9=24MHz
	ISP_InReg32(&m_pIspConfigRegs->TCTRL_CTRL, &setting);
	setting &= 0xffffffe0;	
	ISP_OutReg32(&m_pIspConfigRegs->TCTRL_CTRL, setting);

	GPIOClrBit(m_hGPIO, m_dwCamGpioPwrDown);
    /* Put sensor in reset state */
    GPIOClrBit(m_hGPIO, m_dwCamGpioReset);
	/* provide 2.8V power */
	val = (UINT16)((TWL_PROCESSOR_GRP1 << 13) | (TWL_VAUX2_RES_ID << 4) | TWL_RES_OFF);
    val_temp = (UINT8)(val >> 8);
    TWLWriteRegs(hTWL, TWL_PB_WORD_MSB, &val_temp,1);
	val_temp = (UINT8)val;
    TWLWriteRegs(hTWL, TWL_PB_WORD_LSB, &val_temp,1);
	return bRet;
}

//------------------------------------------------------------------------------
//
//  Function:  ISPInit
//
//  Init. Camera ISP
//
BOOL CIspCtrl::ISPInit()
{
	DWORD dwClock = OMAP_DEVICE_CAMERA;
	CE_BUS_DEVICE_SOURCE_CLOCKS clockInfo;
	UINT32 setting = 0;

	DEBUGMSG(ZONE_FUNCTION, (TEXT("+ISPInit\r\n")));
	RETAILMSG(1, (TEXT("wangchong:+ISPInit\r\n")));

	clockInfo.devId    = dwClock;
	clockInfo.count    = 1;
	clockInfo.rgSourceClocks[0] = kDPLL4_CLKOUT_M5X2;
	RETAILMSG(1, (TEXT("wangchong:m_hRootBus = 0x%x\r\n"), m_hRootBus));
	RETAILMSG(1, (TEXT("wangchong:devId = 0x%x\r\n"), clockInfo.devId));
	RETAILMSG(1, (TEXT("wangchong:SourceClocks = 0x%x\r\n"), clockInfo.rgSourceClocks[0])); 
	// Call bus driver
	if(!DeviceIoControl(m_hRootBus,
						IOCTL_BUS_SOURCE_CLOCKS,
						&clockInfo,
						sizeof(CE_BUS_DEVICE_SOURCE_CLOCKS),
						NULL,
						0,
						NULL,
						NULL))
	{
		RETAILMSG(1, (TEXT("wangchong:DeviceIoControl 1 error\r\n")));
//		return FALSE;	// wangchong 修改过，需要还原	
	}

	// PrcmDeviceEnableClocks(OMAP_DEVICE_CAMERA, TRUE);
	// MCLK Divide 216*4=864
	// PrcmClockSetDivisor(kCAM_MCLK,kDPLL4_CLKOUT_M5X2,4);

	if(!DeviceIoControl(
						m_hRootBus, 
						IOCTL_BUS_REQUEST_CLOCK, 
						&dwClock, 
						sizeof(dwClock),
						NULL, 0, NULL, NULL ))
	{
		RETAILMSG(1, (TEXT("wangchong:DeviceIoControl 2 error\r\n")));
//		return FALSE;	// wangchong，需要还原
	}

	/* Initialize the GPIO Interface */
	m_hGPIO = GPIOOpen();

	if (m_hGPIO == NULL)
	{
		DEBUGMSG(ZONE_ERROR,(L"GPIO Handle open failed\r\n"));
		return FALSE;
	}

	m_dwCamGpioReset 	= 126;
	m_dwCamGpioPwrDown 	= 167;
	/* Set the direction for GPIO pins */
	GPIOSetMode(m_hGPIO, m_dwCamGpioReset, GPIO_DIR_OUTPUT );
	GPIOSetMode(m_hGPIO, m_dwCamGpioPwrDown, GPIO_DIR_OUTPUT );

	// Enable interface and functional clock of Camera's 
    // PrcmDeviceEnableClocks(OMAP_DEVICE_CAMERA, TRUE); do it in oal layer.
    // MCLK Divide 216*4=864
    // PrcmClockSetDivisor(kCAM_MCLK,kDPLL4_CLKOUT_M5X2,4);
    // Init ISP power capability:Disable Auto Idle,No StandBy
    ISP_InReg32(&m_pIspConfigRegs->SYSCONFIG, &setting);
	setting &= ~ISP_SYSCONFIG_AUTOIDLE;// Disable auto idle for subsystem
	setting |= (ISP_SYSCONFIG_MIdleMode_NoStandBy << ISP_SYSCONFIG_MIdleMode_SHIFT);// No standby
	ISP_OutReg32(&m_pIspConfigRegs->SYSCONFIG, setting);

	// Disable all interrupts and clear interrupt status
	setting = 0;
	ISP_OutReg32(&m_pIspConfigRegs->IRQ0ENABLE, setting);
	ISP_OutReg32(&m_pIspConfigRegs->IRQ1ENABLE, setting);
	ISP_InReg32(&m_pIspConfigRegs->IRQ0STATUS, &setting);
	ISP_OutReg32(&m_pIspConfigRegs->IRQ0STATUS, setting);
	ISP_InReg32(&m_pIspConfigRegs->IRQ1STATUS, &setting);
	ISP_OutReg32(&m_pIspConfigRegs->IRQ1STATUS, setting);

	DEBUGMSG(ZONE_FUNCTION, (TEXT("-ISPInit\r\n")));
	return TRUE;        
}

BOOL CIspCtrl::ISPInterruptEnable(BOOL bEnable)
{
	UINT32 setting = 0;
	if(bEnable)
	{
		/* Clear any pending interrupt status */
		ISP_InReg32(&m_pIspConfigRegs->IRQ0STATUS, &setting);		
		ISP_OutReg32(&m_pIspConfigRegs->IRQ0STATUS, setting);		 
		// Enable IRQ0,just resizer done interrupt
#if RSZ_ENABLE		
		setting = IRQ0STATUS_RSZ_DONE_IRQ;
#else
		setting = IRQ0STATUS_CCDC_VD0_IRQ;
#endif
		ISP_OutReg32(&m_pIspConfigRegs->IRQ0ENABLE, setting);	 
	}
	else
	{	
		// Disable using module clock
		RETAILMSG(1,(TEXT("#wangwj#-----setting pclk pol------------\r\n")));
		setting &= ~(ISPCTRL_CCDC_CLK_EN | ISPCTRL_CCDC_RAM_EN |ISPCTRL_SBL_WR0_RAM_EN |ISPCTRL_SBL_WR1_RAM_EN | ISPCTRL_SBL_RD_RAM_EN | ISPCTRL_RSZ_CLK_EN| ISPCTRL_PAR_CLK_POL_INV);	//wangwj_cam			   
		ISP_OutReg32(&m_pIspConfigRegs->CTRL, setting);
				
		// Disable IRQ0
		setting = 0;
		ISP_OutReg32(&m_pIspConfigRegs->IRQ0ENABLE, setting);  
	}
	return TRUE;
}



//------------------------------------------------------------------------------
//
//  Function:  ISPEnable
//
//  Reset and enable ISP need component
//
BOOL CIspCtrl::ISPEnable(BOOL bEnable)
{
      
      DEBUGMSG(ZONE_FUNCTION, (TEXT("+ISPEnable\r\n")));
        UINT32 setting = 0;
        ULONG ulTimeout = 50;       
		//First,reset ISP module,so CCDC reset too.
        if (bEnable == TRUE)
        {
            ISP_InReg32(&m_pIspConfigRegs->SYSCONFIG, &setting);    
            setting |= (ISP_SYSCONFIG_SOFTRESET);   
            ISP_OutReg32(&m_pIspConfigRegs->SYSCONFIG, setting);
             
             
            // Wait till the isp wakes out of reset 
            ISP_InReg32(&m_pIspConfigRegs->SYSSTATUS, &setting);
            setting &= 0x1;

            while ((setting != 0x1) && ulTimeout--)
            {     
                DEBUGMSG(ZONE_VERBOSE, (TEXT("+ISPInit: reset not completed,ulTimeout=%d\r\n"),ulTimeout));
                Sleep(10);// Reset not completed
                ISP_InReg32(&m_pIspConfigRegs->SYSSTATUS, &setting);
                setting &= (0x1);
            }
			setting = 0;
			ISP_InReg32(&m_pIspConfigRegs->SYSCONFIG, &setting);
        	setting |= (ISP_SYSCONFIG_MIdleMode_NoStandBy << ISP_SYSCONFIG_MIdleMode_SHIFT);// No standby  
        	ISP_OutReg32(&m_pIspConfigRegs->SYSCONFIG, setting);
            DEBUGMSG(ZONE_VERBOSE, (TEXT("+ISPEnable: soft reset completed: setting=%d, ulTimeout=%d\r\n"),setting,ulTimeout));
        }    
        setting = 0;		
	//Second,configue ISP:CCDC and RESIZER
    if (bEnable == TRUE)
    {
        // Enable using module clock and disable SBL_AUTOIDLE, PAR BRIDGE
        setting |= (//	ISPCTRL_CCDC_FLUSH											//CCDC memory flush
					//	| ISPCTRL_CCDC_WEN_POL 									//CCDC WEN Polarity,not used
        				 ISPCTRL_CCDC_CLK_EN 										//Enable CCDC Clock
        				| ISPCTRL_CCDC_RAM_EN 										//Use CCDC Ram
#if RSZ_ENABLE        				
        				| ISPCTRL_RSZ_CLK_EN
#endif
        			//	| (ISPCTRL_SYNC_DETECT_VSFALL << ISPCTRL_SYNC_DETECT_SHIFT)
        			//	| ISPCTRL_PAR_CLK_POL_INV
						| (ISPCTRL_SYNC_DETECT_VSRISE << ISPCTRL_SYNC_DETECT_SHIFT) //Falling edge,just want to detect shnchro signals
        				| (0 << ISPCTRL_SHIFT_SHIFT) 								//Data lane shiter No shift 
        				| ISPCTRL_CCDC_FLUSH										//CCDC memory flush
#if RSZ_ENABLE        				
                    	| ISPCTRL_SBL_WR0_RAM_EN 									//SBL module WRITE0 RAM enable
    //                	| ISPCTRL_SBL_WR1_RAM_EN 									//Used by all module
#else
						| ISPCTRL_SBL_WR1_RAM_EN 									//Used by all module
#endif
                    //	| ISPCTRL_SBL_RD_RAM_EN  									//SBL module READ RAM enable
                 	 	| (ISPCTRL_PAR_BRIDGE_BENDIAN)								//8 to 16 bridge disable
                   	//	| (ISPCTRL_PAR_BRIDGE_DISABLE)						//wangwj_cam
                    	| (ISPCTRL_PAR_SER_CLK_SEL_parallel));						//parallel interface	


		setting |=ISPCTRL_SHIFT_2;//wangwj_cam
                    																
     		ISP_OutReg32(&m_pIspConfigRegs->CTRL, setting);
    }
     	DEBUGMSG(ZONE_FUNCTION, (TEXT("-ISPEnable\r\n")));
        return TRUE;        
}


//------------------------------------------------------------------------------
//
//  Function:  IsCCDCBusy
//
//  To check CCDC busy bit
//
BOOL CIspCtrl::IsCCDCBusy()
{
        DEBUGMSG(ZONE_FUNCTION, (TEXT("+IsCCDCBusy\r\n")));
                
        UINT32 setting = 0;
        
        ISP_InReg32(&m_pCCDCRegs->CCDC_PCR, &setting); 
        setting &= ISPCCDC_PCR_BUSY;

        if (setting)
            return TRUE;    
        else
            return false;                   
}
//------------------------------------------------------------------------------
//
//  Function:  Check_IRQ0STATUS
//
//  Dump ISP_IRQ0STATUS register and check CCDC_VD0_IRQ bit
//
BOOL CIspCtrl::Check_IRQ0STATUS()
{
        DEBUGMSG(ZONE_FUNCTION, (TEXT("+IsCCDCBusy\r\n")));
                
        UINT32 setting = 0;
        UINT32 check_bit = 0;

        ISP_InReg32(&m_pIspConfigRegs->IRQ0STATUS, &setting);

        DEBUGMSG(ZONE_VERBOSE, (TEXT("+ISP_IRQ0STATUS:0x%x\r\n"),setting)); 
        check_bit = setting & IRQ0STATUS_CCDC_VD0_IRQ;

        if (check_bit)
        {
            CCDCEnable(false);    
            return TRUE;
        }       
        else
            return FALSE;                   
}
        
//------------------------------------------------------------------------------
//
//  Function:  InitializeCamera
//
//  To initialize Camera .
//
BOOL CIspCtrl::InitializeCamera(void)
{    
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+InitializeCamera\r\n")));    
	RETAILMSG(1,(_T("wangchong:CIspCtrl::InitializeCamera\r\n")));
	UINT16 val = 0;
	UINT8  val_temp = 0;
	RETAILMSG(1,(TEXT("#wangwj#-----InitializeCamera----------------------111------\r\n")));
    if (ConfigGPIO4MDC() == FALSE)
    {   DEBUGMSG(ZONE_ERROR, (TEXT("GPIO Init. failed \r\n")));     
        return FALSE;
    }
    MapCameraReg(); //Map camera registers
    ISPInit();		// Init. ISP
    hTWL = TWLOpen();
    if ( hTWL == NULL )
    {
    	UnMapCameraReg();
    	return FALSE;
    }
	RETAILMSG(1,(TEXT("#wangwj#-----InitializeCamera----------------------222------\r\n")));
	//Off 2.8V
	val = (UINT16)((TWL_PROCESSOR_GRP1 << 13) | (TWL_VAUX2_RES_ID << 4) | TWL_RES_OFF);
	val_temp = (UINT8)(val >> 8);
    TWLWriteRegs(hTWL, TWL_PB_WORD_MSB, &val_temp,1);
	val_temp = (UINT8)val;
    TWLWriteRegs(hTWL, TWL_PB_WORD_LSB, &val_temp,1);	
	//sensor

//RETAILMSG(1,(TEXT("#wangwj#-----ModuleInit--------\r\n")));
	
	if(ModuleInit() == FALSE)
	{
		TWLClose(hTWL);
		UnMapCameraReg();
		return FALSE;
	}
	//ModuleDetect();
	
//RETAILMSG(1,(TEXT("#wangwj#-----ModuleDetect--------\r\n")));

	RETAILMSG(1,(TEXT("#wangwj#-----InitializeCamera----------------------333------\r\n")));
	if(AllocBuffer() == FALSE)
	{
		TWLClose(hTWL);
		UnMapCameraReg();
		ModuleDeinit();
		return FALSE;
	}
	RETAILMSG(1,(TEXT("#wangwj#-----InitializeCamera----------------------444------\r\n")));
    DEBUGMSG(ZONE_FUNCTION, (TEXT("-InitializeCamera\r\n")));
	RETAILMSG(1, (TEXT("-InitializeCamera\r\n")));
    return TRUE;
}




//------------------------------------------------------------------------------
//
//  Function:  EnableCamera
//
//  To enable Camera.
//
//	Modify:Input mode.....
//	Need to change Picture size according to Mode Type
BOOL CIspCtrl::EnableCamera(ULONG lModeType,ULONG width,ULONG height)
{    
		UINT32 setting = 0;
		RSZParams_t  rszParams;
		MODULE_DESCRIPTOR ModuleDesc;		
    	DEBUGMSG(ZONE_FUNCTION, (TEXT("+EnableCamera:%d\r\n"),lModeType)); 
		RETAILMSG(1, (TEXT("wangchong:+EnableCamera:%d\r\n"),lModeType)); 
		//Open sensor
		if(m_sensorState == 0)
		{
			RETAILMSG(1, (TEXT("wangchong:SensorPowerUpSequence\r\n"))); 
			SensorPowerUpSequence();
			RETAILMSG(1, (TEXT("wangchong:ModuleOpen\r\n"))); 
			ModuleOpen();
		}
		RETAILMSG(1, (TEXT("wangchong:ModuleSetImage\r\n"))); 
		//Set sensor pic format
		if(ModuleSetImage(lModeType) == FALSE)
			return FALSE;		
		RETAILMSG(1, (TEXT("wangchong:ModuleGetFormat\r\n"))); 
		//Get sensor pic format
		ModuleGetFormat(ModuleDesc);//must later
		RETAILMSG(1, (TEXT("wangchong:ISPEnable\r\n"))); 
		//Enable ISP hardware
        ISPEnable(TRUE);// Soft reset ISP and clock on used module 

		
		ModuleDetect(); // ...wangchong add 2010-8-24

		
		// Resizer settings
		memset(&rszParams,0x00,sizeof(RSZParams_t));
		rszParams.ulInputImageHeight 	= ModuleDesc.SourceVSize;
		rszParams.ulInputImageWidth  	= ModuleDesc.SourceHSize;
		rszParams.height 				= ModuleDesc.SourceVSize;
		rszParams.width 				= ModuleDesc.SourceHSize;
		rszParams.cropHeight			= ModuleDesc.SourceVSize;
		rszParams.cropWidth  			= ModuleDesc.SourceHSize;
		rszParams.ulOutputImageHeight 	= height;
		rszParams.ulOutputImageWidth  	= width;
		rszParams.ulWriteAddr 			= (ULONG) m_pYUVPhysicalAddr;
		rszParams.bReadFromMemory 		= FALSE;
		rszParams.ulReadAddr	  		= 0;	
#if RSZ_ENABLE		
		RSZInit(&rszParams);
		//Init. CCDC
        if(!CCDCInit(ModuleDesc.SourceHSize,ModuleDesc.SourceVSize))  //use resizer,the size is sensor's output
            return FALSE;
#else
		//Init. CCDC
        if(!CCDCInit(width,height))  
            return FALSE;
#endif		
		ISP_InReg32(&m_pIspConfigRegs->SYSCONFIG, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("ISP_SYSCONFIG    =0x%x\r\n"),setting));
        ISP_InReg32(&m_pIspConfigRegs->CTRL, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("ISP_CTRL         =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pIspConfigRegs->TCTRL_CTRL, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("TCTRL_CTRL       =0x%x\r\n"),setting));		
		ISP_InReg32(&m_pIspConfigRegs->IRQ0ENABLE, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("ISP_IRQ0ENABLE   =0x%x\r\n"),setting));
		
        ISP_InReg32(&m_pCCDCRegs->CCDC_SYN_MODE, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_SYN_MODE    =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_PCR, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_PCR         =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCCDCRegs->CCDC_HORZ_INFO, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_HORZ_INFO   =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_VERT_START, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_VERT_START  =0x%x\r\n"),setting));

		ISP_InReg32(&m_pCCDCRegs->CCDC_VERT_LINES, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_VERT_LINES  =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_CFG, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_CFG         =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCCDCRegs->CCDC_HORZ_INFO, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_HORZ_INFO   =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_VERT_START, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_VERT_START  =0x%x\r\n"),setting));


		ISP_InReg32(&m_pCCDCRegs->CCDC_HSIZE_OFF, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_HSIZE_OFF   =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_SDOFST, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_SDOFST      =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCCDCRegs->CCDC_SDR_ADDR, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_SDR_ADDR    =0x%x\r\n"),setting)); 
		ISP_InReg32(&m_pCCDCRegs->CCDC_VDINT, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("CCDC_VDINT       =0x%x\r\n"),setting));

#if RSZ_ENABLE		
		EnableRSZ(TRUE);
#endif
		ISP_InReg32(&m_pCAMRSZ->RSZ_PCR, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_PCR          =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_CNT, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_CNT          =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_OUT_SIZE, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_OUT_SIZE     =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_IN_START, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_IN_START     =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_IN_SIZE, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_IN_SIZE      =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_SDR_OUTADD, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_SDR_OUTADD   =0x%x\r\n"),setting));
		ISP_InReg32(&m_pCAMRSZ->RSZ_SDR_OUTOFF, &setting);
		DEBUGMSG(ZONE_FUNCTION, (TEXT("RSZ_SDR_OUTOFF   =0x%x\r\n"),setting));
		CCDCEnable(TRUE);
		ISPInterruptEnable(TRUE);
		m_sensorState = 1;
		DEBUGMSG(ZONE_FUNCTION, (TEXT("-EnableCamera\r\n"))); 		
        return TRUE;

}

BOOL CIspCtrl::PauseCamera(void)	
{
	DEBUGMSG(ZONE_FUNCTION, (TEXT("+PauseCamera\r\n")));	
	ISPInterruptEnable(FALSE);
	CCDCEnable(FALSE);
	EnableRSZ(FALSE);
	ISPEnable(FALSE); 
	m_sensorState = 2;
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-PauseCamera\r\n"))); 
	return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  DisableCamera
//
//  To disable Camera.
//
BOOL CIspCtrl::DisableCamera(void)
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+DisableCamera\r\n")));  
	ISPInterruptEnable(FALSE);
    CCDCEnable(FALSE);
	EnableRSZ(FALSE);
    ISPEnable(FALSE); 
	ModuleClose();
	m_sensorState = 0;
	SensorPowerDownSequence();
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-DisableCamera\r\n"))); 
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  ChangeFrameBuffer
//
//  To Change the frame buffer address to CCDC_SDR.
//
BOOL CIspCtrl::ChangeFrameBuffer(ULONG ulVirtAddr)
{
    DEBUGMSG(ZONE_FUNCTION, (TEXT("+ChangeFrameBuffer\r\n")));  

    m_pYUVVirtualAddr= (LPVOID) ulVirtAddr;
    m_pYUVPhysicalAddr = GetPhysFromVirt((ULONG)m_pYUVVirtualAddr);
    if(!m_pYUVPhysicalAddr)
        return FALSE;   
    
    //Set CCDC_SDR address
    CCDCSetOutputAddress((ULONG) m_pYUVPhysicalAddr);
    
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  EnableRSZ  
//  Description: This function enables the RSZ HW to resize data from memory/PRV
//
BOOL 
CIspCtrl::EnableRSZ(BOOL bEnableFlag)
{
    if (bEnableFlag == TRUE)
	{
		//RETAILMSG(1, (TEXT("Enable RESIZER\r\n")));
        SETREG32(&m_pCAMRSZ->RSZ_PCR, RSZ_PCR_ENABLE);
	}
    else
	{
		CLRREG32(&m_pCAMRSZ->RSZ_PCR, RSZ_PCR_ENABLE);
	}
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  CalculateRSZHWParams  
//  Description: This function calculates the RSZ HW params. It does not configure the 
//               hardware.   
//
BOOL 
CIspCtrl::CalculateRSZHWParams(
    RSZParams_t *pRSZParams
    )
{
    ULONG rsz, rsz_7, rsz_4;
    ULONG sph;
    ULONG input_w, input_h;
    ULONG output_w, output_h;
    ULONG output;
    ULONG max_in_otf, max_out_7tap;

    if(m_rszParams.enableZoom)
    {
    input_w = pRSZParams->cropWidth;
    input_h = pRSZParams->cropHeight;
    }
    else
    {
    input_w = pRSZParams->width;
    input_h = pRSZParams->height;
    }

    output_w = pRSZParams->ulOutputImageWidth;
    output_h = pRSZParams->ulOutputImageHeight;

    /*
     * Step 1: Recalculate input/output requirements based on TRM equations
     * Step 2: Programs hardware.
     *
     */


    /* STEP 1*/

    /*
     * We need to ensure input of resizer after size calculations does not
     * exceed output of the preview module.
     */

    input_w = input_w - 6;
    input_h = input_h - 6;

    if (input_h > RSZ_MAX_IN_HEIGHT)
    {
    RETAILMSG(1,(L"ERROR: Height exceeds maximum supported by RSZ\r\n"));
    return FALSE;
    }

    max_in_otf = RSZ_MAX_IN_WIDTH_ONTHEFLY_MODE_ES2;
    max_out_7tap = RSZ_MAX_7TAP_VRSZ_OUTWIDTH_ES2;

    if (input_w > max_in_otf)
    {
    RETAILMSG(1,(L"ERROR: Width exceeds maximum supported by RSZ\r\n"));
    return FALSE;
    }

    sph = RSZ_DEFAULTSTPHASE;

    output = output_h;

    /* Calculate height */
    rsz_7 = ((input_h - 7) * 256) / (output - 1);
    rsz_4 = ((input_h - 4) * 256) / (output - 1);

    rsz = (input_h * 256) / output;

    if (rsz <= RSZ_MID_RESIZE_VALUE)
    {
    rsz = rsz_4;
    if (rsz < RSZ_MINIMUM_RESIZE_VALUE) {
        rsz = RSZ_MINIMUM_RESIZE_VALUE;
        output = (((input_h - 4) * 256) / rsz) + 1;
        }
    }
    else
    {
    rsz = rsz_7;
    if (output_w > max_out_7tap)
        output_w = max_out_7tap;
    if (rsz > RSZ_MAXIMUM_RESIZE_VALUE)
        {
        rsz = RSZ_MAXIMUM_RESIZE_VALUE;
        output = (((input_h - 7) * 256) / rsz) + 1;
        }
    }

    /* Recalculate input */
    if (rsz > RSZ_MID_RESIZE_VALUE)
        input_h = (((64 * sph) + ((output - 1) * rsz) + 32) / 256) + 7;
    else
        input_h = (((32 * sph) + ((output - 1) * rsz) + 16) / 256) + 4;

    if(m_rszParams.enableZoom)
        pRSZParams->cropHeight = input_h;
    else
        pRSZParams->ulInputImageHeight = input_h;

    pRSZParams->v_resz = rsz;
    pRSZParams->ulOutputImageHeight = output;
    pRSZParams->cropTop = RSZ_DEFAULTSTPIXEL;
    pRSZParams->v_startphase = sph;

    /* Calculate Width */
    output = output_w;
    sph = RSZ_DEFAULTSTPHASE;

    rsz_7 = ((input_w - 7) * 256) / (output - 1);
    rsz_4 = ((input_w - 4) * 256) / (output - 1);

    rsz = (input_w * 256) / output;
    if (rsz > RSZ_MID_RESIZE_VALUE)
    {
    rsz = rsz_7;
        if (rsz > RSZ_MAXIMUM_RESIZE_VALUE)
        {
        rsz = RSZ_MAXIMUM_RESIZE_VALUE;
        output = (((input_w - 7) * 256) / rsz) + 1;
        RETAILMSG(1,(L"ERROR: Width exceeds max. Should be limited to %d\r\n", output));
        }
    }
    else
    {
    rsz = rsz_4;
        if (rsz < RSZ_MINIMUM_RESIZE_VALUE)
        {
        rsz = RSZ_MINIMUM_RESIZE_VALUE;
        output = (((input_w - 4) * 256) / rsz) + 1;
        RETAILMSG(1,(L"ERROR: Width less than min. Should be atleast %d\r\n", output));
        }
    }

        /* Recalculate input based on TRM equations */
    if (rsz > RSZ_MID_RESIZE_VALUE)
        input_w = (((64 * sph) + ((output - 1) * rsz) + 32) / 256) + 7;
    else
        input_w = (((32 * sph) + ((output - 1) * rsz) + 16) / 256) + 7;


    pRSZParams->ulOutputImageWidth = output;
    pRSZParams->h_resz = rsz;
    if(m_rszParams.enableZoom)
        pRSZParams->cropWidth = input_w;
    else
        pRSZParams->ulInputImageWidth = input_w;

    pRSZParams->cropLeft = RSZ_DEFAULTSTPIXEL;
    pRSZParams->h_startphase = sph;
    pRSZParams->enableZoom = m_rszParams.enableZoom;

    /* Store current settings to be used in zoom calculations */
    memcpy(&m_rszParams, pRSZParams, sizeof (RSZParams_t) );
    return TRUE;

}


//------------------------------------------------------------------------------
//
//  ConfigureRSZHW  
//  Description: This function configures the CCDC HW with the VideoInfo header
//               information passed to camera driver
//  
//
BOOL 
CIspCtrl::ConfigureRSZHW(
    RSZParams_t *pRSZParams
    )
{
    ULONG rszCnt = 0;
    rszCnt = ISPRSZ_CNT_YCPOS|
			RSZ_CNT_VSTPH(pRSZParams->v_startphase)|
             RSZ_CNT_HSTPH(pRSZParams->h_startphase)|
             RSZ_CNT_VRSZ((pRSZParams->v_resz - 1))|
             RSZ_CNT_HRSZ((pRSZParams->h_resz - 1))
             ;
             
    if (pRSZParams->bReadFromMemory == TRUE)
        {
        rszCnt |= RSZ_CNT_INPSRC;
        OUTREG32(&m_pCAMRSZ->RSZ_SDR_INADD, pRSZParams->ulReadAddr);
        OUTREG32(&m_pCAMRSZ->RSZ_SDR_INOFF, pRSZParams->ulReadOffset);
        DEBUGMSG(ZONE_VERBOSE,(L"RSZ read from memory, RDADDR:%x RDOFFST:%x\r\n",
                     INREG32(&m_pCAMRSZ->RSZ_SDR_INADD),
                     INREG32(&m_pCAMRSZ->RSZ_SDR_INOFF)
                     ));
                      
        }
    
    OUTREG32(&m_pCAMRSZ->RSZ_CNT, rszCnt);
                                   
    OUTREG32(&m_pCAMRSZ->RSZ_IN_START, RSZ_IN_START_VERT_ST(pRSZParams->cropTop)|
                                       RSZ_IN_START_HORZ_ST(pRSZParams->cropLeft));


    if(!m_rszParams.enableZoom)
    {
    OUTREG32(&m_pCAMRSZ->RSZ_IN_SIZE, RSZ_IN_SIZE_VERT(pRSZParams->ulInputImageHeight)|
                                      RSZ_IN_SIZE_HORZ(pRSZParams->ulInputImageWidth));
    }
    else
    {
    OUTREG32(&m_pCAMRSZ->RSZ_IN_SIZE, RSZ_IN_SIZE_VERT(pRSZParams->cropHeight)|
                                      RSZ_IN_SIZE_HORZ(pRSZParams->cropWidth));
    }
    
    OUTREG32(&m_pCAMRSZ->RSZ_OUT_SIZE, RSZ_OUT_SIZE_VERT(pRSZParams->ulOutputImageHeight)|
                                       RSZ_OUT_SIZE_HORZ(pRSZParams->ulOutputImageWidth));
                                     
    m_ulFilledBuffer = pRSZParams->ulWriteAddr;

    // Set SDR output address
    OUTREG32(&m_pCAMRSZ->RSZ_SDR_OUTADD, pRSZParams->ulWriteAddr);

    // Set the output line offset
    OUTREG32(&m_pCAMRSZ->RSZ_SDR_OUTOFF, ((pRSZParams->ulOutputImageWidth *2) << RSZ_SDR_OUTOFF_OFFSET_SHIFT));

	//must be here
	rszCnt = INREG32(&m_pCAMRSZ->RSZ_CNT);	
	OUTREG32(&m_pCAMRSZ->RSZ_CNT,rszCnt|ISPRSZ_CNT_YCPOS);

	rszCnt = INREG32(&m_pCAMRSZ->RSZ_CNT);	
	OUTREG32(&m_pCAMRSZ->RSZ_CNT,rszCnt|ISPRSZ_CNT_CBILIN);
	OUTREG32(&m_pCAMRSZ->RSZ_YENH,0);
	
    // Program the Horz filter coeffs
    if(pRSZParams->h_resz <= RSZ_MID_RESIZE_VALUE)
        {
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT10), ((Rsz4TapModeHorzFilterTable[0] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[1] << RSZ_HFILT_COEFH_SHIFT)));			
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT32), ((Rsz4TapModeHorzFilterTable[2] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[3] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT54), ((Rsz4TapModeHorzFilterTable[4] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[5] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT76), ((Rsz4TapModeHorzFilterTable[6] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[7] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT98), ((Rsz4TapModeHorzFilterTable[8] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[9] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1110), ((Rsz4TapModeHorzFilterTable[10] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[11] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1312), ((Rsz4TapModeHorzFilterTable[12] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[13] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1514), ((Rsz4TapModeHorzFilterTable[14] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[15] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1716), ((Rsz4TapModeHorzFilterTable[16] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[17] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1918), ((Rsz4TapModeHorzFilterTable[18] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[19] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2120), ((Rsz4TapModeHorzFilterTable[20] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[21] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2322), ((Rsz4TapModeHorzFilterTable[22] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[23] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2524), ((Rsz4TapModeHorzFilterTable[24] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[25] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2726), ((Rsz4TapModeHorzFilterTable[26] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[27] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2928), ((Rsz4TapModeHorzFilterTable[28] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[29] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT3130), ((Rsz4TapModeHorzFilterTable[30] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz4TapModeHorzFilterTable[31] << RSZ_HFILT_COEFH_SHIFT)));
        }
    else
        {

            OUTREG32((&m_pCAMRSZ->RSZ_HFILT10), ((Rsz7TapModeHorzFilterTable[0] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[1] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT32), ((Rsz7TapModeHorzFilterTable[2] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[3] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT54), ((Rsz7TapModeHorzFilterTable[4] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[5] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT76), ((Rsz7TapModeHorzFilterTable[6] << RSZ_HFILT_COEFL_SHIFT)));

            OUTREG32((&m_pCAMRSZ->RSZ_HFILT98), ((Rsz7TapModeHorzFilterTable[7] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[8] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1110), ((Rsz7TapModeHorzFilterTable[9] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[10] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1312), ((Rsz7TapModeHorzFilterTable[11] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[12] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1514), ((Rsz7TapModeHorzFilterTable[13] << RSZ_HFILT_COEFL_SHIFT)));


            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1716), ((Rsz7TapModeHorzFilterTable[14] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[15] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT1918), ((Rsz7TapModeHorzFilterTable[16] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[17] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2120), ((Rsz7TapModeHorzFilterTable[18] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[19] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2322), ((Rsz7TapModeHorzFilterTable[20] << RSZ_HFILT_COEFL_SHIFT)));

            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2524), ((Rsz7TapModeHorzFilterTable[21] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[22] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2726), ((Rsz7TapModeHorzFilterTable[23] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[24] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT2928), ((Rsz7TapModeHorzFilterTable[25] << RSZ_HFILT_COEFL_SHIFT)
                | (Rsz7TapModeHorzFilterTable[26] << RSZ_HFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_HFILT3130), ((Rsz7TapModeHorzFilterTable[27] << RSZ_HFILT_COEFL_SHIFT)));
        }

    // Program the Vert filter coeffs
    if(pRSZParams->v_resz <= RSZ_MID_RESIZE_VALUE)
        {
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT10), ((Rsz4TapModeVertFilterTable[0] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[1] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT32), ((Rsz4TapModeVertFilterTable[2] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[3] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT54), ((Rsz4TapModeVertFilterTable[4] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[5] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT76), ((Rsz4TapModeVertFilterTable[6] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[7] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT98), ((Rsz4TapModeVertFilterTable[8] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[9] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1110), ((Rsz4TapModeVertFilterTable[10] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[11] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1312), ((Rsz4TapModeVertFilterTable[12] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[13] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1514), ((Rsz4TapModeVertFilterTable[14] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[15] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1716), ((Rsz4TapModeVertFilterTable[16] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[17] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1918), ((Rsz4TapModeVertFilterTable[18] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[19] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2120), ((Rsz4TapModeVertFilterTable[20] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[21] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2322), ((Rsz4TapModeVertFilterTable[22] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[23] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2524), ((Rsz4TapModeVertFilterTable[24] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[25] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2726), ((Rsz4TapModeVertFilterTable[26] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[27] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2928), ((Rsz4TapModeVertFilterTable[28] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[29] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT3130), ((Rsz4TapModeVertFilterTable[30] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz4TapModeVertFilterTable[31] << RSZ_VFILT_COEFH_SHIFT)));      }
    else
        {
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT10), ((Rsz7TapModeVertFilterTable[0] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[1] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT32), ((Rsz7TapModeVertFilterTable[2] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[3] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT54), ((Rsz7TapModeVertFilterTable[4] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[5] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT76), ((Rsz7TapModeVertFilterTable[6] << RSZ_VFILT_COEFL_SHIFT)));

            OUTREG32((&m_pCAMRSZ->RSZ_VFILT98), ((Rsz7TapModeVertFilterTable[7] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[8] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1110), ((Rsz7TapModeVertFilterTable[9] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[10] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1312), ((Rsz7TapModeVertFilterTable[11] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[12] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1514), ((Rsz7TapModeVertFilterTable[13] << RSZ_VFILT_COEFL_SHIFT)));


            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1716), ((Rsz7TapModeVertFilterTable[14] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[15] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT1918), ((Rsz7TapModeVertFilterTable[16] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[17] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2120), ((Rsz7TapModeVertFilterTable[18] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[19] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2322), ((Rsz7TapModeVertFilterTable[20] << RSZ_VFILT_COEFL_SHIFT)));

            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2524), ((Rsz7TapModeVertFilterTable[21] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[22] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2726), ((Rsz7TapModeVertFilterTable[23] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[24] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT2928), ((Rsz7TapModeVertFilterTable[25] << RSZ_VFILT_COEFL_SHIFT)
                | (Rsz7TapModeVertFilterTable[26] << RSZ_VFILT_COEFH_SHIFT)));
            OUTREG32((&m_pCAMRSZ->RSZ_VFILT3130), ((Rsz7TapModeVertFilterTable[27] << RSZ_VFILT_COEFL_SHIFT)));
        }

    /* Configure RSZ in one shot mode */
//    SETREG32(&m_pCAMRSZ->RSZ_PCR, RSZ_PCR_ONESHOT);
	CLRREG32(&m_pCAMRSZ->RSZ_PCR, RSZ_PCR_ONESHOT);
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  RSZInit
//
//  Init. Camera RSZInit
//
//

BOOL 
CIspCtrl::RSZInit(
    RSZParams_t *pRSZParams
    )
{
	m_rszParams.enableZoom = 0;
	if(CalculateRSZHWParams(pRSZParams) == FALSE)
		return FALSE;
	if(ConfigureRSZHW(pRSZParams) == FALSE)
		return FALSE;
	return TRUE;
}

