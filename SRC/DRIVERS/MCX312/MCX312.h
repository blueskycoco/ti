#include <windows.h>
//#include <ceddk.h>
#include <nkintr.h>
#include <pm.h>
#include "omap.h"
#include "bsp_cfg.h"
#include "omap_gpmc_regs.h"
#include "omap_prcm_regs.h"
#include <ceddkex.h>
//#include "sdk_gpio.h"
//#include "gpio_ioctls.h"
DEFINE_GUID(
    DEVICE_IFC_MCX312_GUID, 0xa0272611, 0xdea0, 0x4678,
    0xae, 0x62, 0x65, 0x61, 0x5b, 0x7d, 0x53, 0xaa
);

//------------------------------------------------------------------------------
#define MCX312_DEVICE_COOKIE       0x11223355
#define IOCTL_MCX312_WRITE       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0314, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_MCX312_READ       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0315, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IO_CTL_START_BUTTON		0x01
#define IO_CTL_MCX312_WRITE      0x02
#define IO_CTL_MCX312_READ       0x03
#define IO_CTL_BEEP_SET         0x04

#define wr0  0x0
#define wr1  0x2
#define wr2  0x4
#define wr3  0x6
#define wr4  0x8
#define wr5  0xa
#define wr6  0xc
#define wr7  0xe
#define rr0  0x0
#define rr1  0x2
#define rr2  0x4
#define rr3  0x6
#define rr4  0x8
#define rr5  0xa
#define rr6  0xc
#define rr7  0xe
#define bp1p  0x4
#define bp1m 0x6
#define bp2p 0x8
#define bp2m 0xa
