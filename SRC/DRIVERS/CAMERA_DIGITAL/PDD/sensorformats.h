//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#ifndef __SENSORFORMATS_H
#define __SENSORFORMATS_H

#ifndef mmioFOURCC    
#define mmioFOURCC( ch0, ch1, ch2, ch3 )          \
     ( (DWORD)(BYTE)(ch0) | ( (DWORD)(BYTE)(ch1) << 8 ) |  \
     ( (DWORD)(BYTE)(ch2) << 16 ) | ( (DWORD)(BYTE)(ch3) << 24 ) )
#endif  

#define BITRATE(DX,DY,DBITCOUNT,FRAMERATE)    (DX * abs(DY) * DBITCOUNT * FRAMERATE)
#define SAMPLESIZE(DX,DY,DBITCOUNT) (DX * abs(DY) * DBITCOUNT / 8)


#define REFTIME_30FPS 333333
#define REFTIME_15FPS 666666
#define REFTIME_3FPS  3333333

//
// FourCC of the YUV formats
// For information about FourCC, go to:
//     http://www.webartz.com/fourcc/indexyuv.htm
//     http://www.fourcc.org
//

#define FOURCC_UYVY     mmioFOURCC('U', 'Y', 'V', 'Y')  // MSYUV: 1394 conferencing camera 4:4:4 mode 1 and 3
#define FOURCC_YUY2     mmioFOURCC('Y', 'U', 'Y', '2')
#define FOURCC_YV12     mmioFOURCC('Y', 'V', '1', '2')
#define FOURCC_JPEG     mmioFOURCC('I', 'J', 'P', 'G')

#define MEDIASUBTYPE_RGB565 {0xe436eb7b, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}
#define MEDIASUBTYPE_RGB555 {0xe436eb7c, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}
#define MEDIASUBTYPE_RGB24  {0xe436eb7d, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}
#define MEDIASUBTYPE_RGB32  {0xe436eb7e, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70}

#define MEDIASUBTYPE_YV12   {0x32315659, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define MEDIASUBTYPE_YUY2   {0x32595559, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
#define MEDIASUBTYPE_UYVY   {0x59565955, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

#define MEDIASUBTYPE_IJPG   {0x47504A49, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}

#define MAKE_STREAM_MODE_UYVY(StreamModeName, DX, DY, DBITCOUNT, FRAMERATE) \
    CS_DATARANGE_VIDEO StreamModeName =  \
    { \
        {    \
            sizeof (CS_DATARANGE_VIDEO),     \
            0, \
            SAMPLESIZE(DX,DY,DBITCOUNT),     \
            0,                               \
            STATIC_CSDATAFORMAT_TYPE_VIDEO,   \
            MEDIASUBTYPE_UYVY, \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO \
        }, \
 \
        TRUE,                   \
        TRUE,                   \
        CS_VIDEOSTREAM_CAPTURE, \
        0,                      \
 \
        { \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO,  \
            CS_AnalogVideo_None, \
            DX,DY,    \
            DX,DY,    \
            DX,DY,    \
            1,        \
            1,        \
            1,        \
            1,        \
            DX, DY,   \
            DX, DY,   \
            DX,       \
            DY,       \
            0,        \
            0,        \
            0,        \
            0,        \
            REFTIME_##FRAMERATE##FPS,                      \
            REFTIME_##FRAMERATE##FPS,                      \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE) / 8,        \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE),            \
        },  \
 \
        { \
            0,0,0,0,                            \
            0,0,0,0,                            \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE), \
            0L,                                 \
            REFTIME_##FRAMERATE##FPS,                      \
            sizeof (CS_BITMAPINFOHEADER),       \
            DX,                                 \
            DY,                                 \
            1,                        \
            DBITCOUNT,                \
            FOURCC_UYVY | BI_SRCPREROTATE,      \
            SAMPLESIZE(DX,DY,DBITCOUNT), \
            0,                        \
            0,                        \
            0,                        \
            0,                        \
            0, 0, 0                   \
        } \
    };  

#define MAKE_STREAM_MODE_YV12(StreamModeName, DX, DY, DBITCOUNT, FRAMERATE) \
    CS_DATARANGE_VIDEO StreamModeName =  \
    { \
        {    \
            sizeof (CS_DATARANGE_VIDEO),     \
            0, \
            SAMPLESIZE(DX,DY,DBITCOUNT),     \
            0,                               \
            STATIC_CSDATAFORMAT_TYPE_VIDEO,   \
            MEDIASUBTYPE_YV12, \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO \
        }, \
 \
        TRUE,                   \
        TRUE,                   \
        CS_VIDEOSTREAM_CAPTURE, \
        0,                      \
 \
        { \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO,  \
            CS_AnalogVideo_None, \
            DX,DY,    \
            DX,DY,    \
            DX,DY,    \
            1,        \
            1,        \
            1,        \
            1,        \
            DX, DY,   \
            DX, DY,   \
            DX,       \
            DY,       \
            0,        \
            0,        \
            0,        \
            0,        \
            REFTIME_##FRAMERATE##FPS,                      \
            REFTIME_##FRAMERATE##FPS,                      \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE) / 8,        \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE),            \
        },  \
 \
        { \
            0,0,0,0,                            \
            0,0,0,0,                            \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE), \
            0L,                                 \
            REFTIME_##FRAMERATE##FPS,                      \
            sizeof (CS_BITMAPINFOHEADER),       \
            DX,                                 \
            DY,                                 \
            3,                        \
            DBITCOUNT,                \
            FOURCC_YV12 | BI_SRCPREROTATE,      \
            SAMPLESIZE(DX,DY,DBITCOUNT), \
            0,                        \
            0,                        \
            0,                        \
            0,                        \
            0, 0, 0                   \
        } \
    }; 

#define MAKE_STREAM_MODE_YUY2(StreamModeName, DX, DY, DBITCOUNT, FRAMERATE) \
    CS_DATARANGE_VIDEO StreamModeName =  \
    { \
        {    \
            sizeof (CS_DATARANGE_VIDEO),     \
            0, \
            SAMPLESIZE(DX,DY,DBITCOUNT),     \
            0,                               \
            STATIC_CSDATAFORMAT_TYPE_VIDEO,   \
            MEDIASUBTYPE_YUY2, \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO \
        }, \
 \
        TRUE,                   \
        TRUE,                   \
        CS_VIDEOSTREAM_CAPTURE, \
        0,                      \
 \
        { \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO,  \
            CS_AnalogVideo_None, \
            DX,DY,    \
            DX,DY,    \
            DX,DY,    \
            1,        \
            1,        \
            1,        \
            1,        \
            DX, DY,   \
            DX, DY,   \
            DX,       \
            DY,       \
            0,        \
            0,        \
            0,        \
            0,        \
            REFTIME_##FRAMERATE##FPS,                      \
            REFTIME_##FRAMERATE##FPS,                      \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE) / 8,        \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE),            \
        },  \
 \
        { \
            0,0,0,0,                            \
            0,0,0,0,                            \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE), \
            0L,                                 \
            REFTIME_##FRAMERATE##FPS,                      \
            sizeof (CS_BITMAPINFOHEADER),       \
            DX,                                 \
            DY,                                 \
            1,                        \
            DBITCOUNT,                \
            FOURCC_YUY2 | BI_SRCPREROTATE,      \
            SAMPLESIZE(DX,DY,DBITCOUNT), \
            0,                        \
            0,                        \
            0,                        \
            0,                        \
            0, 0, 0                   \
        } \
    }; 

#define MAKE_STREAM_MODE_RGB565(StreamModeName, DX, DY, DBITCOUNT, FRAMERATE) \
    CS_DATARANGE_VIDEO StreamModeName =  \
    { \
        {    \
            sizeof (CS_DATARANGE_VIDEO),     \
            0, \
            SAMPLESIZE(DX,DY,DBITCOUNT),     \
            0,                               \
            STATIC_CSDATAFORMAT_TYPE_VIDEO,   \
            MEDIASUBTYPE_RGB565, \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO \
        }, \
 \
        TRUE,                   \
        TRUE,                   \
        CS_VIDEOSTREAM_CAPTURE, \
        0,                      \
 \
        { \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO,  \
            CS_AnalogVideo_None, \
            DX,DY,    \
            DX,DY,    \
            DX,DY,    \
            1,        \
            1,        \
            1,        \
            1,        \
            DX, DY,   \
            DX, DY,   \
            DX,       \
            DY,       \
            0,        \
            0,        \
            0,        \
            0,        \
            REFTIME_##FRAMERATE##FPS,                      \
            REFTIME_##FRAMERATE##FPS,                      \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE) / 4,        \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE),            \
        },  \
 \
        { \
            0,0,0,0,                            \
            0,0,0,0,                            \
            BITRATE(DX,DY,DBITCOUNT,FRAMERATE), \
            0L,                                 \
            REFTIME_##FRAMERATE##FPS,           \
            sizeof (CS_BITMAPINFOHEADER),       \
            DX,                                 \
            DY,                                 \
            1,                                  \
            DBITCOUNT,                          \
            CS_BI_BITFIELDS,  					\
            SAMPLESIZE(DX,DY,DBITCOUNT),        \
            0,                                  \
            0,                                  \
            3,                                  \
            3,                                  \
            0xf800, 0x07e0, 0x001f              \
        } \
    }; 

#define MAKE_STREAM_MODE_JPEG(StreamModeName, DX, DY, DBITCOUNT, FRAMERATE)                   \
    CS_DATARANGE_VIDEO StreamModeName =                                                       \
    {                                                                                         \
        {                                                                                     \
            sizeof (CS_DATARANGE_VIDEO),                                                      \
            0,                                                                                \
            SAMPLESIZE(DX,DY,DBITCOUNT),                                                      \
            0,                                                                                \
            STATIC_CSDATAFORMAT_TYPE_VIDEO,                                                   \
            MEDIASUBTYPE_IJPG,                                                                \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO                                           \
        },                                                                                    \
                                                                                              \
        TRUE,                                                                                 \
        TRUE,                                                                                 \
        CS_VIDEOSTREAM_CAPTURE,                                                               \
        0,                                                                                    \
                                                                                              \
        {                                                                                     \
            STATIC_CSDATAFORMAT_SPECIFIER_VIDEOINFO,                                          \
            CS_AnalogVideo_None,                                                              \
            DX,DY,                                                                            \
            DX,DY,                                                                            \
            DX,DY,                                                                            \
            1,                                                                                \
            1,                                                                                \
            1,                                                                                \
            1,                                                                                \
            DX, DY,                                                                           \
            DX, DY,                                                                           \
            DX,                                                                               \
            DY,                                                                               \
            0,                                                                                \
            0,                                                                                \
            1,                                                                                \
            1,                                                                                \
            REFTIME_##FRAMERATE##FPS,                                                         \
            REFTIME_##FRAMERATE##FPS,                                                         \
            FRAMERATE/2,                                                                      \
            FRAMERATE,                                                                        \
        },                                                                                    \
                                                                                              \
        {                                                                                     \
            0,0,0,0,                                                                          \
            0,0,0,0,                                                                          \
            FRAMERATE,                                                                        \
            0L,                                                                               \
            REFTIME_##FRAMERATE##FPS,                                                         \
            sizeof (CS_BITMAPINFOHEADER),                                                     \
            DX,                                                                               \
            DY,                                                                               \
            1,                                                                                \
            DBITCOUNT,                                                                        \
            FOURCC_JPEG | BI_SRCPREROTATE,                                                    \
            SAMPLESIZE(DX,DY,DBITCOUNT),                                                      \
            0,                                                                                \
            0,                                                                                \
            0,                                                                                \
            0,                                                                                \
            0, 0, 0                                                                           \
        }                                                                                     \
    };

/* 000 - 100 => RGB format */
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_001, 320, 240, 16, 30);//320x240 QVGA
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_002, 352, 288, 16, 15);//352x288 CIF
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_003, 640, 480, 16, 15);//640x480  VGA
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_004, 800, 600, 16, 15);//800x600 SVGA
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_005, 1024,768, 16, 15);//1024x768 XGA
MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_006, 2048,1536,16, 15);//2048x1536 QXGA

MAKE_STREAM_MODE_RGB565(DCAM_StreamMode_007, 1600,1200,16, 15);//1600X1200 UXGA	//ftm add test
//High resolution pic can only take RGB,not YUV


/* 201 - 300 ==> YUY2 format */
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_101, 320, -240, 16, 30);//320x240 QVGA
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_102, 352, -288, 16, 30);//352x288 CIF
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_103, 640, -480, 16, 15);//640x480  VGA
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_104, 800, -600, 16, 15);//800x600 SVGA
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_105, 1024,-768, 16, 15);//1024x768 XGA
//MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_106, 2048,-1536,16, 15);//2048x1536 QXGA
MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_106, 2592,-1944,16, 15);//2048x1536 QXGA

MAKE_STREAM_MODE_YUY2(DCAM_StreamMode_107, 1600,-1200,16, 15);//1600X1200 UXGA	//ftm add test



#endif //__SENSORFORMATS_H
