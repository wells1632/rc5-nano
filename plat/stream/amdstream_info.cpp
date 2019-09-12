/*
 * Copyright 2008 Vyacheslav Chupyatov <goteam@mail.ru>
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Special thanks for help in testing this core to:
 * Alexander Kamashev, PanAm, Alexei Chupyatov
 *
 * $Id: amdstream_info.cpp,v 1.14 2012/02/02 18:58:58 sla Exp $
*/

#include "amdstream_info.h"
#include "amdstream_context.h"
#include <stdlib.h>
#include <string.h>

#include "logstuff.h"

unsigned getAMDStreamDeviceFreq(int device)
{
  stream_context_t *cont = stream_get_context(device);

  if (cont)
    return cont->attribs.engineClock;

  return 0;
}

static const char* GetNameById(u32 id, u32 nSIMDs=0)
{
  switch (id)
  {
  case CAL_TARGET_600: return "HD2900";
  case CAL_TARGET_610: return "HD2400";
  case CAL_TARGET_630: return "HD2600";
  case CAL_TARGET_670: return "HD3870/HD3850/HD3690";
  case CAL_TARGET_7XX: return "R700 class";
  case CAL_TARGET_770: 
    if(nSIMDs==8)  return "HD4830/HD4860";
    if(nSIMDs==10) return "HD4850/HD4870";
    return "HD48xx";
  case CAL_TARGET_710: return "HD43xx/HD45xx";
  case CAL_TARGET_730: return "HD4650/HD4670";
//  case CAL_TARGET_740: return "HD4770/HD4750";
  case CAL_TARGET_CYPRESS: 
    if(nSIMDs==20) return "HD5870/HD5970";
    if(nSIMDs==18) return "HD5850";
    if(nSIMDs==14) return "HD5830";
    return "HD58xx";
  case CAL_TARGET_JUNIPER:
    if(nSIMDs==10) return "HD5770";
    if(nSIMDs==9)  return "HD5750/HD6770";
    return "HD57xx";
  case CAL_TARGET_REDWOOD:
    if(nSIMDs==5)  return "HD5670/HD5570";
    if(nSIMDs==4)  return "HD5550";
    return "HD56xx/HD55xx";
  case CAL_TARGET_CEDAR: return "HD54xx";
  case CAL_TARGET_BARTS:   
    if(nSIMDs==12) return "HD6850";
    if(nSIMDs==14) return "HD6870";
    return "HD68xx";
  case CAL_TARGET_CAYMAN: 
    if(nSIMDs==24) return "HD6970";
    if(nSIMDs==22) return "HD6950";
	return "HD69xx";
  case 20:			//TODO: CAL_TARGET_TAHITI
    if(nSIMDs==32) return "HD7970";
    if(nSIMDs==28) return "HD7950";
	return "HD79xx";
  default:         return "unknown";
  }
}

long getAMDStreamRawProcessorID(int device, const char **cpuname)
{
  stream_context_t *dev = stream_get_context(device);

  if (dev)
  {
    *cpuname = GetNameById(dev->attribs.target, dev->attribs.numberOfSIMD);
    return dev->attribs.target;
  }
  else
  {
    *cpuname = "???";
    return 0;
  }
}

void AMDStreamPrintExtendedGpuInfo(int device)
{
  stream_context_t *dev = stream_get_context(device);
  if (dev == NULL)
    return;

  LogRaw("\n");
  if (!dev->active)
  {
    LogRaw("Warning: device %d not activated\n", device);
  }
  else
  {
    LogRaw("GPU %d attributes (EEEEEEEE == undefined):\n", device);
#ifdef __GNUG__
#define sh(name) LogRaw("%24s: %08X (%d)\n", #name, dev->attribs.name, dev->attribs.name)
#else
#define sh(name) LogRaw("%24s: %08X (%d)\n", #name, dev->attribs.##name, dev->attribs.##name)
#endif
    sh(target);
    sh(targetRevision);
    sh(localRAM);
    sh(uncachedRemoteRAM);
    sh(cachedRemoteRAM);
    sh(engineClock);
    sh(memoryClock);
    sh(wavefrontSize);
    sh(numberOfSIMD);
    sh(doublePrecision);
    sh(localDataShare);
    sh(globalDataShare);
    sh(globalGPR);
    sh(computeShader);
    sh(memExport);
    sh(pitch_alignment);
    sh(surface_alignment);
    sh(numberOfUAVs);
    sh(bUAVMemExport);
    sh(b3dProgramGrid);
    sh(numberOfShaderEngines);
#undef sh
  }
}
