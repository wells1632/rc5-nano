/*
 * Copyright distributed.net 1997-2014 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Created by Jeff Lawson and Tim Charron. Rewritten by Cyrus Patel.
*/

//#define TRACE
//#define DYN_TIMESLICE_SHOWME

const char *clirun_cpp(void) {
return "@(#)$Id: clirun.cpp,v 1.163 2015/10/24 21:01:43 stream Exp $"; }

#include "cputypes.h"  // CLIENT_OS, CLIENT_CPU
#include "baseincs.h"  // basic (even if port-specific) #includes
#include "client.h"    // Client structure
#include "problem.h"   // Problem class
#include "triggers.h"  // [Check|Raise][Pause|Exit]RequestTrigger()
#include "sleepdef.h"  // sleep(), usleep()
#include "pollsys.h"   // NonPolledSleep(), RegPollingProcedure() etc
#include "setprio.h"   // SetThreadPriority(), SetGlobalPriority()
#include "lurk.h"      // dialup object
#include "buffupd.h"   // BUFFERUPDATE_* constants
#include "buffbase.h"  // GetBufferCount()
#include "clitime.h"   // CliTimer(), Time()/(CliGetTimeString(NULL,1))
#include "logstuff.h"  // Log()/LogScreen()/LogScreenPercent()/LogFlush()
#include "clicdata.h"  // CliGetContestNameFromID()
#include "util.h"      // utilCheckIfBetaExpired(), DNETC_UNUSED_*
#include "checkpt.h"   // CHECKPOINT_[OPEN|CLOSE|REFRESH|_FREQ_[SECS|PERC]DIFF]
#include "cpucheck.h"  // GetNumberOfDetectedProcessors()
#include "probman.h"   // GetProblemPointerFromIndex()
#include "probfill.h"  // LoadSaveProblems(), FILEENTRY_xxx macros
#include "modereq.h"   // ModeReq[Set|IsSet|Run]()
#include "clievent.h"  // ClientEventSyncPost() and constants
#include "coremem.h"   // cmem_alloc(), cmem_free()
#include "bench.h"     // BenchResetStaticVars()

// --------------------------------------------------------------------------

struct __dyn_timeslice_struct
{
  unsigned int contest;
  u32 usec;              /* time */
  u32 max, min, optimal; /* ... timeslice/nodes */
};

// FIXME: allow obsolete projects to be omitted
static struct __dyn_timeslice_struct
  default_dyn_timeslice_table[CONTEST_COUNT] =  /* for preempted crunchers */
{
  {  RC5,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  DES,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR,     200000,  0x8000000,  0x00010,  0x10000 },
  {  CSC,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR_NG,  200000,  0x8000000,  0x00010,  0x10000 },
  {  RC5_72, 1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR_P2,  200000,  0x8000000,  0x00010,  0x10000 },
};
// OK!
static struct __dyn_timeslice_struct
  non_preemptive_dyn_timeslice_table[CONTEST_COUNT] = /* for co-op crunchers */
{                                  /* adjusted by ClientRun() if appropriate */
  {  RC5,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  DES,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR,     200000,  0x8000000,  0x00010,  0x10000 },
  {  CSC,    1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR_NG,  200000,  0x8000000,  0x00010,  0x10000 },
  {  RC5_72, 1000000, 0x80000000,  0x00100,  0x10000 },
  {  OGR_P2,  200000,  0x8000000,  0x00010,  0x10000 },
};

// OK!
#if (CONTEST_COUNT != 7)
  #error PROJECT_NOT_HANDLED("timeslice_tables: static initializers expect CONTEST_COUNT == 7")
#endif

// =====================================================================

struct thread_param_block
{
  #if ((CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_SUNOS))
    thread_t threadID;
  #elif (defined(_POSIX_THREADS_SUPPORTED)) //cputypes.h
    pthread_t threadID;
  #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    UINT_PTR threadID;  /* 64 bits on win64 */
  #elif (CLIENT_OS == OS_NETWARE)
    long threadID;
  #elif ((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_PS2LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS)
    long threadID;
  #elif (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
    thread_id threadID;
  #elif (CLIENT_OS == OS_OS2)
    #if defined(__EMX__)
      int threadID;
    #else
      unsigned long threadID;
    #endif
  #else
    int threadID;
  #endif
  unsigned int threadnum;
  unsigned int numthreads;
  int hasexited;
  int realthread;
  unsigned int priority;
  int refillneeded;
  int do_suspend;
  int do_exit;
  int do_refresh;
  int is_suspended;
  int is_non_preemptive_cruncher;
  unsigned long thread_data1;
  unsigned long thread_data2;
  struct __dyn_timeslice_struct *dyn_timeslice_table;
  struct __dyn_timeslice_struct rt_dyn_timeslice_table[CONTEST_COUNT];
  struct { int ixes; int usecs; } dyn_timeslice_slide[CONTEST_COUNT];
  #if (CLIENT_OS == OS_NETWARE)
  int previous_was_sleep;
  #endif
  #if defined(DYN_TIMESLICE_SHOWME)
  struct
  {
    unsigned long totaltime;
    unsigned long totalts;
    unsigned int ctr;
    unsigned int contest;
    int dispactive;
  } dyn_timeslice_showme;
  #endif
  struct thread_param_block *next;
};

// ----------------------------------------------------------------------

static void __cruncher_sleep__(struct thread_param_block * /*thrparams*/)
{
  #if (CLIENT_OS == OS_NETWARE)
    __MPKDelayThread(1000);  /* one second sleep (millisecs) */
  #else
    NonPolledSleep(1);  /* one second sleep */
  #endif
}

static int __cruncher_yield__(struct thread_param_block *thrparams)
{
  DNETC_UNUSED_PARAM(thrparams);

  /* thrparams is needed for some OSs like MacOS that have different
  ** yield calls depending on how the cruncher was created.
  */
  #if ((CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_SUNOS))
    thr_yield();
  #elif (CLIENT_OS == OS_BSDOS)
    #if defined(__ELF__)
    sched_yield();
    #else // a.out
    NonPolledUSleep( 0 ); /* yield */
    #endif
  #elif (CLIENT_OS == OS_OS2)
    DosSleep(1); /* yes, 1 not 0 */
  #elif (CLIENT_OS == OS_IRIX)
    #ifdef _irix5_
    sginap(0);
    #else       // !_irix5_
    sched_yield();
    #endif      // !_irix5_
  #elif (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16)
    w32Yield(); //not Sleep(0)!
  #elif (CLIENT_OS == OS_DOS)
    dosCliYield(); //dpmi yield
  #elif (CLIENT_OS == OS_NETWARE)
    int slow = (GetDiskIOsPending() || GetNestedInterruptLevel());
    if (slow)
      thrparams->previous_was_sleep = 1;
    else if (thrparams->previous_was_sleep)
    {
      thrparams->previous_was_sleep = 0;
      slow = 1;
    }
    if (slow)
    {
      do
      {
        __MPKDelayThread(250);
        slow = (GetDiskIOsPending() || GetNestedInterruptLevel());
        if (slow)
          thrparams->previous_was_sleep = 1;
      } while (slow && !thrparams->do_suspend && !thrparams->do_exit);
      return 250000;
    }
    thrparams->previous_was_sleep = 0;
    MPKYieldThread();
    #if 0
    int client_prio = thrparams->priority;
    if (client_prio > 9)
      client_prio = 9;
    client_prio = ((9-client_prio)>>1);
    do {
      if (GetDiskIOsPending() || GetNestedInterruptLevel())
      {
        __MPKDelayThread(250);
        return 250000;
      }
      MPKYieldThread();
    } while ((--client_prio) > 0);
    #endif
  #elif (CLIENT_OS == OS_RISCOS)
    if (riscos_in_taskwindow)
    {
      int t=read_monotonic_time();
      riscos_upcall_6();
      return (read_monotonic_time()-t)*10000;
    }
  #elif (CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_PS2LINUX) || (CLIENT_OS == OS_ANDROID)
    #if defined(HAVE_KTHREADS) /* kernel threads */
    kthread_yield();
    #elif defined(__ELF__)
    sched_yield();
    #else // a.out libc4
    NonPolledUSleep( 0 ); /* yield */
    #endif
  #elif (CLIENT_OS == OS_MACOSX) || (CLIENT_OS == OS_IOS)
    sched_yield();
  #elif (CLIENT_OS == OS_DRAGONFLY)
    sched_yield();
  #elif (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_OPENBSD)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_NETBSD)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_FREEBSD)
    /* don't use sched_yield() -
       different syscall # on 3.4,4.x */
    //sched_yield();
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_AIX)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_ULTRIX)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_DYNIX)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_HPUX)
    sched_yield();
  #elif (CLIENT_OS == OS_DEC_UNIX)
   /* don't use sched_yield() - only in libc_r */
   // sched_yield();
   NonPolledUSleep(0);
  #elif (CLIENT_OS == OS_NEXTSTEP)
    NonPolledUSleep(0);
  #elif (CLIENT_OS == OS_QNX)
    #if defined(__QNXNTO__)
    sched_yield();
    #else
    NonPolledUSleep(0);
    #endif
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_SCO)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_VMS)
    NonPolledUSleep( 0 ); /* yield */
  #elif (CLIENT_OS == OS_NETWARE6)
    NonPolledUSleep( 1 ); /* yield */
  #else
    #error where is your yield function?
    NonPolledUSleep( 0 ); /* yield */
  #endif
  return 0;
}

// ----------------------------------------------------------------------

void Go_mt( void * parm )
{
#if (CLIENT_OS == OS_AMIGAOS) && !defined(__OS3PPC__)
  /* AmigaOS provides no direct way to pass parameters to sub-tasks! */
  struct Process *thisproc = (struct Process *)FindTask(NULL);
  if (!thisproc->pr_Arguments)
  {
     #if defined(__amigaos4__)
     parm = thisproc->pr_Task.tc_UserData;
     #elif (CLIENT_CPU == CPU_68K)
     struct ThreadArgsMsg *msg;
     WaitPort(&(thisproc->pr_MsgPort));
     msg = (struct ThreadArgsMsg *)GetMsg(&(thisproc->pr_MsgPort));
     parm = msg->tp_Params;
     ReplyMsg((struct Message *)msg);
     #endif
  }
#endif

  struct thread_param_block *thrparams = (thread_param_block *)parm;
  int is_non_preemptive_cruncher = thrparams->is_non_preemptive_cruncher;
  unsigned int threadnum = thrparams->threadnum;
  Problem *thisprob = NULL;

#if (CLIENT_OS == OS_RISCOS)
  #if defined(HAVE_X86_CARD_SUPPORT)
  thisprob = GetProblemPointerFromIndex(threadnum);
  if (!thisprob) /* riscos has no real threads */
    return;      /* so this is a polled job and we can just return */
  if (thisprob->client_cpu == CPU_X86) /* running on x86card */
  {
    thisprob->Run();
    return;
  }
  #endif
#elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  if (thrparams->realthread)
  {
    #if defined(_MSC_VER) && (_MSC_VER < 1299)
    typedef DWORD DWORD_PTR;
    #endif
    DWORD_PTR LAffinity, LProcessAffinity, LSystemAffinity;
    OSVERSIONINFO osver;
    unsigned int numthreads = thrparams->numthreads;

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osver);
    if ((VER_PLATFORM_WIN32_NT == osver.dwPlatformId) && (numthreads > 1))
    {
      if (GetProcessAffinityMask(GetCurrentProcess(), &LProcessAffinity, &LSystemAffinity))
      {
        LAffinity = ((DWORD_PTR)1) << threadnum;
        if (LProcessAffinity & LAffinity)
          SetThreadAffinityMask(GetCurrentThread(), LAffinity);
      }
    }
  }
#elif (CLIENT_OS == OS_NETWARE)
  unsigned long last_restart_ticks = 0;
  #define THREAD_RESTART_INTERVAL_TICKS ((((5*60)*10)*180)/10) /* 5 minutes */
  if (thrparams->realthread)
  {
    int numcpus = GetNumberOfRegisteredProcessors();
    thrparams->threadID = GetThreadID(); /* in case we got here first */
    last_restart_ticks = GetCurrentTime(); /* in ticks */
    if (last_restart_ticks == 0)
      last_restart_ticks = 1;
    if (numcpus < 1)
      numcpus = 1;
    {
      int targetcpu = threadnum % numcpus;
      int numthreads = (int)thrparams->numthreads;
      if (numthreads < numcpus) /* fewer threads than cpus? */
        targetcpu++; /* then leave CPU 0 to netware */
      if (targetcpu == 0 && numcpus > 1) /* if sharing CPU 0 with netware */
        MPKEnterNetWare(); /* then keep the first thread bound to the kernel */
      else
      {
        MPKExitNetWare(); /* spin off into kControl */
        __MPKSetThreadAffinity( targetcpu );
        #if 0 /* not completely tested in a production environment */
        if (numcpus < 2)
          ; /* nothing */
        else if (__MPKEnableThreadPreemption() == 0)
        {
          /* use 10ms quantum. preemption rate is 20ms, which is too high */
          for (int tsinitd=0;tsinitd<CONTEST_COUNT;tsinitd++)
            thrparams->dyn_timeslice_table[tsinitd].usec = 10000;
        }
        else if (is_non_preemptive_cruncher)
        {
          // yield less if not on the primary cpu and
          // not configured for full-preemption (if the latter, then
          // the dyn_timeslice_table is already set to go whole hog)
          for (int tsinitd=0;tsinitd<CONTEST_COUNT;tsinitd++)
            thrparams->dyn_timeslice_table[tsinitd].usec *= 4;
        }
        #endif
      }
    }
  }
#elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  if (thrparams->realthread)
  {
    amigaThreadInit();
    #if (CLIENT_CPU == CPU_POWERPC) && defined(__OS3PPC__)
    /* Only necessary when using 68k for time measurement */
    for (int tsinitd=0;tsinitd<CONTEST_COUNT;tsinitd++)
    {
      switch(thrparams->dyn_timeslice_table[tsinitd].contest)
      {
        case RC5_72:
          thrparams->dyn_timeslice_table[tsinitd].usec = 8000000;
          break;
        case OGR_P2:
        case OGR_NG:
          thrparams->dyn_timeslice_table[tsinitd].usec = 4000000;
          break;
      }
    }
    #endif
  }
#endif

  if (thrparams->realthread)
  {
    TriggersSetThreadSigMask();
    SetThreadPriority( thrparams->priority ); /* 0-9 */
  }

  thrparams->hasexited = 0;
  thrparams->is_suspended = 1;
  thrparams->do_refresh = 1;

  while (!thrparams->do_exit)
  {
    int didwork = 0; /* did we work? */
    if (thrparams->do_refresh)
      thisprob = GetProblemPointerFromIndex(threadnum);
    if (thisprob == NULL || thrparams->do_suspend)
    {
//printf("run: isnull? %08x, issusp? %d\n", thisprob, thrparams->do_suspend);
      if (thisprob == NULL)  // this is a bad condition, and should not happen
        thrparams->refillneeded = 1;// ..., ie more threads than problems
      if (thrparams->realthread) // don't race in the loop
        __cruncher_sleep__(thrparams);
    }
    else if (!ProblemIsInitialized(thisprob))
    {
      thrparams->refillneeded = 1;
      // sleep to avoid busy-waiting for new work to arrive (bug #3618)
      // especially since the client does not exit if it has run out of work
      // __cruncher_yield__(thrparams) is not enough and also this needs to
      // be done independently of the thrparams->realthread setting
      __cruncher_sleep__(thrparams);
    }
    else
    {
      int run; u32 optimal_timeslice = 0;
      u32 elapsed_sec, elapsed_usec, runtime_usec;
      unsigned int contest_i = thisprob->pub_data.contest;
      u32 last_count = thisprob->pub_data.core_run_count;

      #if (!defined(DYN_TIMESLICE)) /* compile time override */
      if (is_non_preemptive_cruncher || contest_i == OGR_NG || contest_i == OGR_P2)
      #endif
      {
        if (last_count == 0) /* prob hasn't started yet */
          thisprob->pub_data.tslice = thrparams->dyn_timeslice_table[contest_i].optimal;
        optimal_timeslice = thisprob->pub_data.tslice;
      }

      elapsed_sec = thisprob->pub_data.runtime_sec;
      elapsed_usec = thisprob->pub_data.runtime_usec;
      thrparams->is_suspended = 0;
      //fprintf(stderr,"thisprob->Run()\n");
      run = ProblemRun(thisprob);
      //fprintf(stderr,"thisprob->Run() = %d\n", run);
      thrparams->is_suspended = 1;

/*
 * The code below is incorrect and has been commented out (bug #4189) because:
 * 
 * 1) ProblemRun() WILL return -1 if problem reload is in progress (old block
 * unloaded but new one is not loaded yet). This normal behavior necessary to
 * avoid race conditions.
 *
 * 2) ProblemRun() will NOT pass negative return codes from cores to upper 
 * level anyway. It will return previous return code (core_prob->priv_data.last_resultcode) 
 * instead. So this code is just useless.
 *
 * Then we need to introduce more error codes than just retval<0 :
 * 
 * CORE_ERROR_RETRY == -1  the non-fatal problem (1), no action is needed
 * CORE_ERROR_REINITIALIZE    in case we find a solution to reinitialize CUDA
 * after certain problems
 * CORE_FATAL_ERROR   there is no known way to recover, so terminate the client,
 * should be used for current CUDA cores
 * 
 * And a solution has to be found to propagate core error codes upwards. (2)
 * It mean that ProblemRun() must be seriously and VERY CAREFULLY rewritten.
 * Do not forget about race conditions.
 */

#if 0
      if (run < 0) // core failure, probably unspported system configuration, terminate
      {
        Log("Terminating: cruncher %d reported error %d\n", threadnum, run);
        thrparams->do_exit = 1;
        RaiseExitRequestTrigger();
      }
#endif

      runtime_usec = 0xfffffffful; /* assume time was bad */
      if (!thisprob->pub_data.last_runtime_is_invalid)
      {
        if (thisprob->pub_data.runtime_usec < elapsed_usec)
        {
          if (thisprob->pub_data.runtime_sec <= elapsed_sec) /* clock is bad */
            elapsed_sec = (0xfffffffful / 1000000ul) + 1; /* overflow it */
          else
          {
            elapsed_sec = (thisprob->pub_data.runtime_sec-1) - elapsed_sec;
            elapsed_usec = (thisprob->pub_data.runtime_usec+1000000ul) - elapsed_usec;
          }
        }
        else
        {
          elapsed_sec = thisprob->pub_data.runtime_sec - elapsed_sec;
          elapsed_usec = thisprob->pub_data.runtime_usec - elapsed_usec;
        }
        if (elapsed_sec <= (0xfffffffful / 1000000ul))
          runtime_usec = (elapsed_sec * 1000000ul) + elapsed_usec;
      }

      didwork = (last_count != thisprob->pub_data.core_run_count);
      if (run != RESULT_WORKING)
      {
        thrparams->refillneeded = 1;
      }

      /* non-preemptive crunchers (real threads or not) yield on every pass.
       * if no work was done then threads for preemptive OSs should also yield
       * in order to give the main thread more cputime to run/reload
      */
      if ((is_non_preemptive_cruncher) || (!didwork && thrparams->realthread))
      {
        int addrunus = __cruncher_yield__(thrparams);
        #if defined(DYN_TIMESLICE_SHOWME)
        if (runtime_usec != 0xfffffffful) /* time was valid */
        {
          if ((++thrparams->dyn_timeslice_showme.dispactive) == 1)
          {
            if (contest_i != thrparams->dyn_timeslice_showme.contest)
            {
              thrparams->dyn_timeslice_showme.totaltime = 0;
              thrparams->dyn_timeslice_showme.totalts = 0;
              thrparams->dyn_timeslice_showme.ctr = 0;
              thrparams->dyn_timeslice_showme.contest = contest_i;
            }
            thrparams->dyn_timeslice_showme.totaltime += runtime_usec;
            thrparams->dyn_timeslice_showme.totalts += thisprob->pub_data.tslice;
            thrparams->dyn_timeslice_showme.ctr++;
          }
          --thrparams->dyn_timeslice_showme.dispactive;
        }
        #endif
        if (addrunus > 0 && runtime_usec != 0xfffffffful)
          runtime_usec += addrunus;
      }

      /* fine tune the timeslice for the *next* round */
      if (optimal_timeslice != 0)
      {
        /* Note: if running on a non-preemptive OS, ogr.cpp must have been
        ** modified such that OGROPT_IGNORE_TIME_CONSTRAINT_ARG is undefined!
        */
        //if (contest_i != OGR) // OGR makes dynamic timeslicing go crazy!
        {
          optimal_timeslice = thisprob->pub_data.tslice; /* get the number done back */
        }
        if (run == RESULT_WORKING) /* timeslice/time is invalid otherwise */
        {
          if (runtime_usec != 0xfffffffful) /* not negative time or other bad thing */
          {
            #ifdef HAVE_I64
            if (contest_i == OGR_NG || contest_i == OGR_P2)
            {
              /* This calculates the optimal timeslice based on a sliding
              ** average of the reached rate. It reacts slower than the
              ** normal algorithm below, but has the advantage that it reaches
              ** a stable point.
              */
              int ixes = thrparams->dyn_timeslice_slide[contest_i].ixes;
              int usecs = thrparams->dyn_timeslice_slide[contest_i].usecs;

              ixes = (ixes*15 + thisprob->pub_data.tslice)/16;
              usecs = (usecs*15 + runtime_usec)/16;

              if (usecs)
              {
                /*if(usecs!=0)
                  printf("%5d %4d\r", thisprob->pub_data.tslice, ixes*1000/usecs);
                */
                #define TS_MULDIV(__muld,__mulr,__divr) \
                  (unsigned long)((((ui64)(__muld)) * ((ui64)(__mulr))) / \
                                 ((unsigned long)(__divr)))

                optimal_timeslice = TS_MULDIV(
                                thrparams->dyn_timeslice_table[contest_i].usec,
                                ixes, usecs );

                if (optimal_timeslice < thrparams->dyn_timeslice_table[contest_i].min)
                  optimal_timeslice = thrparams->dyn_timeslice_table[contest_i].min;
                if (optimal_timeslice > thrparams->dyn_timeslice_table[contest_i].max)
                optimal_timeslice = thrparams->dyn_timeslice_table[contest_i].max;

                thrparams->dyn_timeslice_slide[contest_i].ixes = ixes;
                thrparams->dyn_timeslice_slide[contest_i].usecs = usecs;
              }
            }
            else
            #endif
            {
              const u32 fixed_increment = thisprob->pub_data.tslice_increment_hint;
              const u32 usec = thrparams->dyn_timeslice_table[contest_i].usec;
              const u32 usec5perc = usec / 20;
              if (runtime_usec < (usec - usec5perc))
              {
#if (CLIENT_CPU == CPU_CUDA) || (CLIENT_CPU == CPU_ATI_STREAM) || (CLIENT_CPU == CPU_OPENCL)
                /*
                 * Bump timeslice very fast for GPU clients. An 1GKey GPU have optimal
                 * timeslice 15000 times higher then initial value. Original code
                 * is reaching this point too slow, performance loss becomes noticeable
                 * on small blocks.
                 */
                #define MAX_U32_TIMESLICE 0xFFFFFF00  /* I just don't like 0xFFFFFFFF. It'll be truncated to table.max anyway */
                if (runtime_usec <= usec / 4)
                {
                  /* Be careful about 4GKey+ cards, lesser values also can overflow during bump */
                  ui64 bumped_timeslice = (ui64)optimal_timeslice * usec / (runtime_usec ? runtime_usec : 1);
                  if (bumped_timeslice > MAX_U32_TIMESLICE)
                    bumped_timeslice = MAX_U32_TIMESLICE;
                  optimal_timeslice = (u32)bumped_timeslice;
                }
                else
#endif
                {
                if ((fixed_increment >= optimal_timeslice / 100) && (usec - 5 * usec5perc < runtime_usec))
                  optimal_timeslice += fixed_increment;
                else if (optimal_timeslice < 0x80000000)  /* it's max for RC5, bad things will happens after shifting this! */
                  optimal_timeslice <<= 1;
                }
                if (optimal_timeslice > thrparams->dyn_timeslice_table[contest_i].max)
                  optimal_timeslice = thrparams->dyn_timeslice_table[contest_i].max;
              }
              else if (runtime_usec > (usec + usec5perc))
              {
                optimal_timeslice -= (optimal_timeslice>>2);
                if (is_non_preemptive_cruncher)
                  optimal_timeslice >>= 2; /* fall fast, rise slow(er) */
                if (optimal_timeslice < thrparams->dyn_timeslice_table[contest_i].min)
                  optimal_timeslice = thrparams->dyn_timeslice_table[contest_i].min;
              }
              if (fixed_increment)
              {
                if (optimal_timeslice < fixed_increment)
                  optimal_timeslice = fixed_increment;
                else
                {
                  optimal_timeslice += fixed_increment / 2;
                  optimal_timeslice -= optimal_timeslice % fixed_increment;
                }
              }
            }
            thisprob->pub_data.tslice = optimal_timeslice; /* for the next round */
          }
        }
        else /* ok, we've finished. so save it */
        {
          u32 opt = thrparams->dyn_timeslice_table[contest_i].optimal;
          if (optimal_timeslice > opt)
            thrparams->dyn_timeslice_table[contest_i].optimal = optimal_timeslice;
          optimal_timeslice = 0; /* reset for the next prob */
        }
      }

    }

    if (!didwork)
    {
      thrparams->do_refresh = 1; /* we need to reload the problem */
    }
    if (!thrparams->realthread)
    {
      /* delay re-registration if we are currently paused */
      if (thrparams->do_suspend)
        EnqueuePolledProcedure( (void (*)(void *))Go_mt, parm, 0 );
      else
        RegPolledProcedure( (void (*)(void *))Go_mt, parm, NULL, 0 );
      break;
    }
    #if (CLIENT_OS == OS_NETWARE) && defined(THREAD_RESTART_INTERVAL_TICKS)
    if (last_restart_ticks != 0) /* implies thrparams->realthread */
    {
      /* restart the cruncher every 5 minutes, otherwise what happens is
         that the kernel sees the cruncher taking lots of cpu time, figures
         it needs more, and begins rescheduling it more and more often.
         Restarting the thread effectively resets the kernel counters for
         the cruncher.
      */
      unsigned long ticksnow = GetCurrentTime();
      if (ticksnow > (last_restart_ticks+THREAD_RESTART_INTERVAL_TICKS) )
      {
        int oldthrid = thrparams->threadID;
        thrparams->threadID = BeginThread( Go_mt, (void *)0,
                                 8192, (void *)thrparams );
        last_restart_ticks = ticksnow;
        if (thrparams->threadID == -1)
          thrparams->threadID = oldthrid;
        else
        {
          char threadname[64];
          sprintf(threadname, "%s crunch #%02x", utilGetAppName(),
                                               thrparams->threadnum + 1 );
          RenameThread( thrparams->threadID, threadname );
          return; /* poof, finished here, new thread takes over */
        }
      }
    }
    #endif
  }

  if (thrparams->realthread)
  {
    SetThreadPriority( 9 ); /* 0-9 */
    #if (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    /*
     * Avoid race condition.
     *
     * This is a separate process/task on AmigaOS/MorphOS, but the code
     * it executes is from the parent. If the parent exits before the child
     * does, it will unload the code while the child still executes it.
     *
     * To avoid this race, make sure rest of the child is run atomic.
     *
     */
    Forbid();
    #endif
  }

  thrparams->hasexited = 1; //the thread is dead

  #if ((CLIENT_OS == OS_SUNOS) || (CLIENT_OS == OS_SOLARIS))
  if (thrparams->realthread)
    thr_exit((void *)0);
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  if (thrparams->realthread)
    amigaThreadExit();
  #endif
}

// -----------------------------------------------------------------------

static int __StopThread( struct thread_param_block *thrparams )
{
  if (thrparams)
  {
    if (thrparams->realthread) //real thread
    {
      NonPolledUSleep(100);   // give the thread some air
      #if ((CLIENT_OS == OS_SUNOS) || (CLIENT_OS == OS_SOLARIS))
      thr_join(0, 0, NULL); //all at once
      #elif (defined(_POSIX_THREADS_SUPPORTED)) //cputypes.h
      if (!thrparams->hasexited)
        pthread_join( thrparams->threadID, (void **)NULL);
      #elif (CLIENT_OS == OS_OS2)
      DosSetPriority( 2, PRTYC_REGULAR, 0, (ULONG)thrparams->threadID); /* thread to normal prio */
      if (!thrparams->hasexited)
        DosWaitThread( (PTID)&(thrparams->threadID), DCWW_WAIT);
      #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
      while (!thrparams->hasexited)
        Sleep(100);
      #elif (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
      static status_t be_exit_value;
      if (!thrparams->hasexited)
        wait_for_thread(thrparams->threadID, &be_exit_value);
      #elif (CLIENT_OS == OS_NETWARE)
      while (!thrparams->hasexited)
        delay(100);
      #elif ((CLIENT_OS==OS_LINUX) || (CLIENT_OS == OS_PS2LINUX) \
         || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS) /*kernel threads*/
      kthread_join( thrparams->threadID );
      #elif defined(HAVE_MULTICRUNCH_VIA_FORK)
      int status = 0, elapsed = 0;
      pid_t chld = 0, pid = ((pid_t)(thrparams->threadID));
      while (chld == 0 && elapsed < (5*1000000))  /* 5 secs */
      {
        NonPolledUSleep(100000);
        elapsed += 100000;
        chld = waitpid(pid,&status,WNOHANG|WUNTRACED);
        if (chld != pid && errno == ECHILD)
          chld = pid; /* hmm! */
      }
      if (chld != pid) /* hasn't exited */
      {
        kill(pid, SIGKILL); /* its hung. kill it */
        waitpid(pid,&status,WUNTRACED);
      }
      #if 0
      {
        if (WIFEXITED(status))
          printf("child %d called _exit(%d)\n",pid, WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
        {
          int sig = WTERMSIG(status);
          printf("child %d caught %s signal %d\n",pid,(sig < NSIG ? sys_siglist[sig] : "unknown"), sig );
        }
        else if (WIFSTOPPED(status))
          printf("child %d stopped on %d signal\n",pid, WSTOPSIG(status));
        else
          printf("unknown cause for stop\n");
      }
      #endif
      #elif (CLIENT_OS == OS_FREEBSD)
      while (!thrparams->hasexited)
        NonPolledUSleep(100000);
      #elif (CLIENT_OS == OS_AMIGAOS)
      #if !defined(__OS3PPC__)
      /* Raise thread's priority so it can exit if a user process is hogging cpu time */
      Forbid();
      if (!thrparams->hasexited)
      {
        SetTaskPri((struct Task *)thrparams->threadID,0);
      }
      Permit();
      #endif
      while (!thrparams->hasexited)
        NonPolledUSleep(300000);
      #elif (CLIENT_OS == OS_MORPHOS)
      /* Make the thread run at least the same priority as ourself */
      Forbid();
      if (!thrparams->hasexited)
      {
        LONG mypri = 0, thpri = 0;
        NewGetTaskAttrsA(NULL, &mypri, sizeof(mypri), TASKINFOTYPE_PRI, TAG_DONE);
        NewGetTaskAttrsA((struct Task *) thrparams->threadID, &thpri, sizeof(thpri), TASKINFOTYPE_PRI, TAG_DONE);
        if (thpri < mypri)
          SetTaskPri((struct Task *) thrparams->threadID, mypri);
      }
      Permit();
      while (!thrparams->hasexited)
        NonPolledUSleep(100000);
      #endif
    }
    ClientEventSyncPost( CLIEVENT_CLIENT_THREADSTOPPED,
                         &(thrparams->threadnum), sizeof(thrparams->threadnum) );
    cmem_free( thrparams );
  }
  return 0;
}

// -----------------------------------------------------------------------

static struct thread_param_block *__StartThread( unsigned int thread_i,
        unsigned int numthreads, unsigned int priority, int no_realthreads,
        int is_non_preemptive_os )
{
  int success = 1, use_poll_process = 0;

  struct thread_param_block *thrparams = (struct thread_param_block *)
                 cmem_alloc( sizeof(struct thread_param_block) );
  if (thrparams)
  {
    // Start the thread for this cpu
    memset( (void *)(thrparams), 0, sizeof( struct thread_param_block ));
    thrparams->threadID = 0;              /* whatever type */
    thrparams->numthreads = numthreads;   /* unsigned int */
    thrparams->threadnum = thread_i;      /* unsigned int */
    thrparams->realthread = 1;            /* int */
    thrparams->hasexited = 1;             /* not running yet */
    thrparams->priority = priority;       /* unsigned int */
    thrparams->is_non_preemptive_cruncher = is_non_preemptive_os; /* int */
    thrparams->do_exit = 0;
    thrparams->do_suspend = 0;
    thrparams->is_suspended = 0;
    thrparams->do_refresh = 1;
    thrparams->thread_data1 = 0;          /* ulong, free for thread use */
    thrparams->thread_data2 = 0;          /* ulong, free for thread use */
    thrparams->dyn_timeslice_table = &(thrparams->rt_dyn_timeslice_table[0]);
    if (is_non_preemptive_os)
    {
      /* may be overridden on a thread-by-thread basis */
      memcpy( (void *)(thrparams->dyn_timeslice_table),
              non_preemptive_dyn_timeslice_table,
              sizeof(non_preemptive_dyn_timeslice_table));
    }
    else
    {
      memcpy( (void *)(thrparams->dyn_timeslice_table),
              default_dyn_timeslice_table,
              sizeof(default_dyn_timeslice_table));
    }
    thrparams->next = NULL;

    use_poll_process = 0;

    if ( no_realthreads )
      use_poll_process = 1;
    else
    {
      //defined in cputypes.h
      #if !defined(CLIENT_SUPPORTS_SMP)
        use_poll_process = 1; //no thread support or cores are not thread safe
      #elif defined(HAVE_MULTICRUNCH_VIA_FORK)
      use_poll_process = 1;
      if (thrparams->threadnum != 0) /* second...nth thread */
      {
        pid_t pid = fork();
        if (pid == 0) /* child */
        {
          thrparams->threadID = getpid();
          Go_mt((void *)thrparams);
          _exit(0);
        }
        success = 0;
        if (pid != -1)
        {
          thrparams->threadID = pid;
          success = 1;
        }
        use_poll_process = 0;
      }
      #elif (CLIENT_OS == OS_FREEBSD)
      //#define USE_THREADCODE_ONLY_WHEN_SMP_KERNEL_FOUND /* otherwise its for >=3.0 */
      #define FIRST_THREAD_UNDER_MAIN_CONTROL /* otherwise main is separate */

      static int ok2thread = 0; /* <0 = no, >0 = yes, 0 = unknown */
      #ifdef USE_THREADCODE_ONLY_WHEN_SMP_KERNEL_FOUND
      if (ok2thread == 0)
      {
        if (GetNumberOfDetectedProcessors()<2)
          ok2thread = -1;
        else
        {
          int issmp = 0; size_t len = sizeof(issmp);
          if (sysctlbyname("machdep.smp_active", &issmp, &len, NULL, 0)!=0)
          {
            issmp = 0; len = sizeof(issmp);
            if (sysctlbyname("smp.smp_active", &issmp, &len, NULL, 0)!=0)
              issmp = 0;
          }
          if (issmp)
            issmp = (len == sizeof(issmp));
          if (!issmp)
            ok2thread = -1;
        }
      }
      #endif
      if (ok2thread == 0)
      {
        char buffer[64];size_t len=sizeof(buffer),len2=sizeof(buffer);
        //fprintf(stderr,"sysctlbyname()\n");
        if (sysctlbyname("kern.ostype",buffer,&len,NULL,0)!=0)
          ok2thread = -2;
        else if (len<7 || memcmp(buffer,"FreeBSD",7)!=0)
          ok2thread = -3;
        else if (sysctlbyname("kern.osrelease",buffer,&len2,NULL,0)!=0)
          ok2thread = -4;
        //else if (len<3 || !isdigit(buffer[0]) || buffer[1]!='.' || !isdigit(buffer[2]))
          //ok2thread = -5;
        else
        {
          //buffer[sizeof(buffer)-1] = '\0';
          //ok2thread = ((buffer[0]-'0')*100)+((buffer[2]-'0')*10);
          
          ok2thread = (int)(100*atof(buffer));
	
          //fprintf(stderr, "found fbsd '%s' (interp ver %d)\n", buffer, ok2thread );
          if (ok2thread < 300) /* FreeBSD < 3.0 */
            ok2thread = -6;
        }
      }
      if (ok2thread<1) /* not FreeBSD or not >=3.0 (or non-SMP kernel) */
      {
        use_poll_process = 1; /* can't use this stuff */
        //fprintf(stderr,"using poll-process\n");
      }
      else
      {
        static int assertedvms = 0;
        if (thrparams->threadnum == 0) /* the first to spin up */
        {
          #define ONLY_NEEDED_VMPAGES_INHERITABLE         //children faster
          //#define ALL_DATA_VMPAGES_INHERITABLE          //parent faster
          //#define ALL_TEXT_AND_DATA_VMPAGES_INHERITABLE //like linux threads

          assertedvms = -1; //assume failed
          #if defined(ONLY_NEEDED_VMPAGES_INHERITABLE)
          {
            //children are faster, main is slower
            unsigned int prob_i, numprobs = numthreads;
            assertedvms = +1; //assume success
            for (prob_i = 0; assertedvms > 0 && prob_i < numprobs; prob_i++)
            {
              Problem *thisprob = GetProblemPointerFromIndex(prob_i);
              if (thisprob)
              {
                madvise((void *)thisprob,ProblemGetSize(),MADV_WILLNEED);
                #ifdef FIRST_THREAD_UNDER_MAIN_CONTROL
                if (prob_i != 0) /* don't need to flag first problem */
                #endif
                {
                  int mflag = 0; /*VM_INHERIT_SHARE*/ /*MAP_SHARED|MAP_INHERIT*/;
                  if (minherit((void *)thisprob,ProblemGetSize(),mflag)!=0)
                    assertedvms = -1;
                }
              }
            }
            //fprintf(stderr, "assertedvms = %d\n", assertedvms );
          }
          #else
          {
            extern _start; //iffy since crt0 is at the top only by default
            extern etext;
            extern edata;
            extern end;
            //printf(".text==(start=0x%p - etext=0x%p) .rodata+.data==(etext - edata=0x%p) "
            //  ".bss==(edata - end=0x%p) heap==(end - sbrk(0)=0x%p)\n",
            //  &_start, &etext,&edata,&end,sbrk(0));
            #if defined(ALL_TEXT_AND_DATA_VMPAGES_INHERITABLE)
            //.text+.rodata+.data+.bss+heap (so far)
            if (minherit((void *)&_start,(sbrk(0)-((char *)&_start)),0)==0)
              assertedvms = +1; //success
            #else
            //main is faster, children are slower
            //.rodata+.data+.bss+heap (so far)
            if (minherit((void *)&etext,(sbrk(0)-((char *)&etext)),0)==0)
              assertedvms = +1; //success
            #endif
          }
          #endif

          #ifdef FIRST_THREAD_UNDER_MAIN_CONTROL
          use_poll_process = 1; /* the first thread is always non-real */
          #endif
        }
        if (assertedvms != +1)
          use_poll_process = 1; /* can't use this stuff */
      }
      if (use_poll_process == 0)
      {
        thrparams->threadID = 0;
        success = 0;
        //fprintf(stderr, "beginning minherit()\n" );
        if (minherit((void *)thrparams, sizeof(struct thread_param_block),
                                  0 /* undocumented VM_INHERIT_SHARE*/)==0)
        {
          int rforkflags=RFPROC|RFTHREAD|RFSIGSHARE|RFNOWAIT/*|RFMEM*/;
          //fprintf(stderr, "beginning rfork()\n" );
          pid_t new_threadID = rfork(rforkflags);
          if (new_threadID == -1) /* failed */
            success = 0;
          else if (new_threadID == 0) /* child */
          {
            int newprio=((22*(9-thrparams->priority))+5)/10;/*scale 0-9 to 20-0*/
            thrparams->threadID = getpid(); /* may have gotten here first */
            setpriority(PRIO_PROCESS,thrparams->threadID,newprio);
            //nice(newprio);
            if ((rforkflags & RFNOWAIT) != 0) /* running under init (pid 1) */
             { seteuid(65534); setegid(65534); } /* so become nobody */
            Go_mt( (void *)thrparams );
            _exit(0);
          }
          else if ((rforkflags & RFNOWAIT) != 0)
          {    /* thread is detached (a child of init), so we can't wait() */
            int count = 0;
            while (count<100 && thrparams->threadID==0) /* set by child */
              NonPolledUSleep(1000);
            if (thrparams->threadID) /* child started ok */
              success = 1;
            else
              kill(new_threadID, SIGKILL); /* its hung. kill it */
          }
          else /* "normal" parent, so we can wait() for spinup */
          {
            int status, res;
            NonPolledUSleep(3000); /* wait for fork1() */
            res = waitpid(new_threadID,&status,WNOHANG|WUNTRACED);
            success = 0;
            if (res == 0)
            {
              //printf("waitpid() returns 0\n");
              thrparams->threadID = new_threadID; /* may have gotten here first */
              success = 1;
            }
            #if 0 /* debug stuff */
            else if (res == -1)
              printf("waitpid() returns -1, err=%s\n", strerror(errno));
            else if (res != new_threadID)
              printf("strange. waitpid() returns something wierd\n");
            else if (WIFEXITED(status))
              printf("child %d called exit(%d) or _exit(%d)\n",new_threadID,
                WEXITSTATUS(status), WEXITSTATUS(status));
            else if (WIFSIGNALED(status))
            {
              int sig = WTERMSIG(status);
              printf("child %d caught %s signal %d\n",new_threadID,
                     (sig < NSIG ? sys_signame[sig] : "unknown"), sig );
            }
            else if (WIFSTOPPED(status))
              printf("child %d stopped on %d signal\n",new_threadID,WSTOPSIG(status));
            else
              printf("unkown cause for stop\n");
            #endif
            if (!success)
              kill(new_threadID, SIGKILL); /* its hung. kill it */
          } /* if parent or child */
        } /* thrparam inheritance ok */
      } /* FreeBSD >= 3.0 (+ SMP kernel optional) + global inheritance ok */
      #elif ((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_PS2LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS)
      {
        if (thrparams->threadnum == 0) /* first thread */
          use_poll_process = 1;
        else
        {
          SetGlobalPriority(thrparams->priority); /* so priority is inherited */
          thrparams->threadID = kthread_create( (void (*)(void *))Go_mt,
                                                8192, (void *)thrparams );
          success = (thrparams->threadID > 0);
        }
      }
      #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
      {
        if (winGetVersion() < 400) /* win32s */
          use_poll_process = 1;
        else
        {
          thrparams->threadID = _beginthread( Go_mt, 8192, (void *)thrparams );
          /* MSVC _beginthread() returns -1 on error, _beginthreadex returns 0, other compilers - ? */
          /* so check both cases */
          success = ( thrparams->threadID != 0 && thrparams->threadID != -1 );
        }
      }
      #elif (CLIENT_OS == OS_OS2)
      {
        thrparams->threadID = _beginthread( Go_mt, NULL, 32768, (void *)thrparams );
        success = ( thrparams->threadID != -1);
      }
      #elif (CLIENT_OS == OS_NETWARE)
      {
        char threadname[64];
        if (thrparams->threadnum == 0) /* initialize main while we're here */
        {
          MPKEnterNetWare(); /* stick to CPU 0 for main thread */
          /* and initializes MPK stubs before the threads call it */
          sprintf(threadname, "%s Main", utilGetAppName() );
          RenameThread( GetThreadID(), threadname );
        }
        thrparams->threadID = BeginThread( Go_mt, (void *)0,
                                           8192, (void *)thrparams );
        if (thrparams->threadID == -1)
          thrparams->threadID = 0;
        success = ( thrparams->threadID != 0);
        if (success)
        {
          sprintf(threadname, "%s crunch #%02x", utilGetAppName(),
                                             thrparams->threadnum + 1 );
          RenameThread( thrparams->threadID, threadname );
        }
      }
      #elif (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
      {
        char thread_name[128];
        long be_priority = thrparams->priority+1;
        // Be OS priority should be adjustable from 1 to 10
        // 1 is lowest, 10 is higest for non-realtime and non-system tasks
        sprintf(thread_name, "%s crunch#%d", utilGetAppName(), thread_i + 1);
        thrparams->threadID = spawn_thread((long (*)(void *)) Go_mt,
               thread_name, be_priority, (void *)thrparams );
        if ( ((thrparams->threadID) >= B_NO_ERROR) &&
             (resume_thread(thrparams->threadID) == B_NO_ERROR) )
            success = 1;
      }
      #elif (CLIENT_OS == OS_AMIGAOS)
      {
        char threadname[64];
        sprintf(threadname, "%s crunch #%d", utilGetAppName(),
                                             thrparams->threadnum + 1 );
        #if (CLIENT_CPU == CPU_68K)
        struct Process *proc;
        if ((proc = CreateNewProcTags(NP_Entry, (ULONG)Go_mt,
                                      NP_StackSize, 8192,
                                      NP_Name, (ULONG)threadname,
                                      TAG_END)))
        {
           struct Process *thisproc = (struct Process *)FindTask(NULL);
           struct ThreadArgsMsg argsmsg;
           argsmsg.tp_ExecMessage.mn_Node.ln_Type = NT_MESSAGE;
           argsmsg.tp_ExecMessage.mn_ReplyPort = &(thisproc->pr_MsgPort);
           argsmsg.tp_ExecMessage.mn_Length = sizeof(struct ThreadArgsMsg);
           argsmsg.tp_Params = thrparams;
           PutMsg(&(proc->pr_MsgPort),(struct Message *)&argsmsg);
           WaitPort(&(thisproc->pr_MsgPort));
           GetMsg(&(thisproc->pr_MsgPort));
        }
        thrparams->threadID = (int)proc;
        #elif defined(__amigaos4__)
        thrparams->threadID = (int)CreateNewProcTags(NP_Entry, (ULONG)Go_mt,
                                                     NP_StackSize, 8192,
                                                     NP_Name, (ULONG)threadname,
                                                     NP_Child, TRUE,
                                                     NP_UserData, (ULONG)thrparams,
                                                     TAG_END);
        #elif !defined(__POWERUP__)
        struct TagItem tags[5];
        tags[0].ti_Tag = TASKATTR_CODE; tags[0].ti_Data = (ULONG)Go_mt;
        tags[1].ti_Tag = TASKATTR_NAME; tags[1].ti_Data = (ULONG)threadname;
        tags[2].ti_Tag = TASKATTR_STACKSIZE; tags[2].ti_Data = 8192;
        tags[3].ti_Tag = TASKATTR_R3; tags[3].ti_Data = (ULONG)thrparams;
        tags[4].ti_Tag = TAG_END;
        thrparams->threadID = (int)CreateTaskPPC(tags);
        #else
        struct TagItem tags[4];
        tags[0].ti_Tag = PPCTASKTAG_NAME; tags[0].ti_Data = (ULONG)threadname;
        tags[1].ti_Tag = PPCTASKTAG_STACKSIZE; tags[1].ti_Data = 8192;
        tags[2].ti_Tag = PPCTASKTAG_ARG1; tags[2].ti_Data = (ULONG)thrparams;
        tags[3].ti_Tag = TAG_END;
        thrparams->threadID = (int)PPCCreateTask(NULL,&Go_mt,tags);
        #endif
        if (thrparams->threadID)
        {
          success = 1;
        }
      }
      #elif (CLIENT_OS == OS_MORPHOS)
      {
        char threadname[64];
        sprintf(threadname, "%s crunch #%d", utilGetAppName(),
                                             thrparams->threadnum + 1 );
        struct TagItem tags[6];
        tags[0].ti_Tag = TASKTAG_PC; tags[0].ti_Data = (ULONG)Go_mt;
        tags[1].ti_Tag = TASKTAG_CODETYPE; tags[1].ti_Data = CODETYPE_PPC;
        tags[2].ti_Tag = TASKTAG_NAME; tags[2].ti_Data = (ULONG)threadname;
        tags[3].ti_Tag = TASKTAG_STACKSIZE; tags[3].ti_Data = 8192;
        tags[4].ti_Tag = TASKTAG_PPC_ARG1; tags[4].ti_Data = (ULONG)thrparams;
        tags[5].ti_Tag = TAG_END;
        thrparams->threadID = (int)NewCreateTaskA(tags);
        if (thrparams->threadID)
        {
          success = 1;
        }
      }
      #elif ((CLIENT_OS == OS_SUNOS) || (CLIENT_OS == OS_SOLARIS))
      {
         if (thr_create(NULL, 0, (void *(*)(void *))Go_mt,
                   (void *)thrparams, THR_BOUND, &thrparams->threadID ) == 0)
           success = 1;
      }
      #elif defined(_POSIX_THREADS_SUPPORTED) //defined in cputypes.h
      {
        #if defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
          SetGlobalPriority( thrparams->priority );
          pthread_attr_t thread_sched;
          pthread_attr_init(&thread_sched);
          pthread_attr_setscope(&thread_sched,PTHREAD_SCOPE_SYSTEM);
          pthread_attr_setinheritsched(&thread_sched,PTHREAD_INHERIT_SCHED);
          if (pthread_create( &(thrparams->threadID), &thread_sched,
                (void *(*)(void*)) Go_mt, (void *)thrparams ) == 0)
            success = 1;
          SetGlobalPriority( 9 ); //back to normal
        #else
          if (pthread_create( &(thrparams->threadID), NULL,
             (void *(*)(void*)) Go_mt, (void *)thrparams ) == 0 )
            success = 1;
        #endif
      }
      #else
        use_poll_process = 1;
      #endif
    }

    if (use_poll_process)
    {
      thrparams->realthread = 0;            /* int */

      /* non-real-threads share their dyn_timeslice_data */
      thrparams->dyn_timeslice_table = &(default_dyn_timeslice_table[0]);
      if (is_non_preemptive_os)
        thrparams->dyn_timeslice_table = &(non_preemptive_dyn_timeslice_table[0]);

      //fprintf(stderr,"scheduling poll-process\n");
      thrparams->threadID = (THREADID)RegPolledProcedure(Go_mt,
                                (void *)thrparams , NULL, 0 );
      success = ((thrparams->threadID) != (THREADID)-1);
    }

    if (success)
    {
      ClientEventSyncPost( CLIEVENT_CLIENT_THREADSTARTED, &(thread_i), sizeof(thread_i) );
      if (thrparams->realthread)
        NonPolledUSleep(100);   //let the thread start
    }
    else
    {
      cmem_free( thrparams );
      thrparams = NULL;
    }
  }
  return thrparams;
}

/* ----------------------------------------------------------------------- */

static int __gsc_flag_allthreads(struct thread_param_block *thrparam,
                                 int whichflag )
{
  int isexit = 0, ispause = 0;
  if (whichflag == 'x')
    isexit = 1;
  else if (whichflag == 's')
    ispause = 1;
  while (thrparam)
  {
    if (whichflag == 'c')
    {
      if (!thrparam->is_suspended)
        return -1;
    }
    else
    {
      if (isexit)
        thrparam->do_exit = isexit;
      thrparam->do_suspend = ispause;
    }
    thrparam = thrparam->next;
  }
  return 0;
}


static int __CheckClearIfRefillNeeded(struct thread_param_block *thrparam,
                                      int doclear)
{
  int refillneeded = 0;
  while (thrparam)
  {
    if (thrparam->refillneeded)
      refillneeded++;
    if (doclear)
      thrparam->refillneeded = 0;
    thrparam = thrparam->next;
  }
  return refillneeded;
}

#if defined(DYN_TIMESLICE_SHOWME)
static void __dyn_timeslice_showme(struct thread_param_block *thrparam)
{
  while (thrparam)
  {
    unsigned int ctr = 0;
    unsigned long totaltime = 0, totalts = 0;
    if ((++thrparam->dyn_timeslice_showme.dispactive) == 1)
    {
      totaltime = thrparam->dyn_timeslice_showme.totaltime;
      totalts = thrparam->dyn_timeslice_showme.totalts;
      ctr = thrparam->dyn_timeslice_showme.ctr;
      if (ctr >= 5000 || totaltime > 100000000ul) /* 100 secs */
        thrparam->dyn_timeslice_showme.contest = (unsigned int)-1;
      else
        ctr = 0;
    }
    --thrparam->dyn_timeslice_showme.dispactive;
    if (ctr)
    {
      printf("\rT# %d (r=%d) ctr: %u avgs TS: %lu us: %lu\n",
              thrparam->threadnum , thrparam->realthread,
              ctr, totalts/ctr, totaltime/ctr );
    }
    thrparam = thrparam->next;
  }
  return;
}
#endif

// We need to reset some static variables to their initial state before
// restarting the crunchers (#3495)
static void __resetStaticVars(void)
{
  BenchResetStaticVars();
  CliResetStaticVars();
}

/* ----------------------------------------------------------------------- */

static int GetMaxCrunchersPermitted( void )
{
#if (CLIENT_OS == OS_RISCOS) && defined(HAVE_X86_CARD_SUPPORT)
  if (GetNumberOfDetectedProcessors() > 1)
    return 2; /* thread 0 is ARM, thread 1 is x86 */
#endif
  return ( 512 ); /* just some arbitrary number */
}

/* ---------------------------------------------------------------------- */

// returns:
//    -3 = exit by error (no required co-processor found)
//    -2 = exit by error (all contests closed)
//    -1 = exit by error (critical)
//     0 = exit for unknown reason
//     1 = exit by user request
//     2 = exit by exit file check
//     3 = exit by time limit expiration
//     4 = exit by block count expiration
int ClientRun( Client *client )
{
  unsigned int prob_i;
  int force_no_realthreads = 0;
  int is_non_preemptive_os = 0;
  struct thread_param_block *thread_data_table = NULL;

  int TimeToQuit = 0, exitcode = 0;
  int probmanIsInit = 0;
  int local_connectoften = 0;
  unsigned int load_problem_count = 0;
  unsigned int getbuff_errs = 0;

  time_t timeRun = 0, timeNextConnect=0, timeNextCheckpoint=0;

  time_t last_scheduledupdatetime = 0; /* we reset the next two vars on != */
  //unsigned int flush_scheduled_count = 0; /* used for exponential staging */
  unsigned int flush_scheduled_adj   = 0; /*time adjustment to next schedule*/
  time_t ignore_scheduledupdatetime_until = 0; /* ignore schedupdtime until */

  int checkpointsDisabled = (client->nodiskbuffers != 0);
  int dontSleep=0, isPaused=0, wasPaused=0, timeMonoError = 0;

  ClientEventSyncPost( CLIEVENT_CLIENT_RUNSTARTED, 0, 0 );
  __resetStaticVars();

  // =======================================
  // Notes:
  //
  // Do not break flow with a return()
  // [especially not after problems are loaded]
  //
  // Code order:
  //
  // Initialization: (order is important, 'F' denotes code that can fail)
  // 1.    UndoCheckpoint() (it is not affected by TimeToQuit)
  // 2.    Determine number of problems
  // 3. F  Create problem table (InitializeProblemManager())
  // 4. F  Load (or try to load) that many problems (needs number of problems)
  // 5. F  Initialize polling process (needed by threads)
  // 6. F  Spin up threads
  // 7.    Unload excess problems (problems for which we have no worker)
  //
  // Run...
  //
  // Deinitialization:
  // 8.    Shut down threads
  // 9.   Deinitialize polling process
  // 10.   Unload problems
  // 11.   Deinitialize problem table
  // 12.   Throw away checkpoints
  // =======================================

  // --------------------------------------
  // Recover blocks from checkpoint files before anything else
  // we always recover irrespective of TimeToQuit
  // --------------------------------------

  if (!checkpointsDisabled) //!nodiskbuffers
  {
    TRACE_OUT((0,"doing CheckpointAction(CHECKPOINT_OPEN)\n"));
    if (CheckpointAction( client, CHECKPOINT_OPEN, 0 )) //-> !0 if checkpts disabled
    {
      checkpointsDisabled = 1;
    }
  }

  // --------------------------------------
  // Determine the number of problems to work with. Number is used everywhere.
  // --------------------------------------

  if (!TimeToQuit)
  {
    int numcrunchers = client->numcpu; /* assume this till we know better */
    force_no_realthreads = 0; /* this is a hint. it does not reflect capability */

    if (numcrunchers == 0) /* force single threading */
    {
      numcrunchers = 1;
      force_no_realthreads = 1;
      #if defined(CLIENT_SUPPORTS_SMP)
      LogScreen( "Client will run single-threaded.\n" );
      #endif
    }
    else if (numcrunchers < 0) /* autoselect */
    {
      TRACE_OUT((0,"doing GetNumberOfDetectedProcessors()\n"));
      numcrunchers = GetNumberOfDetectedProcessors(); /* cpucheck.cpp */
      if (numcrunchers < 0)
      {
        LogScreen( CLIENT_OS_NAME_EXTENDED " does not support SMP or\n"
                  "does not support processor count detection.\n"
                  "A single processor machine is assumed.\n");
        numcrunchers = 1;
      }
      else if (numcrunchers == 1)
      {
        LogScreen("Automatic processor detection found %d processor%s.\n",
                   numcrunchers, ((numcrunchers==1)?(""):("s")) );
      }
      else
      {
        Log("Automatic processor detection found %d processor%s.\n",
                   numcrunchers, ((numcrunchers==1)?(""):("s")) );
      }
    }
    if (numcrunchers > GetMaxCrunchersPermitted())
    {
      numcrunchers = GetMaxCrunchersPermitted();
    }

    #if (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_OS2) || (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
    force_no_realthreads = 0; // must run with real threads because the
                              // main thread runs at normal priority
    #elif (CLIENT_OS == OS_NETWARE)
    //if (numcrunchers == 1) // NetWare client prefers non-threading
    //  force_no_realthreads = 1; // if only one thread/processor is to used
    #endif

    // On GPU, number of crunchers must NEVER exceed number of detected GPUs
    #if (CLIENT_CPU == CPU_CUDA) || (CLIENT_CPU == CPU_ATI_STREAM) || (CLIENT_CPU == CPU_OPENCL)
    int gpus = GetNumberOfDetectedProcessors();
    if (gpus < 0)
      gpus = 0; // just in case. It should not fail on GPU (return 0 on error)
    if (numcrunchers > gpus)
      numcrunchers = gpus;
    #endif

    load_problem_count = numcrunchers;
  }

  // --------------------------------------
  // Optional. Print nice error message if no GPUs were found. Even without this,
  // client will stop later anyway, because no problem were loaded, with message
  // "Unable to initialize problem manager", which may be too cryptic.
  // --------------------------------------

  if (!TimeToQuit)
  {
    if (load_problem_count == 0)
    {
      Log( "No crunchers to start. Quitting...\n" );
      exitcode = -3;
      TimeToQuit = 1;
    }
  }

  // -------------------------------------
  // build a problem table
  // -------------------------------------

  if (!TimeToQuit)
  {
    TRACE_OUT((+1,"doing InitializeProblemManager(%d)\n",load_problem_count));
    probmanIsInit = InitializeProblemManager(load_problem_count);
    TRACE_OUT((-1,"InitializeProblemManager() =>%d\n",probmanIsInit));
    if (probmanIsInit > 0)
      load_problem_count = (unsigned int)probmanIsInit;
    else
    {
      Log( "Unable to initialize problem manager. Quitting...\n" );
      probmanIsInit = 0;
      load_problem_count = 0;
      TimeToQuit = 1;
    }
  }

  // -------------------------------------
  // load (or rather, try to load) that many problems
  // -------------------------------------

  if (!TimeToQuit)
  {
    if (load_problem_count != 0)
    {
      //for (prob_i = 0; prob_i < CONTEST_COUNT; prob_i++) //get select core msgs
      //  selcoreGetSelectedCoreForContest( prob_i );      //... out of the way.
      if (load_problem_count > 1)
        Log( "Loading crunchers with work...\n" );
      TRACE_OUT((+1,"LoadSaveProblems(%d)\n",load_problem_count));
      load_problem_count = LoadSaveProblems( client, load_problem_count, 0 );
      TRACE_OUT((-1,"LoadSaveProblems() =>%d\n",load_problem_count));
    }
    if (CheckExitRequestTrigger())
    {
      TimeToQuit = 1;
      exitcode = -2;
    }
    else if (load_problem_count == 0)
    {
      Log("Unable to load any work. Quitting...\n");
      TimeToQuit = 1;
      exitcode = -2;
    }
  }

  // --------------------------------------
  // Initialize the async "process" subsystem
  // --------------------------------------

  if (!TimeToQuit)
  {
    if (InitializePolling())
    {
      Log( "Unable to initialize async subsystem.\n");
      TimeToQuit = 1;
      exitcode = -1;
    }
  }


  // --------------------------------------
  // fixup the dyn_timeslice_table if running a non-preemptive OS
  // --------------------------------------

  if (!TimeToQuit)
  {
    is_non_preemptive_os = 0;  /* assume this until we know better */
    #if (CLIENT_OS == OS_WIN16) || \
        (CLIENT_OS == OS_RISCOS) || (CLIENT_OS == OS_NETWARE) || \
        (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64) /* win32 only if win32s */
    {
      is_non_preemptive_os = 1; /* assume this until we know better */
      #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)       /* only if win32s */
      if (winGetVersion()>=400)
        is_non_preemptive_os = 0;
      #elif (CLIENT_OS == OS_NETWARE)
        /* we enable 'preemptiveness' on a per-thread basis */
      #endif
      if (is_non_preemptive_os)
      {
        int tsinitd;
        for (tsinitd=0;tsinitd<CONTEST_COUNT;tsinitd++)
        {
          #if (CLIENT_OS == OS_RISCOS)
          if (riscos_in_taskwindow)
          {
            non_preemptive_dyn_timeslice_table[tsinitd].usec = 40000;
            non_preemptive_dyn_timeslice_table[tsinitd].optimal = 32768;
          }
          else
          {
            non_preemptive_dyn_timeslice_table[tsinitd].usec = 1000000;
            non_preemptive_dyn_timeslice_table[tsinitd].optimal = 131072;
          }
          #elif (CLIENT_OS == OS_NETWARE)
          {
            long quantum = non_preemptive_dyn_timeslice_table[0].usec;
            if (tsinitd == 0)
            {
              quantum = 512;
              #if (CLIENT_CPU == CPU_X86)
              quantum = 5000; /* 5ms good enough for NetWare 3x */
              if (GetFileServerMajorVersionNumber() >= 4) /* just guess */
              {
                #if 0
                long det_type = (GetProcessorType(1) & 0xff);
                //if (det_type > 0x0A) /* not what we know about */
                //  ConsolePrintf("\rDNETC: unknown CPU type for quantum selection in "__FILE__").\r\n");
                if (det_type==0x02 || det_type==0x07 || det_type==0x09 || det_type == 0x0B)
                  quantum = 250; /* 250 PII/PIII || Celeron-A || AMD-K7 || P4 */
                else /* the rest */
                #endif
                  quantum = 100; /* 100 */
              }
              #endif
              if (client->priority >= 0 && client->priority <= 9)
                quantum *= (client->priority+1);
              Log("NetWare: crunchers will use a %ldus timeslice quantum\n", quantum);
            }
            non_preemptive_dyn_timeslice_table[tsinitd].min = 0x10;
            non_preemptive_dyn_timeslice_table[tsinitd].usec = quantum;
            non_preemptive_dyn_timeslice_table[tsinitd].optimal = 1024;
          }
          #else /* x86 */
          {
            non_preemptive_dyn_timeslice_table[tsinitd].usec = 27500; /* 55/2 ms */
            non_preemptive_dyn_timeslice_table[tsinitd].optimal = 512 * (client->priority+1);
          }
          #endif
        }
      }
    }
    #endif
  }


  // --------------------------------------
  // Spin up the crunchers
  // --------------------------------------

  if (!TimeToQuit)
  {
    struct thread_param_block *thrparamslast = thread_data_table;
    unsigned int planned_problem_count = load_problem_count;
    load_problem_count = 0;

    for ( prob_i = 0; prob_i < planned_problem_count; prob_i++ )
    {
      struct thread_param_block *thrparams =
         __StartThread( prob_i, planned_problem_count,
                        client->priority, force_no_realthreads,
                        is_non_preemptive_os );
      if ( thrparams )
      {
        if (!thread_data_table)
          thread_data_table = thrparams;
        else
          thrparamslast->next = thrparams;
        thrparamslast = thrparams;
        load_problem_count++;
      }
      else
      {
        break;
      }
    }

    if (load_problem_count == 0)
    {
      Log("Unable to initialize cruncher(s). Quitting...\n");
      TimeToQuit = 1;
      exitcode = -1;
    }
    else
    {
      char srange[20];
      srange[0] = 0;
      if (load_problem_count > 1 && load_problem_count <= PROBLEM_PRINTABLE_ID_COUNT /* a-z,A-Z */)
      {
        sprintf( srange, " ('a'%s'%c')",
          ((load_problem_count==2)?(" and "):("-")),
          ProblemLetterId(load_problem_count-1) );
      }
      Log("%u cruncher%s%s ha%s been started.%c%c%u failed to start)\n",
             load_problem_count,
             ((load_problem_count==1)?(""):("s")), srange,
             ((load_problem_count==1)?("s"):("ve")),
             ((load_problem_count < planned_problem_count)?(' '):('\n')),
             ((load_problem_count < planned_problem_count)?('('):(0)),
             (planned_problem_count - load_problem_count) );
    }

    // resize the problem table if we've loaded too much
    if (load_problem_count < planned_problem_count)
    {
      if (TimeToQuit)
        LoadSaveProblems(client, planned_problem_count,PROBFILL_UNLOADALL);
      else
        LoadSaveProblems(client, load_problem_count, PROBFILL_RESIZETABLE);
    }
  }

  #if defined(TRACE) && 0
  for (prob_i = 0; prob_i < CONTEST_COUNT; ++prob_i)
  {
    if (IsProblemLoadPermitted(0, prob_i))
    {
      TRACE_OUT((0, "%s thresholds: in: %d (%d wu/%d h) out: %d (%d wu)\n",
           CliGetContestNameFromID(prob_i),
           ClientGetInThreshold(client, prob_i, 1),
           client->inthreshold[prob_i],
           client->timethreshold[prob_i],
           ClientGetOutThreshold(client, prob_i),
           client->outthreshold[prob_i] ));
    }
  }
  #endif

  //============================= MAIN LOOP =====================
  //now begin looping until we have a reason to quit
  //------------------------------------

  dontSleep = 1; // don't sleep in the first loop
                 // (do percbar, connectoften, checkpt first)

  // Start of MAIN LOOP
  while (TimeToQuit == 0)
  {
    //------------------------------------
    //sleep, run or pause...
    //------------------------------------

    #define MAIN_LOOP_RUN_INTERVAL 5 /* secs */
    if (!dontSleep)
    {
      int i = 0;
      SetGlobalPriority( client->priority );
      if (isPaused)
        NonPolledSleep(1);

      while ((i++)<5
            && !__CheckClearIfRefillNeeded(thread_data_table,0)
            && !CheckExitRequestTriggerNoIO()
            && ModeReqIsSet(-1)==0)
      {
        ClientEventSyncPost(CLIEVENT_CLIENT_RUNIDLE, &i, sizeof(i));
        sleep(1);
        #if defined(DYN_TIMESLICE_SHOWME)
        __dyn_timeslice_showme(thread_data_table);
        #endif
      }
      SetGlobalPriority( 9 );
    }
    dontSleep = 0; //for the next round

    //------------------------------------
    // Fixup timers
    //------------------------------------

    {
      struct timeval tv;
      if (CliClock(&tv) == 0)
      {
        if ( ((unsigned long)tv.tv_sec) >= ((unsigned long)timeRun) )
        {
          timeRun = tv.tv_sec;
          timeMonoError = 0;
        }
        else
        {
          if (timeMonoError == 0)
          {
            if (is_non_preemptive_os &&
                ( ((unsigned long)timeRun) - ((unsigned long)tv.tv_sec) ) > 2)
            {
              Log("Clock error: monotonic time found to be going backwards.\n"
                  "(%lu < %lu but ignored unless it occurs too often)\n",
                    ((unsigned long)tv.tv_sec), ((unsigned long)timeRun) );
            }
          }
          timeRun = tv.tv_sec; /* assume its ok on the next round */
          if ((++timeMonoError) > (60/MAIN_LOOP_RUN_INTERVAL))
          {
            Log("Monotonic time found to be going backwards more than\n"
                "%d times within the space off one minute. Quitting...\n",
                (60/MAIN_LOOP_RUN_INTERVAL) );
            TimeToQuit = 1;
          }
        }
      }
    }

    //----------------------------------------
    // Check for time limit...
    //----------------------------------------

    if ( !TimeToQuit && (client->minutes > 0) &&
                        (timeRun >= (time_t)( (client->minutes)*60 )))
    {
      Log( "Shutdown - reached time limit.\n" );
      TimeToQuit = 1;
      exitcode = 3;
    }

    //----------------------------------------
    // Has -runbuffers exhausted all buffers?
    //----------------------------------------

    // cramer magic (voodoo)
    if (!TimeToQuit && client->nonewblocks > 0 &&
      ((unsigned int)getbuff_errs >= load_problem_count))
    {
      TimeToQuit = 1;
      exitcode = 4;
    }

    //----------------------------------------
    // Check for user break
    //----------------------------------------

    if (!TimeToQuit && CheckExitRequestTrigger()) /* prints a message */
    {
      /* the reason for the ExitRequest has already been printed,
         we just say something here to show that the run loop is break'ing
      */
      LogScreen( "%s...\n",
           (CheckRestartRequestTrigger()?("Restarting"):("Shutting down")) );
      TimeToQuit = 1;
      exitcode = 1;
    }
    if (!TimeToQuit && utilCheckIfBetaExpired(1)) /* prints a message */
    {
      TimeToQuit = 1;
      exitcode = -1;
    }
    if (!TimeToQuit)
    {
      isPaused = CheckPauseRequestTrigger();
      if (isPaused)
      {
        if (!wasPaused)
        {
          __gsc_flag_allthreads(thread_data_table, 's'); //suspend 'em
          timeNextCheckpoint = 0; // write checkpoint file now
        }
        wasPaused = 1;
      }
      else if (wasPaused)
      {
        __gsc_flag_allthreads(thread_data_table, 0 ); //un-suspend 'em
        wasPaused = 0;
      }
    }

    //------------------------------------
    //update the status bar, check all problems for change, do reloading etc
    //------------------------------------

    if (!TimeToQuit && !isPaused)
    {
      int anychanged;
      LogScreenPercent( load_problem_count ); //logstuff.cpp
      anychanged = LoadSaveProblems(client,load_problem_count,PROBFILL_ANYCHANGED);
      __CheckClearIfRefillNeeded(thread_data_table,1);
      if (anychanged)      /* load/save action occurred */
        timeNextCheckpoint = 0; /* re-checkpoint right away */
    }

    //----------------------------------------
    // If not quitting, then write checkpoints
    //----------------------------------------

    if (!TimeToQuit && !checkpointsDisabled
        && (!isPaused || timeNextCheckpoint == 0)
        && !CheckExitRequestTriggerNoIO())
    {
      /* Checkpoints are done when CHECKPOINT_FREQ_SECSDIFF secs
       * has elapsed since the last checkpoint OR timeNextCheckpoint is zero
      */
      #define CHECKPOINT_FREQ_SECSDIFF (10*60)      /* 10 minutes */
      if ( (timeNextCheckpoint == 0) || (timeRun >= timeNextCheckpoint) )
      {
        if (CheckpointAction( client, CHECKPOINT_REFRESH, load_problem_count ))
          checkpointsDisabled = 1;
        timeNextCheckpoint = timeRun + (time_t)(CHECKPOINT_FREQ_SECSDIFF);
      }
    }

    //------------------------------------
    // Check for universally coordinated update
    //------------------------------------

    #define TIME_AFTER_START_TO_UPDATE 10800 // Three hours
    #define UPDATE_INTERVAL 600 // Ten minutes

    if (!TimeToQuit
       && client->scheduledupdatetime != 0
       && !CheckExitRequestTriggerNoIO())
    {
      time_t timeNow = CliTimer(NULL)->tv_sec;
      if (
      (((unsigned long)timeNow) < ((unsigned long)ignore_scheduledupdatetime_until)) &&
      (((unsigned long)timeNow) >= ((unsigned long)client->scheduledupdatetime)) &&
      (((unsigned long)timeNow) < (((unsigned long)client->scheduledupdatetime)+TIME_AFTER_START_TO_UPDATE)) )
      {
        if (last_scheduledupdatetime != ((time_t)client->scheduledupdatetime))
        {
          last_scheduledupdatetime = (time_t)client->scheduledupdatetime;
          //flush_scheduled_count = 0;
          flush_scheduled_adj = (rand()%UPDATE_INTERVAL);
          Log("Buffer update scheduled in %u minutes %02u seconds.\n",
               flush_scheduled_adj/60, flush_scheduled_adj%60 );
          flush_scheduled_adj += timeNow - last_scheduledupdatetime;
        }
        if ( (((unsigned long)flush_scheduled_adj) < TIME_AFTER_START_TO_UPDATE) &&
          (((unsigned long)timeNow) >= (unsigned long)(flush_scheduled_adj+last_scheduledupdatetime)) )
        {
          //flush_scheduled_count++; /* for use with exponential staging */
          flush_scheduled_adj += ((UPDATE_INTERVAL>>1)+
                                 (rand()%(UPDATE_INTERVAL>>1)));
        }
      }
    }


    //------------------------------------
    // Lurking and connect-often
    //------------------------------------

    //this first part has to be done separately from the actual
    //if-update-needed part below because LurkIsConnected()
    //provides user feedback
    local_connectoften = 0;
    if (!CheckExitRequestTriggerNoIO())
    {
      #if defined(LURK)
      if ((LurkIsWatching() & (CONNECT_LURK|CONNECT_LURKONLY))!=0)
      {                                  /* is lurk or lurkonly enabled? */
        client->connectoften = 0; /* turn off old setting */
        if (LurkIsConnected())
          local_connectoften = 3; /* both fetch and flush */
      }
      else
      #endif
      {
        if (!client->offlinemode ||
           (!client->noupdatefromfile && client->remote_update_dir[0]))
        {
          local_connectoften = client->connectoften;
          /* 0=none, &1=in-buf, &2=out-buf, &4=sticky-flag (handled elsewhere) */
        }
      }
    }
    TRACE_OUT((0,"local_connectoften=0x%x,timeRun=%u,timeNextConnect=%u\n",
               local_connectoften, (unsigned)timeRun, (unsigned)timeNextConnect));

    if (!TimeToQuit
       && (local_connectoften & 3)!=0
       && timeRun >= timeNextConnect
       && (client->max_buffupd_interval <= 0 ||
          client->last_buffupd_time == 0 ||
          timeRun >= ((client->last_buffupd_failed_time == 0)
                      ?((time_t)(client->last_buffupd_time +
                                 client->max_buffupd_interval * 60))
                      :((time_t)(client->last_buffupd_failed_time +
                                 client->max_buffupd_retry_interval * 60))) ) )
    {
      TRACE_OUT((0, "FREQ timeRun=%lu  timeNextConnect=%lu  local_connectoften=%d\n", timeRun, timeNextConnect, local_connectoften));
      TRACE_OUT((0, "FREQ last_buffupd_time=%d  last_buffupd_time+interval=%d\n", client->last_buffupd_time, client->last_buffupd_time+ client->max_buffupd_interval * 60));
      TRACE_OUT((0, "FREQ last_buffupd_failed_time=%d  last_buffupd_failed_time+interval=%d\n", client->last_buffupd_failed_time, client->last_buffupd_failed_time+ client->max_buffupd_retry_interval * 60));
      timeNextConnect = timeRun + 30; /* never more often than 30 seconds */
      if (ModeReqIsSet(MODEREQ_FETCH|MODEREQ_FLUSH) == 0)
      {
        int upd_flags = BUFFUPDCHECK_EITHER;
        /* BUFFUPDCHECK_EITHER == return both if _either_ fetch or flush needed*/
        TRACE_OUT((+1,"frequent connect check.\n"));
        if ((local_connectoften & 1) != 0) /* check fetch */
          upd_flags |= BUFFERUPDATE_FETCH|BUFFUPDCHECK_TOPOFF;
        /* BUFFUPDCHECK_TOPOFF == "fetch even if not completely empty" */
        if ((local_connectoften & 2) != 0) /* check flush */
          upd_flags |= BUFFERUPDATE_FLUSH;
        upd_flags = BufferCheckIfUpdateNeeded(client, -1, upd_flags);
        //printf("\rconnect-often: BufferCheckIfUpdateNeeded()=>0x%x\n",upd_flags);
        TRACE_OUT((-1,"frequent connect check. need update?=0x%x\n",upd_flags));
        if (upd_flags >= 0 && /* no error */
           (upd_flags & (BUFFERUPDATE_FETCH|BUFFERUPDATE_FLUSH))!=0)
        {
          ModeReqSet(MODEREQ_FETCH|MODEREQ_FLUSH|MODEREQ_FQUIET);
        }
      }
      if (client->last_buffupd_time == 0)
      {
        // initial connectoften check has been finished
        // now obey client->max_buffupd_[retry_]interval
        client->last_buffupd_time = 1;
      }
    }

    //----------------------------------------
    // If not quitting, then handle mode requests
    //----------------------------------------

    if (!TimeToQuit && ModeReqIsSet(-1))
    {
      int did_suspend = 0;
      if (ModeReqIsSet(MODEREQ_TEST_MASK|MODEREQ_BENCHMARK_MASK|MODEREQ_CONFIG))
      {
        if (!wasPaused) /* read that as 'isPaused' */
        {
          __gsc_flag_allthreads(thread_data_table, 's'); //suspend 'em
          did_suspend = 1;
        }
        while (__gsc_flag_allthreads(thread_data_table, 'c'))
        {
          /* if we got here, then we must be running real threads */
          NonPolledUSleep(250000);
        }
      }
      //For interactive benchmarks, assume that we have "normal priority"
      //at this point and threads are running at lower priority. If that is
      //not the case, then benchmarks are going to return wrong results.
      //The messy way around that is to suspend the threads.
      ModeReqRun(client);
      dontSleep = 1; //go quickly through the loop
      if (did_suspend)
        __gsc_flag_allthreads(thread_data_table, 0 ); //un-suspend 'em
    }
  }  // End of MAIN LOOP

  //======================END OF MAIN LOOP =====================

  RaiseExitRequestTrigger(); // will make other threads exit
  __gsc_flag_allthreads(thread_data_table, 'x'); //exit'ify them

  // ----------------
  // Shutting down: shut down threads
  // ----------------

  if (thread_data_table)  //we have threads running
  {
    LogScreen("Waiting for crunchers to stop...\n");
    while (thread_data_table)
    {
      struct thread_param_block *thrdatap = thread_data_table;
      thread_data_table = thrdatap->next;
      __StopThread( thrdatap );
    }
  }

  // ----------------
  // Close the async "process" handler
  // ----------------

  DeinitializePolling();

  // ----------------
  // Shutting down: save prob buffers, flush if nodiskbuffers, kill checkpts
  // ----------------

  if (probmanIsInit)
  {
    LoadSaveProblems( client,load_problem_count, PROBFILL_UNLOADALL );
    CheckpointAction( client, CHECKPOINT_CLOSE, 0 ); /* also done by LoadSaveProb */
    DeinitializeProblemManager();
  }

  ClientEventSyncPost( CLIEVENT_CLIENT_RUNFINISHED, 0, 0 );

  return exitcode;
}

// ---------------------------------------------------------------------------
