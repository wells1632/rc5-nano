/* Hey, Emacs, this a -*-C++-*- file !
 *
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
*/
#ifndef __SELCORE_H__
#define __SELCORE_H__ "@(#)$Id: selcore.h,v 1.25 2012/01/13 01:05:22 snikkel Exp $"

#include "client.h"
#include "cputypes.h"
#include "ccoreio.h"
#if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
#include "ogr.h"
#endif



/* ---------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

// Definitions of core prototypes for each project.
#if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
typedef CoreDispatchTable *ogr_func;
#endif
typedef s32 CDECL gen_72_func ( RC5_72UnitWork *, u32 *, void * );


typedef union
{
  /* OGR */
  #if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
  CoreDispatchTable *ogr;
  #endif

  /* generic prototype: RC5-72 */
  s32 CDECL (*gen_72)( RC5_72UnitWork *, u32 *iterations, void *memblk );

  #if 0
  PROJECT_NOT_HANDLED("in unit_func_union");
  #endif
} unit_func_union;

#ifdef __cplusplus
}
#endif


struct selcore
{
  int client_cpu;
  int pipeline_count;
  int use_generic_proto;
  int cruncher_is_asynchronous;
  unit_func_union unit_func;
};



/* ---------------------------------------------------------------------- */

/* Set the xx_unit_func vectors/cputype/coresel in the problem. */
/* Returns core # or <0 if error. Called from Prob::LoadState and probfill */
int selcoreSelectCore( Client *client, unsigned int cont_id, unsigned int thrindex,
                       int *client_cpuP, struct selcore *selinfo );

int selcoreGetPreselectedCoreForProject(unsigned int projectid, int device);
/* Get the core # for a contest. Informational use only. */
int selcoreGetSelectedCoreForContest( Client *client, unsigned int contestid );
const char *selcoreGetDisplayName( unsigned int cont_i, int index );
const char **corenames_for_contest( unsigned int cont_i );
unsigned int corecount_for_contest( unsigned int cont_i );
unsigned int nominal_rate_for_contest( unsigned int cont_i);

/* conf calls these */
int selcoreValidateCoreIndex( unsigned int cont_i, int index, Client *client );
void selcoreEnumerate( int (*enumcoresproc)(unsigned int cont,
                                            const char *corename, int idx, void *udata, Client *client ),
                       void *userdata, Client *client );
void selcoreEnumerateWide( int (*enumcoresproc)(
                             const char **corenames, int idx, void *udata ),
                           void *userdata );

/* benchmark/test each core - return < 0 on error, 0 = not supported, > 0=ok */
long selcoreBenchmark( Client *client, unsigned int cont_i, unsigned int secs, int corenum );
long selcoreSelfTest( Client *client, unsigned int cont_i, int corenum );
long selcoreStressTest( Client *client, unsigned int cont_i, int corenum );

/* ClientMain() calls these */
int InitializeCoreTable( int *coretypes );
int DeinitializeCoreTable( void );

// Get GPU device index from devicenum and threadindex
int hackGetUsedDeviceIndex( Client *client, unsigned threadindex );

/* ---------------------------------------------------------------------- */

/* The following are all for internal use by selcore.cpp functions,
 * but are the basic functions defined by each of the project core
 * files.  All of these functions are available generically through
 * via one of the functions above that take a projectid as an
 * argument.
 */

#ifdef HAVE_RC5_72_CORES
int InitializeCoreTable_rc572(int first_time);
void DeinitializeCoreTable_rc572();
const char **corenames_for_contest_rc572();
int apply_selcore_substitution_rules_rc572(int cindex, int device);
int selcoreGetPreselectedCoreForProject_rc572(int device);
int selcoreSelectCore_rc572( Client *client, unsigned int threadindex,
                             int *client_cpuP, struct selcore *selinfo );
unsigned int estimate_nominal_rate_rc572();

#endif
#if defined(HAVE_OGR_PASS2)
int InitializeCoreTable_ogr(int first_time);
void DeinitializeCoreTable_ogr();
const char **corenames_for_contest_ogr();
int apply_selcore_substitution_rules_ogr(int cindex);
int selcoreGetPreselectedCoreForProject_ogr();
int selcoreSelectCore_ogr( Client *client, unsigned int threadindex, 
                           int *client_cpuP,
                           struct selcore *selinfo, unsigned int contestid );
unsigned int estimate_nominal_rate_ogr();

#endif
#if defined(HAVE_OGR_CORES)
int InitializeCoreTable_ogr_ng(int first_time);
void DeinitializeCoreTable_ogr_ng();
const char **corenames_for_contest_ogr_ng();
int apply_selcore_substitution_rules_ogr_ng(int cindex);
int selcoreGetPreselectedCoreForProject_ogr_ng();
int selcoreSelectCore_ogr_ng( Client *client, unsigned int threadindex, 
                              int *client_cpuP,
                              struct selcore *selinfo, unsigned int contestid );
#endif

/* ---------------------------------------------------------------------- */

#endif /* __SELCORE_H__ */
