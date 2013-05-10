#ifndef _CAM_MODULE_H
#define _CAM_MODULE_H

typedef struct _MODULE_DESCRIPTOR
{
    BYTE ITUXXX;            // if ITU-601 8bit, set 1. if ITU-656 8bit, set 0
    BYTE UVOffset;            // Cb, Cr value offset. 1: +128 , 0: 0
    BYTE Order422;             // 0x00:YCbYCr, 0x01:YCrYCb, 0x10:CbYCrY, 0x11:CrYCbY
    BYTE Codec;                // 422: 1   , 420: 0
    BYTE HighRst;            // Reset is    Low->High: 0   High->Low: 1 
    BYTE InvPCLK;            // 1: inverse the polarity of PCLK    0 : normal
    BYTE InvVSYNC;            // 1: inverse the polarity of VSYNC   0 : normal
    BYTE InvHREF;              // 1: inverse the polarity of HREF      0 : normal
    UINT32 ImageType;			//Image Type;
    UINT32 SourceHSize;     // Horizontal size
    UINT32 SourceVSize;        // Vertical size
    UINT32 SourceHOffset;     // Horizontal size
    UINT32 SourceVOffset;        // Vertical size    
    UINT32 Clock;            // clock
} MODULE_DESCRIPTOR;

#define CAM_ITU601                      (1)
#define CAM_ITU656                      (0)

#define CAM_ORDER_YCBYCR                (0)
#define CAM_ORDER_YCRYCB                (1)
#define CAM_ORDER_CBYCRY                (2)
#define CAM_ORDER_CRYCBY                (3)

#define	CAM_UVOFFSET_0                  (0)
#define	CAM_UVOFFSET_128                (1)

#define CAM_CODEC_422                   (1)
#define CAM_CODEC_420                   (0)


BOOL	ModuleInit();
void	ModuleDeinit();
DWORD ModuleDetect(void);
BOOL 	ModuleOpen(void);
void 	ModuleClose(void);
void	ModuleGetFormat(MODULE_DESCRIPTOR &outModuleDesc);
BOOL 	ModuleSetImage(ULONG lModeType);
BOOL 	SingleFocus(BOOL enable);

BOOL 	ModuleSetLightMode(ULONG lightMode);
BOOL 	ModuleSetSaturation(INT8 plus);
BOOL 	ModuleSetBrightness(INT8 plus);
BOOL 	ModuleSetContrast(INT8 plus);
BOOL 	ModuleSetEffects(ULONG effect);
BOOL 	ModuleSetSharpness(ULONG sharpness);
BOOL 	ModuleSetExposureAverage(ULONG level);
BOOL 	ModuleSetExposureHistogram(ULONG level);
BOOL 	ModuleSetMirrorAndFlip(ULONG mode);
void 	ModuleStrobeFlash(BOOL enable);


#endif
