
#ifndef _SENSOR_CTRL_H
#define _SENSOR_CTRL_H

#include "ispreg.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ULONG ulReadAddr;
    ULONG ulReadOffset;
    ULONG ulWriteAddr;
    ULONG ulInputImageWidth;
    ULONG ulInputImageHeight;
    ULONG ulOutputImageWidth;
    ULONG ulOutputImageHeight;
    ULONG h_startphase;
    ULONG v_startphase;
    ULONG h_resz;
    ULONG v_resz;
    ULONG algo;
    ULONG width;
    ULONG height;
    ULONG cropTop;
    ULONG cropLeft;
    ULONG cropHeight;
    ULONG cropWidth;
    BOOL  bReadFromMemory;
    BOOL  enableZoom;
}RSZParams_t;

class CIspCtrl
{
public:
    
    CIspCtrl();
    ~CIspCtrl();
    
    BOOL 					InitializeCamera(void);    
    BOOL 					EnableCamera(ULONG lModeType,ULONG width,ULONG height);
	BOOL 					PauseCamera(void);
    BOOL 					DisableCamera(void);
    CAM_ISP_CONFIG_REGS* 	GetIspCfgRegs(void) {return m_pIspConfigRegs ;}
    CAM_ISP_CCDC_REGS*  	GetCCDCRegs(void) {return m_pCCDCRegs ;}
    LPVOID              	GetFrameBuffer(void) {return m_pYUVVirtualAddr;}
    BOOL                	ChangeFrameBuffer(ULONG ulVirtAddr);
private:
    CAM_ISP_CONFIG_REGS     *m_pIspConfigRegs;
    CAM_ISP_CCDC_REGS       *m_pCCDCRegs;
	RSZParams_t          	m_rszParams;
	HANDLE               	m_hRootBus;				//operate bus
	
    LPVOID                  m_pYUVVirtualAddr;      //YUV virtual address
    LPVOID                  m_pYUVPhysicalAddr;     //YUV physical address
    LPVOID                  m_pYUVDMAAddr;          //YUV DMA address    
	ULONG              		m_ulFilledBuffer;

	DWORD					m_sensorState;			//0:disabled 1:enabled 2:paused
	HANDLE            		m_hGPIO;
	DWORD             		m_dwCamGpioReset;
    DWORD             		m_dwCamGpioPwrDown;

	HANDLE					hTWL;					//2.8V,96530
	
    LPVOID GetPhysFromVirt(
        ULONG ulVirtAddr
    );
    
    BOOL MapCameraReg();

	void UnMapCameraReg();
       
    BOOL ConfigGPIO4MDC();
    
    BOOL CCDCInitCFG();
    
    BOOL ConfigOutlineOffset(
        UINT32 offset, 
        UINT8 oddeven, 
        UINT8 numlines);
        
    BOOL CCDCSetOutputAddress(
        ULONG SDA_Address);
        
    BOOL CCDCEnable(
        BOOL bEnable);	
    
    BOOL AllocBuffer();
        
    BOOL DeAllocBuffer();
        
    BOOL CCDCInit(ULONG width,ULONG height);

	BOOL CCDCConfigSize(ULONG width,ULONG height);
    
    BOOL ISPInit();
    
    BOOL ISPEnable(
        BOOL bEnable);
	BOOL ISPInterruptEnable(
		BOOL bEnable);   

    BOOL IsCCDCBusy();
    
    BOOL Check_IRQ0STATUS();
    
    BOOL CCDCInitSYNC();

	BOOL EnableRSZ(BOOL bEnableFlag);

	BOOL RSZInit(
    RSZParams_t *pRSZParams
    );

	BOOL CalculateRSZHWParams(
    RSZParams_t *pRSZParams
    );

	BOOL ConfigureRSZHW(
    RSZParams_t *pRSZParams
    );

	BOOL SensorPowerUpSequence();

	BOOL SensorPowerDownSequence();
	
  
};

#ifdef __cplusplus
}
#endif

#endif