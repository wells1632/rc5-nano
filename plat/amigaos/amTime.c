/*
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: amTime.c,v 1.5 2008/12/30 20:58:44 andreasb Exp $
 *
 * Created by Oliver Roberts <oliver@futaura.co.uk>
 *
 * ----------------------------------------------------------------------
 * This file contains all the Amiga specific code related to time
 * functions, including obtaining system time, GMT offset, generating
 * delays, obtaining monotonic & linear time.
 * ----------------------------------------------------------------------
*/

/*
** Using PPC timers for short delays should be ok (and decreases overhead).
** However, there appears to be a bug in PowerUp which causes the kernel to
** hang, so don't use for PowerUp (ok with 46.30+)
*/
//#ifndef __POWERUP__
#define USE_PPCDELAY
//#endif

/*
** Native PowerUP and WarpOS timers are not guaranteed to increment as a wall
** clock would, and are inaccurate especially on overclocked systems.  PowerUp
** also treats a 66.66666MHz bus speed as 66 MHz, which causes timers to not
** be in sync with real time seconds!  Therefore, we better use 68k timers (and
** workaround the slowdown involved because of context switching overhead)
*/
//#define USE_PPCTIMER

/*
** Use PPC timers to calculate system time?  Best not!
*/
//#define USE_PPCSYSTIME


#include "amiga.h"

#ifdef __OS3PPC__
#pragma pack(2)
#endif

#include <exec/types.h>
#ifdef __amigaos4__
#include <proto/timezone.h>
#else
#include <proto/locale.h>
#endif
#include <proto/timer.h>
#include <devices/timer.h>
#include <stdlib.h>

#ifdef __OS3PPC__
#pragma pack()
#endif

#if defined(__OS3PPC__) && defined(__POWERUP__)
#include <powerup/ppclib/time.h>
#endif

static int GMTOffset = -9999;

#if defined(__MORPHOS__) || defined(__amigaos4__)
#define LOCALEBASETYPE struct Library *
#else
#define LOCALEBASETYPE struct LocaleBase *
#endif

#ifdef __MORPHOS__
#define TIMERBASETYPE  struct Library *
#else
#define TIMERBASETYPE  struct Device *
#endif

#ifdef __amigaos4__
#define TIMEVALTYPE struct TimeVal
#else
#define TIMEVALTYPE struct timeval
#endif

TIMERBASETYPE TimerBase = NULL;

#ifdef __OS3PPC__

#ifdef USE_PPCSYSTIME
static struct timeval BaseTime;
#endif

#if defined(__POWERUP__) && (defined(USE_PPCTIMER) || defined(USE_PPCSYSTIME))
static void *TimerObj = NULL;
#endif

#endif /* __OS3PPC__ */

#ifdef __amigaos4__
struct LocaleIFace *ILocale = NULL;
struct TimerIFace *ITimer = NULL;
#define DST_NONE 0
#endif

/* allocated on a per task basis */
struct TimerResources
{
   struct timerequest *tr_TimeReq;
   struct MsgPort     *tr_TimePort;
   BOOL                tr_TimerDevOpen;
   BOOL                tr_IsThread;

   #if defined(__OS3PPC__) && defined(__POWERUP__) && defined(USE_PPCDELAY)
   ULONG               tr_DelaySig;
   #endif
};

#ifndef NO_GUI
extern struct Library *DnetcBase;
#endif

/* Local prototypes */
static int GetGMTOffset(void);
TIMERBASETYPE OpenTimer(BOOL isthread);
VOID CloseTimer(VOID);
BOOL GlobalTimerInit(VOID);
VOID GlobalTimerDeinit(VOID);
#if !defined(__OS3PPC__)
/* 68K / OS4 / MorphOS */
static struct timerequest *amigaSleepAsync(struct TimerResources *res, unsigned int secs, unsigned int usecs);
#elif defined(__POWERUP__)
/* PowerUp */
static void *amigaSleepAsync(unsigned int ticks, ULONG sigmask);
#endif
/********************/

VOID CloseTimer(VOID)
{
   struct TimerResources *res;

   #ifndef __OS3PPC__
   struct Task *task = FindTask(NULL);
   res = (struct TimerResources *)task->tc_UserData;
   #elif !defined(__POWERUP__)
   struct TaskPPC *task = FindTaskPPC(NULL);
   res = (struct TimerResources *)task->tp_Task.tc_UserData;
   #else
   res = (struct TimerResources *)PPCGetTaskAttr(PPCTASKTAG_EXTUSERDATA);
   #endif

   if (res) {
      #if defined(__OS3PPC__) && defined(__POWERUP__) && defined(USE_PPCDELAY)
      if (res->tr_DelaySig != (ULONG)-1) PPCFreeSignal(res->tr_DelaySig);
      #endif

      if (res->tr_TimeReq) {
         if (res->tr_TimerDevOpen) CloseDevice((struct IORequest *)res->tr_TimeReq);
         DeleteIORequest((struct IORequest *)res->tr_TimeReq);
      }
      if (res->tr_TimePort) DeleteMsgPort(res->tr_TimePort);
      FreeVec(res);

      #ifndef __OS3PPC__
      task->tc_UserData = NULL;
      #elif !defined(__POWERUP__)
      task->tp_Task.tc_UserData = NULL;
      #else
      PPCSetTaskAttr(PPCTASKTAG_EXTUSERDATA,(ULONG)NULL);
      #endif
   }
}


TIMERBASETYPE OpenTimer(BOOL isthread)
{
   TIMERBASETYPE timerbase = NULL;
   struct TimerResources *res;

   if ((res = (struct TimerResources *)AllocVec(sizeof(struct TimerResources),MEMF_CLEAR|MEMF_PUBLIC))) {
      #ifndef __OS3PPC__
      (FindTask(NULL))->tc_UserData = (APTR)res;
      #elif !defined(__POWERUP__)
      (FindTaskPPC(NULL))->tp_Task.tc_UserData = (APTR)res;
      #else
      PPCSetTaskAttr(PPCTASKTAG_EXTUSERDATA,(ULONG)res);
      #endif

      if ((res->tr_TimePort = CreateMsgPort())) {
         if ((res->tr_TimeReq = (struct timerequest *)CreateIORequest(res->tr_TimePort,sizeof(struct timerequest)))) {
            if (!OpenDevice("timer.device",UNIT_VBLANK,(struct IORequest *)res->tr_TimeReq,0)) {
               res->tr_TimerDevOpen = TRUE;
               timerbase = (TIMERBASETYPE) res->tr_TimeReq->tr_node.io_Device;
	    }
         }
      }

      #if defined(__OS3PPC__) && defined(__POWERUP__) && defined(USE_PPCDELAY)
      res->tr_DelaySig = (ULONG)-1;
      if (timerbase) {
         res->tr_DelaySig = PPCAllocSignal((ULONG)-1);
         if (res->tr_DelaySig == (ULONG)-1) timerbase = NULL;
      }
      #endif

      res->tr_IsThread = isthread;
   }

   if (!timerbase) CloseTimer();

   return(timerbase);
}

VOID GlobalTimerDeinit(VOID)
{
   CloseTimer();

   #ifdef __amigaos4__
   if (ITimer) DropInterface((struct Interface *)ITimer);
   #endif

   #if defined(__OS3PPC__) && defined(__POWERUP__)
   #if defined(USE_PPCTIMER) || defined (USE_PPCSYSTIME)
   if (TimerObj) PPCDeleteTimerObject(TimerObj);
   #endif
   #endif
}

BOOL GlobalTimerInit(VOID)
{
   BOOL done;

   GMTOffset = GetGMTOffset();

   done = ((TimerBase = OpenTimer(FALSE)) != NULL);
   #ifdef __amigaos4__
   if (done) {
      if (!(ITimer = (struct TimerIFace *)GetInterface( (struct Library *)TimerBase, "main", 1L, NULL ))) {
         done = FALSE;
      }
   }
   #endif

   #ifdef __OS3PPC__
   #ifdef __POWERUP__
   /*
   ** PowerUp
   */
   #if defined(USE_PPCTIMER) || defined(USE_PPCSYSTIME)
   if (done) {
      done = FALSE;
      struct TagItem tags[2] = { {PPCTIMERTAG_CPU, TRUE}, {TAG_END,0} };
      if ((TimerObj = PPCCreateTimerObject(tags))) {
         #ifdef USE_PPCSYSTIME
         GetSysTime(&BaseTime);
         PPCSetTimerObject(TimerObj,PPCTIMERTAG_START,NULL);
         /* add the offset from UNIX to AmigaOS time system */
         BaseTime.tv_sec += 2922 * 24 * 3600 + 60 * GMTOffset;
         #endif
         done = TRUE;
      }
   }
   #endif

   #else
   /*
   ** WarpOS
   */
   #ifdef USE_PPCSYSTIME
   if (done) {
      struct timeval tv;
      GetSysTime(&BaseTime);
      GetSysTimePPC(&tv);
      /* add the offset from UNIX to AmigaOS time system */
      BaseTime.tv_sec += 2922 * 24 * 3600 + 60 * GMTOffset;
      SubTimePPC(&BaseTime,&tv);
   }
   #endif

   #endif
   #endif /* __OS3PPC__ */

   if (!done) {
      GlobalTimerDeinit();
   }

   return(done);
}

#ifndef NO_GUI
#if !defined(__OS3PPC__)
/* 68K / OS4 / MorphOS */
static struct timerequest *amigaSleepAsync(struct TimerResources *res, unsigned int secs, unsigned int usecs)
{
   res->tr_TimeReq->tr_node.io_Command = TR_ADDREQUEST;
   {
   struct timeval *tv = &res->tr_TimeReq->tr_time;
   tv->tv_secs = secs + (usecs / 1000000);
   tv->tv_micro = usecs % 1000000;
   }
   SendIO((struct IORequest *)res->tr_TimeReq);

   return(res->tr_TimeReq);
}
#elif defined(__POWERUP__)
/* PowerUp */
static void *amigaSleepAsync(unsigned int ticks, ULONG sigmask)
{
   struct TagItem tags[4];

   tags[0].ti_Tag = PPCTIMERTAG_50HZ;       tags[0].ti_Data = ticks;
   tags[1].ti_Tag = PPCTIMERTAG_SIGNALMASK; tags[1].ti_Data = sigmask;
   tags[2].ti_Tag = PPCTIMERTAG_AUTOREMOVE; tags[2].ti_Data = TRUE;
   tags[3].ti_Tag = TAG_END;

   return PPCCreateTimerObject(tags);
}
#endif
#endif /* NO_GUI */

void amigaSleep(unsigned int secs, unsigned int usecs)
{
#ifndef NO_GUI
   struct TimerResources *res;

   #ifndef __OS3PPC__
   res = (struct TimerResources *)(FindTask(NULL))->tc_UserData;
   #elif !defined(__POWERUP__)
   res = (struct TimerResources *)(FindTaskPPC(NULL))->tp_Task.tc_UserData;
   #else
   unsigned int ticks = secs * TICKS_PER_SECOND + (usecs * TICKS_PER_SECOND / 1000000);
   void *timer;
   ULONG sigmask;
   res = (struct TimerResources *)PPCGetTaskAttr(PPCTASKTAG_EXTUSERDATA);
   sigmask = 1L << res->tr_DelaySig;
   #endif

   if (!res->tr_IsThread && DnetcBase) {
      #if !defined(__OS3PPC__)
      amigaHandleGUI(amigaSleepAsync(res,secs,usecs));
      #elif defined(__POWERUP__)
      if (ticks > 0) {
         timer = amigaSleepAsync(ticks,sigmask);
         amigaHandleGUI(timer,timer ? sigmask : 0);
      }
      else {
         Delay(0);
      }
      #else
      struct timeval tv;
      tv.tv_sec = secs;
      tv.tv_micro = usecs;
      amigaHandleGUI(&tv);
      #endif
      return;
   }
#endif

#if !defined(__OS3PPC__) || !defined(USE_PPCDELAY)
   /*
   ** 68K / OS4 / MorphOS
   */
  #ifdef NO_GUI
   struct TimerResources *res;

   #ifndef __OS3PPC__
   res = (struct TimerResources *)(FindTask(NULL))->tc_UserData;
   #elif !defined(__POWERUP__)
   res = (struct TimerResources *)(FindTaskPPC(NULL))->tp_Task.tc_UserData;
   #else
   res = (struct TimerResources *)PPCGetTaskAttr(PPCTASKTAG_EXTUSERDATA);
   #endif
  #endif
   res->tr_TimeReq->tr_node.io_Command = TR_ADDREQUEST;
   {
   struct timeval *tv = &res->tr_TimeReq->tr_time;
   tv->tv_secs = secs + (usecs / 1000000);
   tv->tv_micro = usecs % 1000000;
   }
   DoIO((struct IORequest *)res->tr_TimeReq);
#elif !defined(__POWERUP__)
   /*
   ** WarpOS 
   */
   if (secs <= 4294) {
      WaitTime(0,secs*1000000 + usecs);
   } else {
      /* will almost definitely never happen! */
      Delay(secs * TICKS_PER_SECOND + (usecs * TICKS_PER_SECOND / 1000000));
   }
#else
   /*
   ** PowerUp 
   */
  #ifdef NO_GUI
   struct TimerResources *res = (struct TimerResources *)PPCGetTaskAttr(PPCTASKTAG_EXTUSERDATA);
  #endif

   if (ticks > 0) {  // PowerUp timer objects can't handle zero!
      if ((timer = amigaSleepAsync(ticks,sigmask))) {
         PPCWait(sigmask);
         PPCDeleteTimerObject(timer);
      }
   }
   else {
      Delay(0);
   }
#endif
}

static int GetGMTOffset(void)
{
   int gmtoffset = 0;
#ifdef __amigaos4__
   struct Library *TimezoneBase;
   struct TimezoneIFace *ITimezone;
   if((TimezoneBase = OpenLibrary("timezone.library",52L))) {
      if((ITimezone = (struct TimezoneIFace *)GetInterface(TimezoneBase,"main",1L,NULL))) {
         GetTimezoneAttrs(NULL,TZA_UTCOffset,&gmtoffset,TAG_END);
         DropInterface((struct Interface *)ITimezone);
      }
      CloseLibrary(TimezoneBase);
   }
#else
   if (!LocaleBase) {
      LocaleBase = (LOCALEBASETYPE)OpenLibrary("locale.library",38L);
   }

   if (LocaleBase) {
      struct Locale *locale;
      if ((locale = OpenLocale(NULL))) {
         gmtoffset = locale->loc_GMTOffset;
         CloseLocale(locale);
      }
   }
#endif
   return(gmtoffset);
}

int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
#if !defined(__OS3PPC__) || !defined(USE_PPCSYSTIME)
   /*
   ** 68K / OS4
   */
   if (tp) {
      /* This check required as C++ class init code calls this routine */
      if (!TimerBase) GlobalTimerInit();
      GetSysTime((TIMEVALTYPE *)tp);
      /* add the offset from UNIX to AmigaOS time system */
      tp->tv_sec += 2922 * 24 * 3600 + 60 * GMTOffset;
   }
#elif !defined(__POWERUP__)
   /*
   ** WarpOS
   */
   if (tp) {
      /* This check required as C++ class init code calls this routine */
      if (!TimerBase) GlobalTimerInit();

      GetSysTimePPC(tp);
      AddTimePPC(tp,&BaseTime);
   }
#else
   /*
   ** PowerUp
   */
   if (tp) {
      unsigned long long secs, usecs;

      /* This check required as C++ class init code calls this routine */
      if (!TimerBase) GlobalTimerInit();

      PPCSetTimerObject(TimerObj,PPCTIMERTAG_STOP,NULL);
      PPCGetTimerObject(TimerObj,PPCTIMERTAG_DIFFSECS,&secs);
      PPCGetTimerObject(TimerObj,PPCTIMERTAG_DIFFMICRO,&usecs);
      tp->tv_sec = BaseTime.tv_sec + secs;
      tp->tv_usec = BaseTime.tv_usec + (usecs - secs * 1000000);
      if (tp->tv_usec >= 1000000) {
         tp->tv_sec++;
         tp->tv_usec -= 1000000;
      }
   }
#endif

   if (tzp) {
      tzp->tz_minuteswest = -GMTOffset;
      tzp->tz_dsttime = DST_NONE;
   }

   return 0;
}

/*
** Since this routine is called frequently, especially during cracking, we
** need this routine to have as little overhead as possible.  On the PPC,
** this means using PPC native kernel calls (no context switches to 68k)
*/
int amigaGetMonoClock(struct timeval *tp)
{
   if (tp) {
#ifdef __MORPHOS__
      /*
      ** MorphOS
      */
      UQUAD cpuclock;
      ULONG cpufreq = ReadCPUClock(&cpuclock);
      tp->tv_sec  = cpuclock / cpufreq;
      tp->tv_usec = (cpuclock % cpufreq) * 1000000 / cpufreq;
#elif defined(__amigaos4__)
      /*
      ** OS4
      */
      GetUpTime((TIMEVALTYPE *)tp);
#elif !defined(__OS3PPC__) || !defined(USE_PPCTIMER)
      /*
      ** 68K / OS4
      */
      union {
         struct EClockVal   ec;
         unsigned long long etime;
      };
      ULONG efreq = ReadEClock(&ec);
      tp->tv_sec = etime / efreq;
      tp->tv_usec = (etime % efreq) * 1000000 / efreq;
#elif !defined(__POWERUP__)
      /*
      ** WarpOS
      */
      GetSysTimePPC(tp);  // contrary to the autodoc, this is not actually
                          // system time, but a linear monotonic clock
#else
      /*
      ** PowerUp
      */
      unsigned long long ppctime, tickspersec;
      if (!TimerBase) GlobalTimerInit();
      PPCGetTimerObject(TimerObj,PPCTIMERTAG_TICKSPERSEC,&tickspersec);
      PPCGetTimerObject(TimerObj,PPCTIMERTAG_CURRENTTICKS,&ppctime);
      tp->tv_sec = ppctime / tickspersec;
      tp->tv_usec = (ppctime % tickspersec) * 1000000 / tickspersec;   
#endif
   }

   return 0;
}

#if !defined(__amigaos4__) && !defined(__MORPHOS__)
/*
** libnix mktime() has broken leap year handling, so use this instead
*/
time_t mktime(struct tm *tm)
{
   static int mon[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   time_t  t;
   int notLeap,i;

   /*
    *  1976 was a leap year, take quad years from 1976, each
    *  366+365+365+365 days each.  Then adjust for the year
    */

   t = ((tm->tm_year - 76) / 4) * ((366 + 365 * 3) * 86400);

   /*
    *  compensate to make it work the same as unix time (unix time
    *  started 8 years earlier)
    */

   t += 2922 * 24 * 60 * 60;

   /*
    *  take care of the year within a four year set, add a day for
    *  the leap year if we are based at a year beyond it.
    */

   t += ((notLeap = (tm->tm_year - 76) % 4)) * (365 * 86400);

   if (notLeap) t += 86400;

   /*
    *  calculate days over months then days offset in the month
    */

   for (i = 0; i < tm->tm_mon; ++i) {
      t += mon[i] * 86400;
      if (i == 1 && notLeap == 0) t += 86400;
   }
   t += (tm->tm_mday - 1) * 86400;

   /*
    *  our time is from 1978, not 1976
    */

   t -= (365 + 366) * 86400;

   /*
    *  calculate hours, minutes, seconds
    */

   t += tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;

   return(t);
}
#endif
