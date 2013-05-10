#include <windows.h>
//#include <ceddk.h>
#include <nkintr.h>
#include <pm.h>
#include "omap.h"
#include "bsp_cfg.h"
#include "soc_cfg.h"
#include "sdk_i2c.h"
#include <ceddkex.h>

//------------------------------------------------------------------------------
#define PCF8591_DEVICE_COOKIE       0x11223366
#define IOCTL_PCF8591_WRITE       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0316, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_PCF8591_READ       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0317, METHOD_BUFFERED, FILE_ANY_ACCESS)

