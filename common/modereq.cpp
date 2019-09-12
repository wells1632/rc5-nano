/*
 * Copyright distributed.net 1997-2011 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Created by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
 * ---------------------------------------------------------------
 * This file contains functions for getting/setting/clearing
 * "mode" requests (--flush,--fetch etc) and the like. Client::Run() will
 * clear/run the modes when appropriate.
 * ---------------------------------------------------------------
*/
const char *modereq_cpp(void) {
  return "@(#)$Id: modereq.cpp,v 1.50 2012/05/13 09:32:55 stream Exp $";
}

//#define TRACE

#include "client.h"   //client class + CONTEST_COUNT
#include "baseincs.h" //basic #includes
#include "triggers.h" //RaiseRestartRequestTrigger/CheckExitRequestTriggerNoIO
#include "modereq.h"  //our constants
#include "util.h"     //trace

#include "disphelp.h" //"mode" DisplayHelp()
#include "cpucheck.h" //"mode" DisplayProcessorInformation()
#include "cliident.h" //"mode" CliIdentifyModules();
#include "selcore.h"  //"mode" selcoreSelftest(), selcoreBenchmark()
#include "selftest.h" //"mode" SelfTest()
#include "bench.h"    //"mode" Benchmark()
#include "buffbase.h" //"mode" UnlockBuffer(), ImportBuffer()
#include "buffupd.h"  //"mode" BufferUpdate()
#include "confmenu.h" //"mode" Configure()
#include "logstuff.h"  // Log()/LogScreen()

/* --------------------------------------------------------------- */

static struct
{
  int isrunning;
  int reqbits;
  const char *filetounlock;
  const char *filetoimport;
  const char *helpoption;
  unsigned long bench_projbits;
  unsigned long test_projbits;
  unsigned long stress_projbits;
  int cmdline_config; /* user passed --config opt, so don't do tty check */
} modereq = {0,0,(const char *)0,(const char *)0,(const char *)0,0,0,0,0};

/* --------------------------------------------------------------- */

int ModeReqIsProjectLimited(int mode, unsigned int contest_i)
{
  unsigned long l = 0;
  if (contest_i < CONTEST_COUNT)
  {
    if (mode == MODEREQ_BENCHMARK ||
        mode == MODEREQ_BENCHMARK_QUICK ||
        mode == MODEREQ_BENCHMARK_ALLCORE )
      l = modereq.bench_projbits;
    else if (mode == MODEREQ_TEST ||
             mode == MODEREQ_TEST_ALLCORE )
      l = modereq.test_projbits;
    else if (mode == MODEREQ_STRESS ||
             mode == MODEREQ_STRESS_ALLCORE )
      l = modereq.stress_projbits;
    l &= (1L<<contest_i);
  }
  return (l != 0);
}

/* --------------------------------------------------------------- */

int ModeReqLimitProject(int mode, unsigned int contest_i)
{
  unsigned long l;
  if (contest_i > CONTEST_COUNT)
    return -1;
  l = 1<<contest_i;
  if (l == 0) /* wrapped */
    return -1;
  if (mode == MODEREQ_BENCHMARK ||
      mode == MODEREQ_BENCHMARK_QUICK ||
      mode == MODEREQ_BENCHMARK_ALLCORE )
    modereq.bench_projbits |= l;
  else if (mode == MODEREQ_TEST ||
           mode == MODEREQ_TEST_ALLCORE )
    modereq.test_projbits |= l;
  else if (mode == MODEREQ_STRESS ||
           mode == MODEREQ_STRESS_ALLCORE)
    modereq.stress_projbits |= l;
  else
    return -1;
  return 0;
}

/* --------------------------------------------------------------- */

int ModeReqSetArg(int mode, const void *arg)
{
  if (mode == MODEREQ_UNLOCK)
    modereq.filetounlock = (const char *)arg;
  else if (mode == MODEREQ_IMPORT)
    modereq.filetoimport = (const char *)arg;
  else if (mode == MODEREQ_CMDLINE_HELP)
    modereq.helpoption = (const char *)arg;
  else if (mode == MODEREQ_CONFIG)
    modereq.cmdline_config = 1;
  else
    return -1;
  ModeReqSet(mode);
  return 0;
}

/* --------------------------------------------------------------- */

int ModeReqIsSet(int modemask)
{
  if (modemask == -1)
    modemask = MODEREQ_ALL;
  return (modereq.reqbits & modemask);
}

/* --------------------------------------------------------------- */

int ModeReqSet(int modemask)
{
  if (modemask == -1)
    modemask = MODEREQ_ALL;
  int oldmask = (modereq.reqbits & modemask);
  modereq.reqbits |= modemask;
  return oldmask;
}

/* --------------------------------------------------------------- */

int ModeReqClear(int modemask)
{
  int oldmask;
  if (modemask == -1)
  {
    oldmask = modereq.reqbits;
    memset( &modereq, 0, sizeof(modereq));
  }
  else
  {
    modemask &= MODEREQ_ALL;
    oldmask = (modereq.reqbits & modemask);
    modereq.reqbits ^= (modereq.reqbits & modemask);
  }
  return oldmask;
}

/* --------------------------------------------------------------- */

int ModeReqIsRunning(void)
{
  return (modereq.isrunning != 0);
}

/* --------------------------------------------------------------- */

int ModeReqRun(Client *client)
{
  int retval = 0;

  /* This exits if we don't see a CPU we want,
     Also saves us from floating point exceptions on GPU clients 
  */
  if ((modereq.reqbits & MODEREQ_NEEDS_CPU_MASK) != 0)
  {
    if (GetNumberOfDetectedProcessors() == 0)  // -1 is OK when OS doesn't support detection
      return 0;
  }

  if (++modereq.isrunning == 1)
  {
    int restart = ((modereq.reqbits & MODEREQ_RESTART) != 0);
    modereq.reqbits &= ~MODEREQ_RESTART;

    while ((modereq.reqbits & MODEREQ_ALL)!=0)
    {
      unsigned int bits = modereq.reqbits;
      if ((bits & (MODEREQ_BENCHMARK |
                   MODEREQ_BENCHMARK_QUICK |
                   MODEREQ_BENCHMARK_ALLCORE )) != 0)
      {
        do
        {
          unsigned int contest, benchsecs = 16;
          unsigned long sel_contests = modereq.bench_projbits;
          modereq.bench_projbits = 0;

          if ((bits & (MODEREQ_BENCHMARK_QUICK))!=0)
            benchsecs = 8;
          for (contest = 0; contest < CONTEST_COUNT; contest++)
          {
            if (CheckExitRequestTriggerNoIO())
              break;
            if (sel_contests == 0 /*none set==all set*/
                || (sel_contests & (1L<<contest)) != 0)
            {
              if ((bits & (MODEREQ_BENCHMARK_ALLCORE))!=0)
                if(client->corenumtotestbench < 0)
                  selcoreBenchmark( client, contest, benchsecs, -1 );
                else
                  selcoreBenchmark( client, contest, benchsecs, client->corenumtotestbench);
              else
                TBenchmark( client, contest, benchsecs, 0, NULL, NULL );
            }
          }
          Log("Compare and share your rates in the speeds database at\n"
              "http://www.distributed.net/speed/\n"
              "(benchmark rates are for a single processor core)\n");
        } while (!CheckExitRequestTriggerNoIO() && modereq.bench_projbits);
        retval |= (bits & (MODEREQ_BENCHMARK_QUICK | MODEREQ_BENCHMARK |
                           MODEREQ_BENCHMARK_ALLCORE));
        modereq.reqbits &= ~(MODEREQ_BENCHMARK_QUICK | MODEREQ_BENCHMARK |
                             MODEREQ_BENCHMARK_ALLCORE);
      }
      if ((bits & MODEREQ_CMDLINE_HELP) != 0)
      {
        DisplayHelp(modereq.helpoption);
        modereq.helpoption = (const char *)0;
        modereq.reqbits &= ~(MODEREQ_CMDLINE_HELP);
        retval |= (MODEREQ_CMDLINE_HELP);
      }
      if ((bits & (MODEREQ_CONFIG | MODEREQ_CONFRESTART)) != 0)
      {
        /* cmdline_config is set if there is an explicit --config on the cmdline */
        /* Configure() returns <0=error,0=exit+nosave,>0=exit+save */
        Configure(client,(!(!modereq.cmdline_config)) /* nottycheck */);
        /* it used to be such that restart would only be posted on an exit+save */
        /* but now we restart for other retvals too, otherwise the GUI windows */
        /* end up with half-assed content */
        if ((bits & MODEREQ_CONFRESTART) != 0)
          restart = 1;
        modereq.cmdline_config = 0;
        modereq.reqbits &= ~(MODEREQ_CONFIG | MODEREQ_CONFRESTART);
        retval |= (bits & (MODEREQ_CONFIG | MODEREQ_CONFRESTART));
      }
      if ((bits & (MODEREQ_FETCH | MODEREQ_FLUSH)) != 0)
      {
        if (client)
        {
          int domode = 0;
          int interactive = ((bits & MODEREQ_FQUIET) == 0);
          domode  = ((bits & MODEREQ_FETCH) ? BUFFERUPDATE_FETCH : 0);
          domode |= ((bits & MODEREQ_FLUSH) ? BUFFERUPDATE_FLUSH : 0);
          TRACE_BUFFUPD((0, "BufferUpdate: reason = ModeReqRun\n"));
          domode = BufferUpdate( client, domode, interactive );
          if (domode & BUFFERUPDATE_FETCH)
            retval |= MODEREQ_FETCH;
          if (domode & BUFFERUPDATE_FLUSH)
            retval |= MODEREQ_FLUSH;
          if (domode!=0 && (bits & MODEREQ_FQUIET) != 0)
            retval |= MODEREQ_FQUIET;
        }
        modereq.reqbits &= ~(MODEREQ_FETCH | MODEREQ_FLUSH | MODEREQ_FQUIET);
      }
      if ((bits & MODEREQ_IDENT) != 0)
      {
        CliIdentifyModules();
        modereq.reqbits &= ~(MODEREQ_IDENT);
        retval |= (MODEREQ_IDENT);
      }
      if ((bits & MODEREQ_UNLOCK) != 0)
      {
        if (modereq.filetounlock)
        {
          UnlockBuffer(modereq.filetounlock);
          modereq.filetounlock = (const char *)0;
        }
        modereq.reqbits &= ~(MODEREQ_UNLOCK);
        retval |= (MODEREQ_UNLOCK);
      }
      if ((bits & MODEREQ_IMPORT) != 0)
      {
        if (modereq.filetoimport && client)
        {
          BufferImportFileRecords(client, modereq.filetoimport, 1 /* interactive */);
          modereq.filetoimport = (const char *)0;
          retval |= (MODEREQ_IMPORT);
        }
        modereq.reqbits &= ~(MODEREQ_IMPORT);
      }
      if ((bits & MODEREQ_CPUINFO) != 0)
      {
        DisplayProcessorInformation();
        modereq.reqbits &= ~(MODEREQ_CPUINFO);
        retval |= (MODEREQ_CPUINFO);
      }
      if ((bits & (MODEREQ_TEST | MODEREQ_TEST_ALLCORE)) != 0)
      {

        int testfailed = 0;
        do
        {
          unsigned int contest;
          unsigned long sel_contests = modereq.test_projbits;
          modereq.test_projbits = 0;

          for (contest = 0; !testfailed && contest < CONTEST_COUNT; contest++)
          {
            if (CheckExitRequestTriggerNoIO())
            {
              testfailed = 1;
              break;
            }
            if (sel_contests == 0 /*none set==all set*/
                || (sel_contests & (1L<<contest)) != 0)
            {
              if ((bits & (MODEREQ_TEST_ALLCORE)) != 0)
              {
                if (client->corenumtotestbench < 0)
                {
                  if (selcoreSelfTest( client, contest, -1 ) < 0)
                    testfailed = 1;
                }
                else
                {
                  if (selcoreSelfTest( client, contest, client->corenumtotestbench ) < 0)
                    testfailed = 1;
                }
              }
              else if ( SelfTest( client, contest ) < 0 )
                testfailed = 1;
            }
          }
        } while (!testfailed && modereq.test_projbits);
        retval |= (MODEREQ_TEST|MODEREQ_TEST_ALLCORE);
        modereq.reqbits &= ~(MODEREQ_TEST|MODEREQ_TEST_ALLCORE);
      }
      if ((bits & (MODEREQ_STRESS | MODEREQ_STRESS_ALLCORE)) != 0)
      {

        int testfailed = 0;
        do
        {
          unsigned int contest;
          unsigned long sel_contests = modereq.stress_projbits;
          modereq.stress_projbits = 0;

          for (contest = 0; !testfailed && contest < CONTEST_COUNT; contest++)
          {
            if (CheckExitRequestTriggerNoIO())
            {
              testfailed = 1;
              break;
            }
            if (sel_contests == 0 /*none set==all set*/
                || (sel_contests & (1L<<contest)) != 0)
            {
              if ((bits & (MODEREQ_STRESS_ALLCORE)) != 0)
              {
                if (client->corenumtotestbench < 0)
                {
                  if (selcoreStressTest( client, contest, -1 ) < 0)
                    testfailed = 1;
                }
                else
                {
                  if (selcoreStressTest( client, contest, client->corenumtotestbench ) < 0)
                    testfailed = 1;
                }
              }
              else if ( StressTest( client, contest ) < 0 )
                testfailed = 1;
            }
          }
        } while (!testfailed && modereq.stress_projbits);
        retval |= (MODEREQ_STRESS|MODEREQ_STRESS_ALLCORE);
        modereq.reqbits &= ~(MODEREQ_STRESS|MODEREQ_STRESS_ALLCORE);
      }
      if ((bits & MODEREQ_VERSION) != 0)
      {
        /* the requested information already has been printed */
        modereq.reqbits &= ~(MODEREQ_VERSION);
        retval |= (MODEREQ_VERSION);
      }
      if (CheckExitRequestTriggerNoIO())
      {
        restart = 0;
        break;
      }
    } //end while

    if (restart)
      RaiseRestartRequestTrigger();
  } //if (++isrunning == 1)

  modereq.isrunning--;
  return retval;
}

