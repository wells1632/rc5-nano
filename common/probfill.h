/* Hey, Emacs, this a -*-C++-*- file !
 *
 * Copyright distributed.net 1997-2003 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
*/ 
#ifndef __PROBFILL_H__
#define __PROBFILL_H__ "@(#)$Id: probfill.h,v 1.20 2007/10/22 16:48:27 jlawson Exp $"

#define PROBFILL_ANYCHANGED  1
#define PROBFILL_GETBUFFERRS 2
#define PROBFILL_UNLOADALL   3
#define PROBFILL_RESIZETABLE 4

#include "client.h"

unsigned int LoadSaveProblems(Client *client,
                              unsigned int load_problem_count,int mode);
/* returns number of actually loaded problems */

unsigned int ClientGetInThreshold(Client *client, int contestid, int force );

// --------------------------------------------------------------------------

/* Buffer counts obtained from ProbfillGetBufferInfo() are for 
** informational use (by frontends etc) only. Don't shortcut 
** any of the common code calls to GetBufferCount().
** Threshold info is updated by probfill, blk/swu_count is
** updated via called by GetBufferCount() [buffbase.cpp] whenever 
** both swu_count and blk_count were determined.
*/
int ProbfillGetBufferCounts( unsigned int contest, int is_out_type,
                             long *threshold, int *thresh_in_swu,
                             long *blk_count, long *swu_count, 
                             unsigned int *till_completion );

int ProbfillCacheBufferCounts( Client *client,
                               unsigned int cont_i, int is_out_type,
                               long blk_count, long swu_count);

// --------------------------------------------------------------------------

#define PROBLDR_DISCARD      0x01
#define PROBLDR_FORCEUNLOAD  0x02
extern int SetProblemLoaderFlags( const char *loaderflags_map /* 1 char per contest */ );

// --------------------------------------------------------------------------

/* the following macros are used by probfill and when scanning a buffer */
/* for "most suitable packet" */
#if 0 // finally we got rid of this insanity
#define FILEENTRY_OS         CLIENT_OS
#define FILEENTRY_BUILDHI    ((CLIENT_BUILD_FRAC >> 8) & 0xff)
#define FILEENTRY_BUILDLO    ((CLIENT_BUILD_FRAC     ) & 0xff)
#define FILEENTRY_CPU(_core_cpu,_core_sel) \
                         ((char)(((_core_cpu & 0x0f)<<4) | (_core_sel & 0x0f)))

#define FILEENTRY_CPU_TO_CPUNUM( _fe_cpu ) (( _fe_cpu >> 4 ) & 0x0f)
#define FILEENTRY_CPU_TO_CORENUM( _fe_cpu ) ( _fe_cpu & 0x0f)
#define FILEENTRY_BUILD_TO_BUILD( _fe_buildhi, _fe_buildlo ) \
                             ((((int)(_fe_buildhi))<<8) | _fe_buildlo)
#define FILEENTRY_OS_TO_OS( _fe_os ) ( _fe_os )
#else
#define FILEENTRY_OS                CLIENT_OS
#define FILEENTRY_CPU( _fe_cpu )    ( _fe_cpu )
#define FILEENTRY_BUILD             CLIENT_VERSION
#define FILEENTRY_CORE( _fe_core )  ( _fe_core )
#endif

// --------------------------------------------------------------------------

#endif /* __PROBFILL_H__ */
