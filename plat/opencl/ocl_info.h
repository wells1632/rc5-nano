 /*
 * Copyright 2008-2014 Vyacheslav Chupyatov <goteam@mail.ru>
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Special thanks for help in testing this core to:
 * Alexander Kamashev, PanAm, Alexei Chupyatov
 *
 * $Id: ocl_info.h 2014/08/19 22:18:25 gkhanna Exp $
*/

#ifndef OCL_INFO_H
#define OCL_INFO_H

#include "cputypes.h"
#if (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || \
    (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_LINUX) || \
    (CLIENT_OS == OS_ANDROID)
#include <CL/cl.h>
#elif (CLIENT_OS == OS_MACOSX)
#include <OpenCL/opencl.h>
#endif

int     getOpenCLDeviceCount(void);
u32     getOpenCLDeviceFreq(int device);
long    getOpenCLRawProcessorID(int device, const char **cpuname);
void    OpenCLPrintExtendedGpuInfo(int device);

#endif // OCL_INFO_H
