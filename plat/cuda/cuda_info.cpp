/*
 * Copyright distributed.net 2009-2011, 2014 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: cuda_info.cpp,v 1.6 2014/02/23 21:07:07 zebe Exp $
*/

#include "cuda_info.h"
#include "cuda_core_count.h"
#include "cuda_setup.h"

#include <stdio.h>
#include <cuda_runtime.h>

// returns -1 if not supported
// returns 0 if no supported GPU was found
int GetNumberOfDetectedCUDAGPUs()
{
  static int gpucount = -123;

  if (gpucount == -123) {
    gpucount = -1;
    if (cuda_init_state >= 0) {
      cudaDeviceProp deviceProp;
      cudaError_t rc = cudaGetDeviceCount(&gpucount);
      if (rc != cudaSuccess) {
        gpucount = -1;
      } else {
        rc = cudaGetDeviceProperties(&deviceProp, 0); /* Query first device to be sure CUDA is working somehow */
        if (rc != cudaSuccess || (deviceProp.major == 9999 && deviceProp.minor == 9999))
          gpucount = 0;
      }
    }
  }

  return gpucount;
}

long GetRawCUDAGPUID(int device, const char **cpuname)
{
  static char namebuf[40];
  int cores;

  namebuf[0] = '\0';
  if (cpuname)
    *cpuname = &namebuf[0];

  if (device >= 0 && device < GetNumberOfDetectedCUDAGPUs()) {
    cudaDeviceProp deviceProp;
    cudaError_t rc = cudaGetDeviceProperties(&deviceProp, device);
    if (rc == cudaSuccess) {
      if ((cores = getCUDACoresPerSM (deviceProp.major, deviceProp.minor)) != -1) {
        snprintf(namebuf, sizeof(namebuf), "%.29s (%d SPs)",
          deviceProp.name, deviceProp.multiProcessorCount*cores);
      } else {
        snprintf(namebuf, sizeof(namebuf), "%.29s (%d MPs - ? SPs)",
          deviceProp.name, deviceProp.multiProcessorCount);
      }

      // FIXME: we need some ID to distinguish different cards
      // for now the register count is enough to decide whether 256-thread cores are feasible
      return deviceProp.regsPerBlock;
    }
  }
  return -1;
}

// returns the frequency in MHz, or 0.
unsigned int GetCUDAGPUFrequency(int device)
{
  unsigned int freq = 0;
  if (device >= 0 && device < GetNumberOfDetectedCUDAGPUs()) {
    cudaDeviceProp deviceProp;
    cudaError_t rc = cudaGetDeviceProperties(&deviceProp, device);
    if (rc == cudaSuccess)
      freq = deviceProp.clockRate / 1000;
  }
  return freq;
}
