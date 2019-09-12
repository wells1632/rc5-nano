/*
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * ----------------------------------------------------------------------
 * This file contains functions for obtaining contest constants (name, id,
 * iteration-to-keycount-multiplication-factor) or obtaining/adding to
 * contest summary data (totalblocks, totaliterations, totaltime).
 * The data itself is hidden from other modules to protect integrity and
 * ease maintenance.
 * ----------------------------------------------------------------------
*/ 
const char *clicdata_cpp(void) {
return "@(#)$Id: clicdata.cpp,v 1.45 2012/05/13 09:32:54 stream Exp $"; }

//#define TRACE

#include "baseincs.h" // for timeval
#include "projdata.h" // general project data: ids, flags, states; names, ...
#include "clitime.h"  // required for CliTimerDiff() and CliClock()
#include "bench.h"    // for TBenchmark
#include "selcore.h"  // for selcoreGetSelectedCoreForContest()
#include "util.h"     // TRACE_OUT

/* ------------------------------------------------------------------------ */

/* all the static information has been moved to projdata.h/.cpp and has been
   extended  with flags etc. to be shared with the proxynet source */
static struct contestInfo
{
#if 0 // now in projdata.h/.cpp
  const char *ContestName;
  const char *UnitName;
#endif
  int ContestID;
  unsigned int Iter2KeyFactor; /* by how much must iterations/keysdone
                        be multiplied to get the number of keys checked. */
  /* FIXME: is Iter2KeyFactor still used anywhere? move it to projdata if needed */
  unsigned int BlocksDone;
  struct { u32 hi, lo; } IterDone, TotalIterDone;
  struct timeval TimeDone;
  unsigned int UnitsDone;
  unsigned int BestTime;  /* in seconds */
  int BestTimeWasForced;
} conStats[] = {  { /*"RC5",    "keys", */ RC5,     1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"DES",    "keys", */ DES,     2, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"OGR",    "nodes",*/ OGR,     1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"CSC",    "keys", */ CSC,     1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"OGR_NG", "nodes",*/ OGR_NG,  1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"RC5-72", "keys", */ RC5_72,  1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /*"OGR-P2", "nodes",*/ OGR_P2,  1, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 },
                  { /* NULL,    NULL,   */ -1,      0, 0, {0,0}, {0,0}, {0,0}, 0, 0, 0 }  };
// obsolete projects may be omitted
#if (CONTEST_COUNT != 7)
  #error PROJECT_NOT_HANDLED("conStats[]: static initializer expects CONTEST_COUNT == 7")
#endif

/* ----------------------------------------------------------------------- */

// Reset benchmark infos to allow for accurate results in case the client
// gets restarted with new settings.
void CliResetStaticVars(void)
{
  int contest;
  for (contest = 0; contest < CONTEST_COUNT; contest++) {
    conStats[contest].BestTime = 0;
    conStats[contest].BestTimeWasForced = 0;
  }
}

/* ----------------------------------------------------------------------- */

static struct contestInfo *__internalCliGetContestInfoVectorForID( int contestid )
{
  for (int i = 0; conStats[i].ContestID >= 0; i++)
  {
    if (conStats[i].ContestID == contestid)
      return (&conStats[i]);
  }
  return ((struct contestInfo *)(NULL));
}

// ---------------------------------------------------------------------------

#if 0

FIXME this function doesn't do what it's name/comments say FIXME

// obtain the contestID for a contest identified by name.
// returns -1 if invalid name (contest not found).
int CliGetContestIDFromName( char *name )
{
  for (int i = 0; conStats[i].ContestName != NULL; i++)
  {
    int n;
    for (n = 0; conStats[i].ContestName[n] != 0; n++)
    {
      if (conStats[i].ContestName[n] != name[n])
        // FIXME this aborts too early
        return -1;
    }
    if (!name[n])
      // FIXME this returns an useless index - use conStats[i].ContestID instead
      return i;
  }
  return -1;
}
#endif

// ---------------------------------------------------------------------------

// returns the expected time to complete a work unit, in seconds
// if force is true, then a microbenchmark will be done to get the
// rate if no work on this contest has been completed yet.
int CliGetContestWorkUnitSpeed( Client *client, int contestid, int force, int *was_forced)
{
  TRACE_OUT((+1, "CliGetContestWorkUnitSpeed project=%d force=%d\n", contestid, force));
  struct contestInfo *conInfo =
                       __internalCliGetContestInfoVectorForID( contestid );
  if (conInfo)
  {
    if ((conInfo->BestTime == 0) && force)
    {
      u32 benchratehi, benchratelo;

      // This may trigger a mini-benchmark, which will get the speed
      // we need and not waste time.
      selcoreGetSelectedCoreForContest( client, contestid );

      if (conInfo->BestTime == 0)
      {
        BenchGetBestRate(client, contestid, &benchratehi, &benchratelo);
        if (benchratehi || benchratelo)
          ProjectSetSpeed(contestid, benchratehi, benchratelo);
      }
      if (conInfo->BestTime == 0)
      {
        long status;
        status = TBenchmark(client, contestid, 2, TBENCHMARK_QUIET | TBENCHMARK_IGNBRK, &benchratehi, &benchratelo);
        if (status > 0)
          ProjectSetSpeed(contestid, benchratehi, benchratelo);
      }
      if (conInfo->BestTime)
        conInfo->BestTimeWasForced = 1;
    }
    if (was_forced) /* ever (besttime was via benchmark) */
      *was_forced = conInfo->BestTimeWasForced;
    TRACE_OUT((-1, "CliGetContestWorkUnitSpeed => %d\n", conInfo->BestTime));
    return conInfo->BestTime;
  }
  TRACE_OUT((-1, "CliGetContestWorkUnitSpeed => 0\n"));
  return 0;  
}

// ---------------------------------------------------------------------------

// set new record speed for a contest, returns !0 on success
// XXX we only call this function once, from problem.cpp line 2362
//     and we don't check it's return value.  should we convert this
//     to void, or handle a failed call in problem.cpp, or what?
int CliSetContestWorkUnitSpeed( int contestid, unsigned int sec)
{
  struct contestInfo *conInfo =
                       __internalCliGetContestInfoVectorForID( contestid );
  if (conInfo && sec)
  {
    if ((conInfo->BestTime == 0) || (conInfo->BestTime > sec))
    {
      conInfo->BestTime = sec;
      return 1; /* success */
    }
  }  
  return 0; /* failed */
}
    
// ---------------------------------------------------------------------------

#if 0 // unused ???
// obtain constant data for a contest. name/iter2key may be NULL
// returns 0 if success, !0 if error (bad contestID).
int CliGetContestInfoBaseData( int contestid, const char **name, unsigned int *iter2key )
{
  struct contestInfo *conInfo =
                       __internalCliGetContestInfoVectorForID( contestid );
  if (!conInfo)
    return -1;
  if (name)     *name = conInfo->ContestName;
  if (iter2key) *iter2key = (conInfo->Iter2KeyFactor<=1)?(1):(conInfo->Iter2KeyFactor);
  return 0;
}
#endif

// ---------------------------------------------------------------------------

// reset the contest summary data for a contest
int CliClearContestInfoSummaryData( int contestid )
{
  struct contestInfo *conInfo =
                      __internalCliGetContestInfoVectorForID( contestid );
  if (!conInfo)
    return -1;
  conInfo->BlocksDone = 0;
  conInfo->IterDone.hi = conInfo->IterDone.lo = 0;
  conInfo->TotalIterDone.hi = conInfo->TotalIterDone.lo = 0;
  conInfo->TimeDone.tv_sec = conInfo->TimeDone.tv_usec = 0;
  conInfo->UnitsDone = 0;
  conInfo->BestTime = 0;
  conInfo->BestTimeWasForced = 0;
  return 0;
}  

// ---------------------------------------------------------------------------

// obtain summary data for a contest. unrequired args may be NULL
// returns 0 if success, !0 if error (bad contestID).
int CliGetContestInfoSummaryData( int contestid, unsigned int *totalblocks,
                                  u32 *doneiterhi, u32 *doneiterlo,
                                  struct timeval *totaltime, 
                                  unsigned int *totalunits )
{
  struct contestInfo *conInfo =
                      __internalCliGetContestInfoVectorForID( contestid );
  if (!conInfo)
    return -1;
  if (totalblocks) *totalblocks = conInfo->BlocksDone;
  if (doneiterhi)  *doneiterhi  = conInfo->IterDone.hi;
  if (doneiterlo)  *doneiterlo  = conInfo->IterDone.lo;
  if (totalunits)  *totalunits  = conInfo->UnitsDone;
  if (totaltime)
  {
    struct timeval tv;
    tv.tv_sec = conInfo->TimeDone.tv_sec;
    tv.tv_usec = conInfo->TimeDone.tv_usec;
    if (conInfo->BlocksDone > 1)
    {
      //get time since first call to CliTimer() (time when 1st prob started)
      struct timeval tv2;
      CliClock(&tv2);
      if (tv2.tv_sec < tv.tv_sec) 
      {   //no overlap means non-mt or only single thread
        tv.tv_sec = tv2.tv_sec;
        tv.tv_usec = tv2.tv_usec;
      }
    }
    totaltime->tv_sec = tv.tv_sec;
    totaltime->tv_usec = tv.tv_usec;
  }
  return 0;
}

// ---------------------------------------------------------------------------

// add data to the summary data for a contest.
// returns 0 if added successfully, !0 if error (bad contestID).
int CliAddContestInfoSummaryData( int contestid, 
                                  u32 iter_hi, u32 iter_lo, 
                                  const struct timeval *addtime,
                                  unsigned int addunits )

{
  struct contestInfo *conInfo =
                       __internalCliGetContestInfoVectorForID( contestid );
  if (!conInfo || !addtime)
    return -1;
  conInfo->BlocksDone++;
  iter_lo += conInfo->IterDone.lo;
  if (iter_lo < conInfo->IterDone.lo)
    conInfo->IterDone.hi++;
  conInfo->IterDone.lo  = iter_lo;
  conInfo->IterDone.hi += iter_hi;
  conInfo->UnitsDone += addunits;
  conInfo->TimeDone.tv_sec += addtime->tv_sec;
  conInfo->TimeDone.tv_usec += addtime->tv_usec;
  if (conInfo->TimeDone.tv_usec > 1000000L)
  {
    conInfo->TimeDone.tv_sec += (conInfo->TimeDone.tv_usec / 1000000L);
    conInfo->TimeDone.tv_usec %= 1000000L;
  }
  return 0;
}

// ---------------------------------------------------------------------------

#if 0 // unused
// return 0 if contestID is invalid, non-zero if valid.
int CliIsContestIDValid(int contestid)
{
  return (__internalCliGetContestInfoVectorForID(contestid) != NULL);
}
#endif

// ---------------------------------------------------------------------------

// Return a usable contest name.
const char *CliGetContestNameFromID(int contestid)
{
  return ProjectGetName(contestid);
}

// ---------------------------------------------------------------------------

// Return a usable contest unit name.
const char *CliGetContestUnitFromID(int contestid)
{
  return ProjectGetUnitName(contestid);
}

// ---------------------------------------------------------------------------
