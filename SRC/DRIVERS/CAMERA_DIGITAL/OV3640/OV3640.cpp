#include <windows.h>
#include <winbase.h>
#include <nkintr.h>
#include <bsp.h>
//#include <oalintr.h>
#include <pm.h>
#include <ceddk.h>
#include "sdk_i2c.h"
#include "module.h"
//#include "ov3640.h"	
#include "fang_OV3640.h"
#include "Omap3530_base_regs.h"
#include "Cs.h"
#include "csmedia.h"
#include "CameraPDDProps.h"

#include "dstruct.h"
#include "cam_pdd.h"
#include "cameradriver.h"

// Macros

// Definitions
#define MSG_ERROR        					1
#define MSG_MODULE							1//0 // wangchong 2010-8-23

#define CAMERA_ADDR    						0x30  //wangwj add 0x3C			//78>>1
#define DEFAULT_MODULE_ITUXXX        		CAM_ITU656//CAM_ITU601
#define DEFAULT_MODULE_YUVORDER        		CAM_ORDER_YCBYCR
#define DEFAULT_MODULE_HSIZE        		0				//VGA default
#define DEFAULT_MODULE_VSIZE        		0
#define DEFAULT_MODULE_HOFFSET        		0
#define DEFAULT_MODULE_VOFFSET        		0
#define DEFAULT_MODULE_UVOFFSET        		CAM_UVOFFSET_128
#define DEFAULT_MODULE_CLOCK        		24000000
#define DEFAULT_MODULE_CODEC        		CAM_CODEC_422
#define DEFAULT_MODULE_HIGHRST        		1
#define DEFAULT_MODULE_INVPCLK        		0
#define DEFAULT_MODULE_INVVSYNC        		0
#define DEFAULT_MODULE_INVHREF         		0
//#define OV3640_ID							0x364C			//3640ID,check for detection
//#define OV5642_ID							0x5642
#define OV5642_ID							0x2656

//---------------------------------------Variables-------------------------------------//
static MODULE_DESCRIPTOR             		gModuleDesc;	// Save sensor' setting parameters
static HANDLE                        		hI2C;   		// I2C Bus Driver


// Internal Functions
DWORD 	ModuleDetect(void);
DWORD 	read_i2c(USHORT StartReg,PUCHAR pData);
DWORD 	write_i2c(USHORT StartReg, UCHAR Data);
BOOL 	InitSensorAFC(void);
DWORD	WriteRegisters(const struct RegStruct *sensor_reg);



/////////////////////////////////////////////////////////////////////////////////

//------------------------------------------------------------------------------
//
//  Function:  I2CInit
//
//  Init 3640 I2C interface
//      
BOOL I2CInit()
{
    //hI2C = I2COpen(I2CGetDeviceIdFromMembase(OMAP_I2C2_REGS_PA));  
    hI2C = I2COpen(OMAP_DEVICE_I2C2);
	if(hI2C == NULL)
		return FALSE;
    I2CSetSlaveAddress(hI2C,  CAMERA_ADDR);
	I2CSetBaudIndex(hI2C,FULLSPEED_MODE);//
    I2CSetSubAddressMode(hI2C, I2C_SUBADDRESS_MODE_0);
    //subaddress organized by outselves.    
    return TRUE;
}

//------------------------------------------------------------------------------
//
//  Function:  I2CDeinit
//
//  Deinit 3640 I2C interface
//      
BOOL I2CDeinit()
{
    if (hI2C)
    {
        I2CClose(hI2C);
        hI2C = NULL;
    }
    return TRUE;
}

//OV Sensor's sccb bus'operation is strange,it's order:
//Read operation:ID(w)+ADDR+ID(R)==> <==Data
//Write operation:ID(w)+ADDR+Data
DWORD
write_i2c(USHORT StartReg, UCHAR Data)
{
    DWORD dwErr=0;
	UCHAR data[3];
	data[0] = StartReg>>8;
	data[1] = StartReg&0x00ff;
	data[2] = Data;
	DWORD len = I2CWrite(hI2C, NULL, data, 3);
    if(len != 3)
    {
    	dwErr = GetLastError();
        RETAILMSG(MSG_ERROR,(TEXT("IOCTL_IIC_WRITE ERROR\r\n")));
		return 0;
    }
    return len;
}

DWORD
read_i2c(USHORT StartReg,PUCHAR pData)
{
	DWORD len;
	UCHAR data[2];	
	data[0] = StartReg>>8;
	data[1] = StartReg&0x00ff;
	len = I2CWrite(hI2C, 0, data, 2);//ID(w)+ADDR
	if(len != 2)
	{
		RETAILMSG(MSG_ERROR,(TEXT("I2CRead ERROR \r\n")));
        return 0;
	}
	len = I2CRead(hI2C, 0, pData, 1);  	
    if ( len !=  1) {      
        RETAILMSG(MSG_ERROR,(TEXT("I2CRead ERROR \r\n")));
        return 0;
    }
    return len;
}
//Write Registers
DWORD
WriteRegisters(const struct RegStruct *sensor_reg)
{
	DWORD i;
	for(i=0; ; i++)
	{
		if(sensor_reg[i].subaddr == 0)
		{
			Sleep(sensor_reg[i].value); 
		}
		else if(sensor_reg[i].subaddr == 0xffff)//terminate
		{
			break;
		}
		else if(write_i2c(sensor_reg[i].subaddr, sensor_reg[i].value) == 0)
		{
			RETAILMSG(MSG_ERROR,(TEXT("ModuleSetImage HW_WriteRegisters ERROR\r\n")));
			return FALSE;
		}
		Sleep(20);//wangwj add to write reg corectly
	}
	return TRUE;
}


//return id,if return value is zero,error accur.
DWORD ModuleDetect(void)
{
	UCHAR hByte;
	UCHAR lByte;
	DWORD id = 0;	
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleDetect\r\n")));
	if(0 == read_i2c(0x300A,&hByte))
	{
		RETAILMSG(MSG_MODULE,(TEXT("ModuleDetect HW_ReadRegisters Failed\r\n")));
		return id;
	}
	if(0 == read_i2c(0x300B,&lByte))
	{	
		RETAILMSG(MSG_MODULE,(TEXT("ModuleDetect HW_ReadRegisters Failed\r\n")));
		return id;
	}
	id = (hByte<<8)+lByte;
	if(OV5642_ID == id)
	{
		RETAILMSG(MSG_MODULE,(TEXT("-ModuleDetect Success\r\n")));
		return OV5642_ID;
	}
	RETAILMSG(MSG_ERROR,(TEXT("-ModuleDetect Failed:0x%x!!!!\r\n"),id));
	return NULL;
	
}

BOOL ModuleInit()
{
    DWORD dwErr = ERROR_SUCCESS;
    RETAILMSG(MSG_MODULE,(TEXT("+ModuleInit\n")));
	if(I2CInit() == NULL)
		return FALSE;
    gModuleDesc.ITUXXX 			= DEFAULT_MODULE_ITUXXX;
	gModuleDesc.ImageType 		= CAMRES_INVALID;
    gModuleDesc.UVOffset 		= DEFAULT_MODULE_UVOFFSET;
    gModuleDesc.SourceHSize 	= DEFAULT_MODULE_HSIZE;
    gModuleDesc.Order422 		= DEFAULT_MODULE_YUVORDER;
    gModuleDesc.SourceVSize		= DEFAULT_MODULE_VSIZE;
    gModuleDesc.Clock			= DEFAULT_MODULE_CLOCK;
    gModuleDesc.Codec			= DEFAULT_MODULE_CODEC;
    gModuleDesc.HighRst			= DEFAULT_MODULE_HIGHRST;
    gModuleDesc.SourceHOffset	= DEFAULT_MODULE_HOFFSET;
    gModuleDesc.SourceVOffset	= DEFAULT_MODULE_VOFFSET;
    gModuleDesc.InvPCLK			= DEFAULT_MODULE_INVPCLK;
    gModuleDesc.InvVSYNC		= DEFAULT_MODULE_INVVSYNC;
    gModuleDesc.InvHREF			= DEFAULT_MODULE_INVHREF;	
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleInit\n")));
    return TRUE;
}

void ModuleDeinit(void)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleDeinit\n")));
    I2CDeinit();
}

BOOL ModuleOpen(void)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleOpen\n")));
	if(InitSensorAFC() == NULL)
	{
		return FALSE;
	}

	RETAILMSG(MSG_MODULE,(TEXT("-ModuleOpen\n")));
	return TRUE;
}

void ModuleClose(void)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleClose\n")));
    SingleFocus(FALSE);	
}


#define STEP_STATE_NO 		0x80
#define MASK_MODECHANGE 	0x40
#define MASK_CAPTURECMD 	0x20
#define RESERVED_INVALID 	0x10
#define MODE_IDLE 			0x00
#define MODE_SINGLE 		0x04
#define MODE_CONTINUE 		0x08
#define MODE_STEP 			0x0c
#define MODE_STEP_INSTRUCTION 0x00
#define MODE_STEP_FOCUSING 	0x01
#define MODE_STEP_FOCUSED 	0x02
#define MODE_STEP_CAPTURE 	0x03

//Single focus function
//be clear:AF must initiated before changing to other resolution pic.
//due to ov's problem,i don't understand why
BOOL SingleFocus(BOOL enable)
{
	UCHAR state;	
	UCHAR STATE_INF 		=MODE_IDLE|MODE_STEP_INSTRUCTION;
	UCHAR STATE_SINGLE  	=MODE_SINGLE|MODE_STEP_FOCUSING|MASK_MODECHANGE|MASK_CAPTURECMD;
	UCHAR STATE_SUCCESS_S	=MODE_SINGLE|MODE_STEP_FOCUSED|MASK_MODECHANGE;
	UCHAR STATE_FAIL_S 		=MODE_SINGLE|MODE_STEP_FOCUSED|MASK_MODECHANGE|STEP_STATE_NO;
	UCHAR STATE_CAPTURE_S	=MODE_SINGLE|MODE_STEP_CAPTURE|MASK_MODECHANGE;
	UINT16 wait;
	static BOOL bInit = FALSE;
	/*
	if(bInit)
	{
	bInit = FALSE;
	RETAILMSG(MSG_MODULE,(TEXT("+SingleFocus\r\n")));
	if(enable != TRUE)
	{
		write_i2c(0x3f00,0x08);		//Send finish command
//		write_i2c(0x3f00,0x01);
		RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus disable!!\r\n")));
		return TRUE;
	}		
//	WriteRegisters(ov3640_reg_AF); // wangchong 2010-9-2
	RETAILMSG(MSG_MODULE,(TEXT("Start SingleFocus\r\n")));
	wait = 100;// 1s
	do
	{		
		read_i2c(0x3f01,&state);	//read state register
		if(STATE_INF == state)
			break;
		Sleep(10);					//10ms		
	}while(--wait);
	// step 1:Check current firmware running state	
//	read_i2c(0x3f01,&state);		//read state register	
	if(STATE_INF != state)
	{
		RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus AF is busy state=0x%x!!\r\n"),state));
		return FALSE;
	}
	
	//step 2:Send single focus command
	write_i2c(0x3f00,0x03);//write cmd_SingleMode(0x03)to command register(0x3f00);	
	wait = 200;// 2s
	do
	{		
		read_i2c(0x3f01,&state);	//read state register
		if(STATE_SINGLE != state)
			break;
		Sleep(10);					//10ms		
	}while(--wait);
	if(!wait)
	{
		RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus Fail_step1 state=0x%x!!\r\n"),state));
		return FALSE;
	}
	if(STATE_FAIL_S == state)
	{	
		RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus Fail_step2 state=0x%x!!\r\n"),state));
		return FALSE;
	}
	else if(STATE_SUCCESS_S == state)	//focused
	{			
		RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus Success!!\r\n")));
		write_i2c(0x3f00,0x02);			//disable overlay window
	//	write_i2c(0x3f00,0x08);//Send finish command	
		
		//Step 02   Stop AEC/AGC and set AEC value to maximum if it is not maximum
		read_i2c(0x3013,&state);
		write_i2c(0x3013,state&0xfa);//AEC & AGC auto closed
		write_i2c(0x3002,1563>>8);//1536
		write_i2c(0x3002,1563&0x0ff);
		// wangchong 2010-9-6, Stop AEC/AGC
		write_i2c(0x3503, 0x07);
		return TRUE;
	}
	RETAILMSG(MSG_MODULE,(TEXT("-SingleFocus Fail_step3 state=0x%x!!\r\n"),state));
	return FALSE;
	}
	else*/
		return TRUE;
}



//Initialize Sensor and AFC
BOOL InitSensorAFC(void)
{
	/* Reset */
//	write_i2c(0x3008, 0x80);	// software reset
//	write_i2c(0x0000, 0x0a);	// delay 10ms

	RETAILMSG(1,(TEXT("#wangwj#---Camera reset       \r\n\r\n")));
	write_i2c(0x3012, 0x80);	// software reset
	RETAILMSG(1,(TEXT("#wangwj#---Camera reset    over   \r\n\r\n")));
	/* Init	*/	

	RETAILMSG(1,(TEXT("#wangwj#---Camera   Write init regs     \r\n\r\n")));
//	WriteRegisters(ov3640_reg_Initialization);	//ftm del test
	WriteRegisters(ov3640_reg_QXGA_UXGA);	//ftm add test

	RETAILMSG(1,(TEXT("#wangwj#---Camera   Write init regs  over   \r\n\r\n")));
	//write_i2c(0x3308, 0xa5); 	// Simple AWB	
	//write_i2c(0x3301, 0xde); 	// Pixel Correction ON,bit[2:1]: 11,select enable

//	write_i2c(0x307c, 0x12);	//mirror
//	write_i2c(0x3090, 0xc8);
//	write_i2c(0x3023, 0x0a);

	//WriteRegisters(ov3640_reg_AF);		
	//Sleep(50);
	//SingleFocus(TRUE);	

#if 0
//	write_i2c(0x3000, 0x20);
//	write_i2c(0x3001, 0xff);
//	write_i2c(0x3002, 0xff);
//	write_i2c(0x3003, 0xff);
///	write_i2c(0x3004, 0xff);
//	write_i2c(0x3005, 0xff);
//	write_i2c(0x3006, 0xff);
//	write_i2c(0x3007, 0x3f);
	write_i2c(0x3008, 0x80);	// software reset
	write_i2c(0x0000, 0x0a);	// delay 10ms
	write_i2c(0x3017, 0xff); // FREX, VSYNC, HREF, PCLK output enable
	write_i2c(0x3018, 0xff);
	write_i2c(0x3004, 0xff);
	write_i2c(0x3000, 0xff);
	write_i2c(0xffff, 0x00);

//	write_i2c(0x300f, 0x06);
//	write_i2c(0x3011, 0x14);
//	write_i2c(0x3016, 0x02);
	// I/O Control

//	write_i2c(0x3019, 0x02);
//	write_i2c(0x301a, 0xff);
//	write_i2c(0x301b, 0xf3);
//	write_i2c(0x301c, 0x02);
//	write_i2c(0x301d, 0xff);
//	write_i2c(0x301e, 0xf3);
//	write_i2c(0x302c, 0x82); // output drive capability control
#endif
	return TRUE;
}


BOOL ModuleSetImage(ULONG lModeType)
{
	const struct RegStruct *sensor_reg = 0;	
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetImage::lModeType=%d\r\n"),lModeType));		
	    switch(lModeType)
	    {
			case QVGA://preview only surpport QVGA CIF VGA SVGA XGA five resolutions
				sensor_reg = ov3640_reg_QXGA_QVGA;
				gModuleDesc.SourceHSize = 320;
	    		gModuleDesc.SourceVSize = 240;
				gModuleDesc.ImageType = QVGA;
				break;	
			case QCIF:
				sensor_reg = ov3640_reg_QXGA_CIF;
				gModuleDesc.SourceHSize = 352;
	    		gModuleDesc.SourceVSize = 288;
				gModuleDesc.ImageType = QCIF;
				break;
			case VGA:
				sensor_reg = ov3640_reg_Initialization;
				gModuleDesc.SourceHSize = 640;
	    		gModuleDesc.SourceVSize = 480;
				gModuleDesc.ImageType = VGA;
				break;
			case SVGA:
				sensor_reg = ov3640_reg_QXGA_VGA;
				gModuleDesc.SourceHSize = 800;
	    		gModuleDesc.SourceVSize = 600;
				gModuleDesc.ImageType = SVGA;
				break;
			case XGA:
				sensor_reg = ov3640_reg_QXGA_XGA;
				gModuleDesc.SourceHSize = 1024;
	    		gModuleDesc.SourceVSize = 768;
				gModuleDesc.ImageType = XGA;
				break;
//ftm add test
			case UXGA:
				sensor_reg = ov3640_reg_QXGA_UXGA;
				gModuleDesc.SourceHSize = 1600;
	    		gModuleDesc.SourceVSize = 1200;
				gModuleDesc.ImageType = UXGA;
				break;
//ftm add test
			case QXGA://designed still only surpport QXGA		
				//Stop AE/AG
			//	read_i2c(0x3013,&state);
			//	write_i2c(0x3013,state & 0xfa);
				sensor_reg = ov3640_reg_PREVIEW2QXGA;				
				gModuleDesc.SourceHSize = 2592;
	    		gModuleDesc.SourceVSize = 1944;
				gModuleDesc.ImageType = QXGA;				
				break;
			case CAMRES_INVALID:
				break;
			default:
				return FALSE;
	    }

	// ye 2010-9-6
//	gModuleDesc.SourceHSize = 640;
//	gModuleDesc.SourceVSize = 480;
//	gModuleDesc.ImageType = VGA;
//	sensor_reg = ov3640_reg_Initialization; // wangchong 2010-9-3
	if(sensor_reg)
		WriteRegisters(sensor_reg);	
/*	if(QXGA == lModeType)
	{
		ModuleStrobeFlash(TRUE);
	}*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetImage\n")));
    return TRUE;
}

// copy module data to output buffer
void ModuleGetFormat(MODULE_DESCRIPTOR &outModuleDesc)
{
    memcpy(&outModuleDesc, &gModuleDesc, sizeof(MODULE_DESCRIPTOR));
}



BOOL ModuleSetLightMode(ULONG lightMode)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetLightMode\n")));
/*
	switch(lightMode)
	{
		case 0://AUTO_LIGHTMODE:
			write_i2c(0x332b, 0x00);//AWB auto, bit[3]:0,auto
			break;
		case 1://SUNNY_LIGHTMODE:
			write_i2c(0x332b, 0x08); //AWB off
			write_i2c(0x33a7, 0x5e);
			write_i2c(0x33a8, 0x40);
			write_i2c(0x33a9, 0x46);
			break;
		case 2://CLOUDY_LIGHTMODE:
			write_i2c(0x332b, 0x08); 
			write_i2c(0x33a7, 0x68);
			write_i2c(0x33a8, 0x40);
			write_i2c(0x33a9, 0x4e);
			break;
		case 3://OFFICE_LIGHTMODE:
			write_i2c(0x332b, 0x08); 
			write_i2c(0x33a7, 0x52);
			write_i2c(0x33a8, 0x40);
			write_i2c(0x33a9, 0x58);
			break;
		case 4://HOME_LIGHTMODE:
			write_i2c(0x332b, 0x08); 
			write_i2c(0x33a7, 0x44);
			write_i2c(0x33a8, 0x40);
			write_i2c(0x33a9, 0x70);
			break;
		default:
			return FALSE;			
	} 
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetLightMode\n")));
	return TRUE;
}


BOOL ModuleSetSaturation(INT8 plus)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetSaturation\n")));
/*
	switch(plus)
	{
		case 2://Saturation + 2(1.75x)
			write_i2c(0x3302, 0xef);//bit[7]:1, enable SDE
			write_i2c(0x3355, 0x02);//enable color saturation 
			write_i2c(0x3358, 0x70);
			write_i2c(0x3359, 0x70);
			break;
		case 1://Saturation + 1(1.25x)
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x02);  
			write_i2c(0x3358, 0x50);
			write_i2c(0x3359, 0x50);
			break;
		case 0://Saturation + 0
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x02); 
			write_i2c(0x3358, 0x40);
			write_i2c(0x3359, 0x40);
			break;
		case -1://Saturation -1(0.75x)
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x02);  
			write_i2c(0x3358, 0x30);
			write_i2c(0x3359, 0x30);
			break;
		case -2://Saturation ¨C 2(0.25x)
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x02);  
			write_i2c(0x3358, 0x10);
			write_i2c(0x3359, 0x10);
			break;
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetSaturation\n")));
	return TRUE;
}

BOOL ModuleSetBrightness(INT8 plus)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetBrightness\n")));
/*
	switch(plus)
	{
		case 3://Brightness +3
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);//  bit[2] enable
			write_i2c(0x3354, 0x01);//  bit[3] sign of brightness
			write_i2c(0x335e, 0x30);
			break;
		case 2://Brightness +2
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);
			write_i2c(0x3354, 0x01);
			write_i2c(0x335e, 0x20);
			break;
		case 1://Brightness +1
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01);
			write_i2c(0x335e, 0x10);
			break;
		case 0://Brightness 0
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01);
			write_i2c(0x335e, 0x00);
			break;
		case -1://Brightness -1
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x09);
			write_i2c(0x335e, 0x10);
		case -2://Brightness -2
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04); 
			write_i2c(0x3354, 0x09);
			write_i2c(0x335e, 0x20);
			break;
		case -3://Brightness -3
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x09);
			write_i2c(0x335e, 0x30);
			break;
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetBrightness\n")));
	return TRUE;
}

BOOL ModuleSetContrast(INT8 plus)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetContrast\n")));
/*
	switch(plus)
	{
		case 3://Brightness +3
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);//  bit[2] enable contrast/brightness
			write_i2c(0x3354, 0x01);// bit[2] Yoffset sign
			write_i2c(0x335c, 0x2c);
			write_i2c(0x335d, 0x2c);
			break;
		case 2://Brightness +2
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x28);
			write_i2c(0x335d, 0x28);
			break;
		case 1://Brightness +1
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04); 
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x24);
			write_i2c(0x335d, 0x24);
			break;
		case 0://Brightness 0
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x20);
			write_i2c(0x335d, 0x20);
			break;
		case -1://Brightness -1
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x1c);
			write_i2c(0x335d, 0x1c);
			break;
		case -2://Brightness -2
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x18);
			write_i2c(0x335d, 0x18);
			break;
		case -3://Brightness -3
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x04);  
			write_i2c(0x3354, 0x01); 
			write_i2c(0x335c, 0x14);
			write_i2c(0x335d, 0x14);
			break;
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetContrast\n")));
	return TRUE;
}


BOOL ModuleSetEffects(ULONG effect)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetEffects\n")));
/*
	switch(effect)
	{
		case SEPIA_SPEFFECT://Sepia(antique)
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);
			write_i2c(0x335a, 0x40);
			write_i2c(0x335b, 0xa6);
			break;
		case BLUISH_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);
			write_i2c(0x335a, 0xa0);
			write_i2c(0x335b, 0x40);
			break;
		case GREENISH_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);
			write_i2c(0x335a, 0x60);
			write_i2c(0x335b, 0x60);
			break;
		case REDDISH_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);
			write_i2c(0x335a, 0x80);
			write_i2c(0x335b, 0xc0);
			break;
		case YELLOWISH_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);
			write_i2c(0x335a, 0x30);
			write_i2c(0x335b, 0x90);
			break;
		case BANDW_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x18);//bit[4]fix u enable, bit[3]fix v enable
			write_i2c(0x335a, 0x80);
			write_i2c(0x335b, 0x80);
			break;
		case NEGATIVE_SPEFFECT://Brightness -3
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x40);//bit[6] negative
			break;
		case NORMAL_SPEFFECT:
			write_i2c(0x3302, 0xef);
			write_i2c(0x3355, 0x00);
			break;		
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetEffects\n")));
	return TRUE;
}


BOOL ModuleSetSharpness(ULONG sharpness)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetSharpness\n")));
/*
	switch(sharpness)
	{
		case 1://Sharpness 1
			write_i2c(0x332d, 0x41);
			break;
		case 2:
			write_i2c(0x332d, 0x42);
			break;
		case 3:
			write_i2c(0x332d, 0x43);
			break;
		case 4:
			write_i2c(0x332d, 0x44);
			break;
		case 5:
			write_i2c(0x332d, 0x45);
			break;
		case 6:
			write_i2c(0x332d, 0x46);
			break;
		case 7:
			write_i2c(0x332d, 0x47);
			break;
		case 8:
			write_i2c(0x332d, 0x48);
			break;		
		case 9://Sharpness auto
			write_i2c(0x332d, 0x60);
			write_i2c(0x332f, 0x03);
			break;	
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetSharpness\n")));
	return TRUE;
}


BOOL ModuleSetExposureAverage(ULONG level)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetExposureAverage\n")));
/*
	if((level <1) || (level >11))
		return FALSE;
	write_i2c(0x3047,0x00);
	switch(level)
	{			
		case 1://-1.7EV
			write_i2c(0x3018, 0x10);
			write_i2c(0x3019, 0x08);
			write_i2c(0x301a, 0x21);
			break;		 
		case 2://	-1.3EV	 
			write_i2c(0x3018, 0x18);
			write_i2c(0x3019, 0x10);
			write_i2c(0x301a, 0x31);
			break;		  
		case 3:	//	-1.0EV	  
			write_i2c(0x3018, 0x20);
			write_i2c(0x3019, 0x18);
			write_i2c(0x301a, 0x41);
			break;		  
		case 4://	-0.7EV	 
			write_i2c(0x3018, 0x28);
			write_i2c(0x3019, 0x20);
			write_i2c(0x301a, 0x51);
			break;		  
		case 5://	-0.3EV	  
			write_i2c(0x3018, 0x30);
			write_i2c(0x3019, 0x28);
			write_i2c(0x301a, 0x61);
			break;		  
		case 6://	default   
			write_i2c(0x3018, 0x38);
			write_i2c(0x3019, 0x30);
			write_i2c(0x301a, 0x61);
			break;		  
		case 7://	0.3EV	  
			write_i2c(0x3018, 0x40);
			write_i2c(0x3019, 0x38);
			write_i2c(0x301a, 0x71);
			break;		  
		case 8://	0.7EV	 
			write_i2c(0x3018, 0x48);
			write_i2c(0x3019, 0x40);
			write_i2c(0x301a, 0x81);
			break;		
		case 9://	1.0EV	  
			write_i2c(0x3018, 0x50);
			write_i2c(0x3019, 0x48);
			write_i2c(0x301a, 0x91);
			break;		
		case 10://	1.3EV	
			write_i2c(0x3018, 0x58);
			write_i2c(0x3019, 0x50);
			write_i2c(0x301a, 0x91);
			break;
		case 11://  	1.7EV	  
			write_i2c(0x3018, 0x60);
			write_i2c(0x3019, 0x58);
			write_i2c(0x301a, 0xa1);
			break;		
		default:
			return FALSE;
	*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetExposureAverage\n")));
	return TRUE;
}

BOOL ModuleSetExposureHistogram(ULONG level)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetExposureHistogram\n")));
/*
	if((level <1) || (level >11))
		return FALSE;
	write_i2c(0x3047,0x80);
	switch(level)
	{			
		case 1://-1.7EV
			write_i2c(0x3018, 0x58);
			write_i2c(0x3019, 0x38);
			break;		 
		case 2://	-1.3EV	 
			write_i2c(0x3018, 0x60);
			write_i2c(0x3019, 0x40);
			break;		  
		case 3:	//	-1.0EV	  
			write_i2c(0x3018, 0x68);
			write_i2c(0x3019, 0x48);
			break;		  
		case 4://	-0.7EV	 
			write_i2c(0x3018, 0x70);
			write_i2c(0x3019, 0x50);
			break;		  
		case 5://	-0.3EV	  
			write_i2c(0x3018, 0x78);
			write_i2c(0x3019, 0x58);
			break;		  
		case 6://	default   
			write_i2c(0x3018, 0x80);
			write_i2c(0x3019, 0x60);
			break;		  
		case 7://	0.3EV	  
			write_i2c(0x3018, 0x88);
			write_i2c(0x3019, 0x68);
			break;		  
		case 8://	0.7EV	 
			write_i2c(0x3018, 0x90);
			write_i2c(0x3019, 0x70);
			break;		
		case 9://	1.0EV	  
			write_i2c(0x3018, 0x98);
			write_i2c(0x3019, 0x78);
			break;		
		case 10://	1.3EV	
			write_i2c(0x3018, 0xa0);
			write_i2c(0x3019, 0x80);
			break;
		case 11://  	1.7EV	  
			write_i2c(0x3018, 0xa8);
			write_i2c(0x3019, 0x88);
			break;		
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetExposureHistogram\n")));
	return TRUE;
}

BOOL ModuleSetMirrorAndFlip(ULONG mode)
{
	RETAILMSG(MSG_MODULE,(TEXT("+ModuleSetMirrorAndFlip\n")));
/*
	switch(mode)
	{
		case 1://MIRROR
			write_i2c(0x307c, 0x12);//mirror
			write_i2c(0x3090, 0xc8);
			write_i2c(0x3023, 0x0a);
			break;
		case 2://FLIP
			write_i2c(0x307c, 0x11);//flip
			write_i2c(0x3023, 0x09);
			write_i2c(0x3090, 0xc0);			
			break;
		case 3://MIRROR&FLIP
			write_i2c(0x307c, 0x13);//flip/mirror
			write_i2c(0x3023, 0x09);
			write_i2c(0x3090, 0xc8);			
			break;
		case 4://NORML 
			write_i2c(0x307c, 0x10);//no mirror/flip
			write_i2c(0x3090, 0xc0);
			write_i2c(0x3023, 0x0a);
			break;
		default:
			return FALSE;
	}
*/
	RETAILMSG(MSG_MODULE,(TEXT("-ModuleSetMirrorAndFlip\n")));
	return TRUE;
}
//Xenon control through STROBE pin of OV3640, STROBE will go high one-four lines period 
//at the VSYNC blanking period when active.
void ModuleStrobeFlash(BOOL enable)
{
	//UCHAR state;
	if(enable)
	{
	//	read_i2c(0x3014,&state);
	//	write_i2c(0x3014, state|0x08);//night mode
			
		//write_i2c(0x307a, 0x00);//clear bit[7]
		//write_i2c(0x307a, 0x83);//set bit[7],4 lines
	//	write_i2c(0x307a, 0x8d);//set bit[7],4 lines
	//	write_i2c(0x307a, 0x8e);//set bit[7],4 lines
	}
	else
	{
	//	read_i2c(0x3014,&state);
	//	write_i2c(0x3014, state&0xf7);//return night mode
		
		//write_i2c(0x307a, 0x00);//from 1 to 0, turn off strobe pin
	}
}


