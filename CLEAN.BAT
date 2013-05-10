@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this sample source code is subject to the terms of the Microsoft
@REM license agreement under which you licensed this sample source code. If
@REM you did not accept the terms of the license agreement, you are not
@REM authorized to use this sample source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the LICENSE.RTF on your install media or the root of your tools installation.
@REM THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
@REM
@REM
@REM (C) Copyright 2006 Marvell International Ltd.
@REM All Rights Reserved
@REM
REM INTEL CONFIDENTIAL
REM Copyright 2000-2003 Intel Corporation All Rights Reserved.
REM
REM The source code contained or described herein and all documents
REM related to the source code (Material) are owned by Intel Corporation
REM or its suppliers or licensors.  Title to the Material remains with
REM Intel Corporation or its suppliers and licensors. The Material contains
REM trade secrets and proprietary and confidential information of Intel
REM or its suppliers and licensors. The Material is protected by worldwide
REM copyright and trade secret laws and treaty provisions. No part of the
REM Material may be used, copied, reproduced, modified, published, uploaded,
REM posted, transmitted, distributed, or disclosed in any way without Intel’s
REM prior express written permission.

REM No license under any patent, copyright, trade secret or other intellectual
REM property right is granted to or conferred upon you by disclosure or
REM delivery of the Materials, either expressly, by implication, inducement,
REM estoppel or otherwise. Any license under such intellectual property rights
REM must be express and approved by Intel in writing.
REM
REM  Copyright ?2003 Intel Corp.
REM
REM
REM  filename: clean.bat
REM   - Removes output file directories, which will be recreated during build
REM   - Also removes some individual file types
REM   - Must be run from platform\bvdmain directory


del /s /q build.*
del /s /q *.bif
del /s /q vssver.scc
del /s /q sources.gen
del /s /q *.*~
del /s /q *~

rd /s /q target
rd /s /q lib
rd /s /q cesysgen\files
del /s /q files\uldr.nb0

for  /R %%I  in (.) do (
        if /I obj==%%~nI  rd /s /q   %%~fI

)