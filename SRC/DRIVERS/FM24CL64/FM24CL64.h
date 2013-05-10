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
#define FM24CL64_DEVICE_COOKIE       0x11223377
#define IOCTL_FM24CL64_WRITE       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0318, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define IOCTL_FM24CL64_READ       \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0319, METHOD_BUFFERED, FILE_ANY_ACCESS)

