/*
 * Copyright distributed.net 1997-2013 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Written by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
 * This module contains functions for raising/checking flags normally set
 * (asynchronously) by user request. Encapsulating the flags in
 * functions has two benefits: (1) Transparency: the caller doesn't
 * (_shouldn't_) need to care whether the triggers are async from signals
 * or polled. (2) Portability: we don't need a bunch of #if (CLIENT_OS...)
 * sections preceding every signal variable check. As such, someone writing
 * new code doesn't need to ensure that someone else's signal handling isn't
 * affected, and inversely, that coder doesn't need to check if his platform
 * is affected by every itty-bitty change. (3) Modularity: gawd knows we need
 * some of this. (4) Extensibility: hup, two, three, four...  - cyp
*/

const char *triggers_cpp(void) {
return "@(#)$Id: triggers.cpp,v 1.46 2013/12/15 17:11:00 zebe Exp $"; }

/* ------------------------------------------------------------------------ */

//#define TRACE
#include "cputypes.h"
#include "baseincs.h"  // basic (even if port-specific) #includes
#include "pathwork.h"  // GetFullPathForFilename()
#include "clitime.h"   // CliClock()
#include "util.h"      // TRACE and utilxxx()
#include "logstuff.h"  // LogScreen()
#include "pollsys.h"   // RegQueuedPolledProcedures()
#include "triggers.h"  // keep prototypes in sync

/* ----------------------------------------------------------------------- */

#define PAUSEFILE_CHECKTIME_WHENON  (3)  //seconds
#define PAUSEFILE_CHECKTIME_WHENOFF (3*PAUSEFILE_CHECKTIME_WHENON)
#define EXITFILE_CHECKTIME          (PAUSEFILE_CHECKTIME_WHENOFF)

/* note that all flags are single bits */
#if !defined(TRIGSETBY_SIGNAL) /* defined like this in triggers.h */
#define TRIGSETBY_SIGNAL       0x01 /* signal or explicit call to raise */
#define TRIGSETBY_FLAGFILE     0x02 /* flag file */
#define TRIGSETBY_CUSTOM       0x04 /* something other than the above */
#endif
/* the following are internal and are exported as TRIGSETBY_CUSTOM */
#define TRIGPAUSEBY_APPACTIVE  0x10 /* pause due to app being active*/
#define TRIGPAUSEBY_SRCBATTERY 0x20 /* pause due to running on battery */
#define TRIGPAUSEBY_CPUTEMP    0x40 /* cpu temperature guard */

#if (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
# define CLISIGHANDLER_IS_SPECIAL 1
#endif

struct trigstruct
{
  const char *flagfile;
  struct { unsigned int whenon, whenoff; } pollinterval;
  unsigned int incheck; //recursion check
  void (*pollproc)(int io_cycle_allowed);
  volatile int trigger;
  int laststate;
  time_t nextcheck;
};

static struct
{
  int doingmodes;
  struct trigstruct exittrig;
  struct trigstruct pausetrig;
  struct trigstruct huptrig;
  char pausefilebuf[128]; /* includes path */
  char exitfilebuf[128];
  int overrideinifiletime;
  time_t nextinifilecheck;
  unsigned long currinifiletime;
  char inifile[128];
  char pauseplistbuffer[128];
  const char *pauseplist[16];
  int lastactivep;
  int pause_if_no_mains_power;
  struct
  {
    u32 lothresh, hithresh; /* in Kelvin */
    int marking_high; /* we were >= high, waiting for < lowthresh */
  } cputemp;
} trigstatics;

// -----------------------------------------------------------------------

static void __assert_statics(void)
{
  static int initialized = -1;
  if (initialized == -1)
  {
    memset( &trigstatics, 0, sizeof(trigstatics) );
    initialized = +1;
  }
}

// -----------------------------------------------------------------------

static int __trig_raise(struct trigstruct *trig )
{
  int oldstate;
  __assert_statics();
  oldstate = trig->trigger;
  trig->trigger |= TRIGSETBY_SIGNAL;
  return oldstate;
}

static int __trig_clear(struct trigstruct *trig )
{
  int oldstate;
  __assert_statics();
  oldstate = trig->trigger;
  trig->trigger &= ~TRIGSETBY_SIGNAL;
  return oldstate;
}

int RaiseExitRequestTrigger(void)
{ return __trig_raise( &trigstatics.exittrig ); }
int RaiseRestartRequestTrigger(void)
{ RaiseExitRequestTrigger(); return __trig_raise( &trigstatics.huptrig ); }

#ifndef CLISIGHANDLER_IS_SPECIAL

/* used internally */
static int ClearRestartRequestTrigger(void) {
  return __trig_clear( &trigstatics.huptrig );
}

#endif /* !CLISIGHANDLER_IS_SPECIAL */

int RaisePauseRequestTrigger(void)
{ return __trig_raise( &trigstatics.pausetrig ); }
int ClearPauseRequestTrigger(void)
{
  int oldstate = __trig_clear( &trigstatics.pausetrig );
  #if 0
  if ((trigstatics.pausetrig.trigger & TRIGSETBY_FLAGFILE)!=TRIGSETBY_FLAGFILE
     && trigstatics.pausetrig.flagfile)
  {
    if (access( trigstatics.pausetrig.flagfile, 0 ) == 0)
    {
      unlink( trigstatics.pausetrig.flagfile );
      trigstatics.pausetrig.trigger &= ~TRIGSETBY_FLAGFILE;
    }
  }
  #endif
  return oldstate;
}
int CheckExitRequestTriggerNoIO(void)
{
  __assert_statics();
  if (trigstatics.exittrig.pollproc)
#ifdef CRUNCHERS_CALL_THIS_DIRECTLY
    (*trigstatics.exittrig.pollproc)(0 /* io_cycle_NOT_allowed */);
#else
    (*trigstatics.exittrig.pollproc)(1 /* io_cycle_IS_allowed */);
#endif
  return (trigstatics.exittrig.trigger);
}
int CheckPauseRequestTriggerNoIO(void)
{ __assert_statics();
  return ((trigstatics.pausetrig.trigger&(TRIGSETBY_SIGNAL|TRIGSETBY_FLAGFILE))
         |((trigstatics.pausetrig.trigger&
           (~(TRIGSETBY_SIGNAL|TRIGSETBY_FLAGFILE)))?(TRIGSETBY_CUSTOM):(0)));
}
int CheckRestartRequestTriggerNoIO(void)
{ __assert_statics(); return (trigstatics.huptrig.trigger); }

// -----------------------------------------------------------------------

void *RegisterPollDrivenBreakCheck( register void (*proc)(int) )
{
  register void (*oldproc)(int);
  __assert_statics();
  oldproc = trigstatics.exittrig.pollproc;
  trigstatics.exittrig.pollproc = proc;
  return (void *)oldproc;
}

// -----------------------------------------------------------------------

static void __PollExternalTrigger(struct trigstruct *trig, int undoable)
{
  __assert_statics();
  if ((undoable || (trig->trigger & TRIGSETBY_FLAGFILE) == 0) && trig->flagfile)
  {
    struct timeval tv;
    if (CliClock(&tv) == 0)
    {
      time_t now = tv.tv_sec;
      if (now >= trig->nextcheck)
      {
        if ( access( trig->flagfile, 0 ) == 0 )
        {
          trig->nextcheck = now + (time_t)trig->pollinterval.whenon;
          trig->trigger |= TRIGSETBY_FLAGFILE;
        }
        else
        {
          trig->nextcheck = now + (time_t)trig->pollinterval.whenoff;
          trig->trigger &= ~TRIGSETBY_FLAGFILE;
        }
      }
    }
  }
  return;
}

// -----------------------------------------------------------------------

static unsigned long __get_file_time(const char *filename)
{
  unsigned long filetime = 0; /* returns zero on error */
  struct stat statblk;
  if (stat( filename, &statblk ) == 0)
    filetime = (unsigned long)statblk.st_mtime;
  return filetime;
}

static void __CheckIniFileChangeStuff(void)
{
  __assert_statics();
  if (trigstatics.inifile[0]) /* have an ini filename? */
  {
    struct timeval tv;
    if (CliClock(&tv) == 0)
    {
      time_t now = tv.tv_sec;
      if (now > trigstatics.nextinifilecheck)
      {
        unsigned long filetime = __get_file_time(trigstatics.inifile);
        trigstatics.nextinifilecheck = now + ((time_t)5);
        if (filetime)
        {
          if (trigstatics.overrideinifiletime > 0)
          {
            trigstatics.currinifiletime = 0;
            trigstatics.overrideinifiletime--;
          }
          else if (!trigstatics.currinifiletime)     /* first time */
          {
            trigstatics.currinifiletime = filetime;
          }
          else if (trigstatics.currinifiletime == 1)
          {                                   /* got change some time ago */
            RaiseRestartRequestTrigger();
            trigstatics.currinifiletime = 0;
            trigstatics.nextinifilecheck = now + ((time_t)60);
          }
          else if (filetime != trigstatics.currinifiletime)
          {                                                /* mark change */
            trigstatics.currinifiletime = 1;
          }
        }
      }
    }
  }
  return;
}

// -----------------------------------------------------------------------

static const char *__mangle_pauseapp_name(const char *name, int unmangle_it )
{
  DNETC_UNUSED_PARAM(unmangle_it);

  #if ((CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16))
  /* these two are frequently used 16bit apps that aren't visible (?)
     to utilGetPIDList so they are searched for by window class name,
     which is a unique identifier so we can find/tack them on to the
     end of the pauseplist in __init. (They used to be hardcoded in
     the days before utilGetPIDList)
  */
  if (winGetVersion() >= 400 && winGetVersion() < 2000) /* win9x only */
  {
    static const char *app2wclass[] = { "scandisk",  "#ScanDskWDlgClass",
                                        "scandiskw", "#ScanDskWDlgClass",
                                        "defrag",    "#MSDefragWClass1" };
    unsigned int app;
    if (unmangle_it)
    {
      TRACE_OUT((+1,"x1: demangle: '%s'\n",name));
      for (app = 0; app < (sizeof(app2wclass)/sizeof(app2wclass[0])); app+=2)
      {
        if ( strcmp( name, app2wclass[app+1]) == 0)
        {
          name = app2wclass[app+0];
          break;
        }
      }
      TRACE_OUT((-1,"x2: demangle: '%s'\n",name));
    }
    else /* this only happens once per InitializeTriggers() */
    {
      unsigned int bpos, blen;
      blen = bpos = strlen( name );
      while (bpos>0 && name[bpos-1]!='\\' && name[bpos-1]!='/' && name[bpos-1]!=':')
        bpos--;
      blen -= bpos;
      if (blen > 3 && strcmpi(&name[bpos+(blen-4)],".exe") == 0)
        blen-=4;        /* only need to look for '.exe' since all are .exe */
      TRACE_OUT((+1,"x1: mangle: '%s', pos=%u,len=%u\n",name,bpos,blen));
      for (app = 0; app < (sizeof(app2wclass)/sizeof(app2wclass[0])); app += 2)
      {
        if ( memicmp( name+bpos, app2wclass[app+0], blen) == 0)
        {
          name = app2wclass[app+1];
          break;
        }
      }
      TRACE_OUT((-1,"x2: mangle: '%s'\n",name));
    }
  } /* win9x? */
  #endif /* ((CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16)) */

  return name;
}

// -----------------------------------------------------------------------

#if (CLIENT_OS == OS_NETBSD) && (CLIENT_CPU == CPU_X86)
// for apm support in __IsRunningOnBattery
#include <machine/apmvar.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#elif ((CLIENT_OS == OS_FREEBSD) || (CLIENT_OS == OS_DRAGONFLY)) && \
      (CLIENT_CPU == CPU_X86)
#include <fcntl.h>
#include <machine/apm_bios.h>

#elif (CLIENT_OS == OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#if (CLIENT_CPU != CPU_PPC)
#include <Availability.h> /* only available in 10.6+ */
#endif
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
#include <IOKit/ps/IOPowerSources.h>
#else
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <mach/mach_init.h> /* for bootstrap_port */
#endif

#elif (CLIENT_OS == OS_IOS)
/* TBD */

#elif (CLIENT_OS == OS_MORPHOS)
int morphos_isrunningonbattery(void);
LONG morphos_cputemp(void);
#endif

static int __IsRunningOnBattery(void) /*returns 0=no, >0=yes, <0=err/unknown*/
{
  if (trigstatics.pause_if_no_mains_power)
  {
    #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    static FARPROC getsps = ((FARPROC)-1);
    if (getsps == ((FARPROC)-1))
    {
      HMODULE hKernel = GetModuleHandle("kernel32.dll");
      if (hKernel)
        getsps = GetProcAddress( hKernel, "GetSystemPowerStatus");
      else
        getsps = (FARPROC)0;
    }
    if (!getsps)
      trigstatics.pause_if_no_mains_power = 0;
    else
    {
      SYSTEM_POWER_STATUS sps;
      sps.ACLineStatus = 255;
      if ((*((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))getsps))(&sps))
      {
        TRACE_OUT((0,"sps: ACLineStatus = 0x%02x, BatteryFlag = 0x%02x\n",sps.ACLineStatus,sps.BatteryFlag));
        if (sps.ACLineStatus == 1) /* AC power is online */
          return 0; /* no, we are not on battery */
        if (sps.ACLineStatus == 0) /* AC power is offline */
          return 1; /* yes, we are on battery */
        /* third condition is 0xff ("unknown"), so fall through */
      }
    }
    #elif ((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && \
          ((CLIENT_CPU == CPU_X86) || (CLIENT_CPU == CPU_AMD64))
    {
      // requires the ac kernel module
      int fd = open("/sys/class/power_supply/AC/online", O_RDONLY);
      if (fd == -1) {
      } else {
        char buffer[16];
        ssize_t readsz = read( fd, buffer, sizeof(buffer));
        close(fd);
        if (readsz > 0 && ((size_t)readsz) < sizeof(buffer)) {
          int online;
          buffer[readsz-1] = '\0';
          if (sscanf(buffer, "%d\n", &online) == 1) {
            if (online == 1)
              return 0; // AC online
            else if (online == 0)
              return 1; // AC offline
          }
        }
      }
    }
    #elif ((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && \
          (CLIENT_CPU == CPU_X86)
    {
      static long time_last = 0;
      long time_now = (time(0)/60); /* not more than once per minute */
      if (time_now != time_last)
      {
        int disableme= 1; // if this is still set when we get to the end
                          // then disable further apm checking
        char buffer[256]; // must be big enough for complete read of /proc/apm
                          // nn.nn nn.nn 0xnn 0xnn 0xnn 0xnn [-]nnn% [-]nnnn *s\n
        int readsz = -1;
        int fd = open( "/proc/apm", O_RDONLY );

        time_last = time_now;

        if (fd == -1)
        {
          if (errno == ENOMEM || errno == EAGAIN) /*ENOENT,ENXIO,EIO,EPERM */
            disableme = 0;  /* ENOMEM because apm kmallocs a struct per open */

          TRACE_OUT((0,"sps: open(\"/proc/apm\",O_RDONLY) => %s, disableme=%d\n", strerror(errno), disableme));
          #if defined(TRACE) /* real-life example (1.2 is similar) */
          readsz = strlen(strcpy(buffer, "1.13 1.2 0x07 0xff 0xff 0xff -1% -1 ?"));
          #endif
        }
        else
        {
          readsz = read( fd, buffer, sizeof(buffer));
          close(fd);
        }

        /* read should never fail for /proc/apm, and the size must be less
           than sizeof(buffer) otherwise its some /proc/apm that we
           don't know how to parse.
        */
        if (readsz > 0 && ((unsigned int)readsz) < sizeof(buffer))
        {
          unsigned int drv_maj, drv_min; /* "1.2","1.9","1.10","1.12,"1.13" etc*/
          int bios_maj, bios_min; /* %d.%d */
          int bios_flags, ac_line_status, batt_status, batt_flag; /* 0x%02x */
          /* remaining fields are percentage, time_units, units. (%d %d %s) */

          buffer[readsz-1] = '\0';
          if (sscanf( buffer, "%u.%u %d.%d 0x%02x 0x%02x 0x%02x 0x%02x",
                              &drv_maj, &drv_min, &bios_maj, &bios_min,
                              &bios_flags, &ac_line_status,
                              &batt_status, &batt_flag ) == 8 )
          {
            TRACE_OUT((0,"sps: drvver:%u.%u biosver:%d.%d biosflags:0x%02x "
                         "ac_line_status=0x%02x, batt_status=0x%02x\n",
                         drv_maj, drv_min, bios_maj, bios_min, bios_flags,
                         ac_line_status, batt_status ));
            if (drv_maj == 1)
            {
              #define _APM_16_BIT_SUPPORT   (1<<0)
              #define _APM_32_BIT_SUPPORT   (1<<1)
              //      _APM_IDLE_SLOWS_CLOCK (1<<2)
              #define _APM_BIOS_DISABLED    (1<<3)
              //      _APM_BIOS_DISENGAGED  (1<<4)
              if ((bios_flags & (_APM_16_BIT_SUPPORT | _APM_32_BIT_SUPPORT))!=0
                && (bios_flags & _APM_BIOS_DISABLED) == 0)
              {
                disableme = 0;
                ac_line_status &= 0xff; /* its a char */
                /* From /usr/src/[]/arch/i386/apm.c for (1.2)1996-(1.13)2/2000
                   3) AC Line Status:
                      0x00: Off-line
                      0x01: On-line
                      0x02: On backup-power (APM BIOS 1.1+ only)
                      0xff: Unknown
                */
                if (ac_line_status == 1)
                  return 0; /* we are not on battery */
                if (ac_line_status != 0xff) /* 0x00, 0x02 */
                  return 1; /* yes we are on battery */
                /* fallthrough, return -1 */
              }
            } /* drv_maj == 1 */
          } /* sscanf() == 8 */
        } /* readsz */

        if (disableme) /* disable further checks */
        {
          TRACE_OUT((0,"sps: further pause_if_no_mains_power checks now disabled\n"));
          trigstatics.pause_if_no_mains_power = 0;
        }
        // Hang on, don't disable yet, let's try acpi first. STAN
        else
        {
          // first check if the relevant directory can be opened
          int dirfd = open( "/proc/acpi/ac_adapter/", O_DIRECTORY );
          close(dirfd);
          if ( dirfd == -1 )
            {
              //nope, oh well we tried
              TRACE_OUT((0,"sps: further pause_if_no_mains_power checks now disabled\n"));
              trigstatics.pause_if_no_mains_power = 0;
            }
          else
            {
              // now check if it has subirectories (should have 1 per PSU)
              int dircount,i;
              struct direct **files;
              dircount = scandir("/proc/acpi/ac_adapter/", &files, NULL, NULL);
              if (dircount != 3) 
                // assumptions : 3 = 1 actual subdir + . + ..
                // less subdirs = acpi not set up
                // more then 3 = more then 1 PSU, so not a laptop, so who cares anyway
                {
                  TRACE_OUT((0,"sps: further pause_if_no_mains_power checks now disabled\n"));
                  trigstatics.pause_if_no_mains_power = 0; 
                }
              else
                {
                  // ok now let's check what the lonely PSU says 
                  for (i=1;i<dircount+1;++i)
                    {
                      if ((strcmp(files[i-1]->d_name, ".") != 0) && (strcmp(files[i-1]->d_name, "..") != 0))
                        {
                          char acpi_path[256];
                          snprintf(acpi_path,sizeof(acpi_path),"/proc/acpi/ac_adapter/%s/state",files[i-1]->d_name);
                          char bufferb[40];
                          int readsz = -1;
                          int state = open(acpi_path, O_RDONLY );
                          readsz = read(state, bufferb, sizeof(bufferb));
                          close(state);
                          if(strstr(bufferb,"on-line")) {
                            return 0; // we are not on battery 
                          } else {   
                              return 1; // yes we are on battery
                          }
                        }
                    }
                } 
            } 
        }
      } 
    } /* #if (linux & cpu_x86) */

    #elif (CLIENT_OS == OS_LINUX) && ((CLIENT_CPU == CPU_POWERPC) || (CLIENT_CPU == CPU_CELLBE))
    {
      static long time_last = 0;
      long time_now = (time(0)/60); /* not more than once per minute */
      if (time_now != time_last)
      {
        int disableme = 1; // if this is still set when we get to the end
                           // then disable further apm checking
        FILE *fd = fopen("/proc/pmu/info", "r" );
        if ( fd > 0)
        {
          char buffer[256];
          int ac_status = -1;
          while(fgets(buffer, sizeof(buffer), fd))
          {
            buffer[sizeof(buffer) - 1] = '\0';
            if (strstr(buffer, "AC Power") == buffer)
            {
              if (sscanf(buffer, "AC Power               : %d",
                         &ac_status) == 1)
              {
                disableme = 0;
                break;
              }
            }
          } /* while */
          fclose (fd);
          if (ac_status == 1)
            return 0; /* not running from battery */
          if (ac_status == 0)
            return 1; /* are running on battery */
          /* fallthrough, return -1 */
        } /* if (fd) */

        if (disableme) /* disable further checks */
        {
          TRACE_OUT((0,"sps: further pause_if_no_mains_power checks now disabled\n"));
          trigstatics.pause_if_no_mains_power = 0;
        }
      } /* if time */
    } /* #if (linux & cpu_ppc) */

    #elif ((CLIENT_OS == OS_FREEBSD) || (CLIENT_OS == OS_DRAGONFLY)) && \
          (CLIENT_CPU == CPU_X86)
    {
      /* This is sick and sooo un-BSD like! Tatsumi Hokosawa must have
         been dealing too much with linux and forgot all about sysctl. :)
      */
      int disableme = 1; /* assume further apm checking should be disabled */
      int fd = open("/dev/apm", O_RDONLY);
      if (fd != -1)
      {
        #if defined(APMIO_GETINFO_OLD) /* (__FreeBSD__ >= 3) */
        struct apm_info_old info;      /* want compatibility for 2.2 */
        int whichioctl = APMIO_GETINFO_OLD;
        #else
        struct apm_info info;
        int whichioctl = APMIO_GETINFO;
        #endif
        disableme = 0;

        memset( &info, 0, sizeof(info));
        if (ioctl(fd, whichioctl, (caddr_t)&info, 0 )!=0)
        {
          disableme = 1;
          info.ai_acline = 255; /* what apm returns for "unknown" */
        }
        else
        {
          TRACE_OUT((+1,"APM check\n"));
          TRACE_OUT((0,"aiop->ai_major = %d\n", info.ai_major));
          TRACE_OUT((0,"aiop->ai_minor = %d\n", info.ai_minor));
          TRACE_OUT((0,"aiop->ai_acline = %d\n",  info.ai_acline));
          TRACE_OUT((0,"aiop->ai_batt_stat = %d\n", info.ai_batt_stat));
          TRACE_OUT((0,"aiop->ai_batt_life = %d\n", info.ai_batt_life));
          TRACE_OUT((0,"aiop->ai_status = %d\n", info.ai_status));
          TRACE_OUT((-1,"conclusion: AC line state: %s\n", ((info.ai_acline==0)?
                 ("offline"):((info.ai_acline==1)?("online"):("unknown"))) ));
        }
        close(fd);

        if (info.ai_acline == 1)
          return 0; /* We have AC power */
        if (info.ai_acline == 0)
          return 1; /* no AC power */
      }
      if (disableme)
      {
        /* possible causes for a disable are
        ** EPERM: no permission to open /dev/apm) or
        ** ENXIO: apm device not configured, or disabled [kern default],
        ** or (for ioctl()) real<->pmode transition or bios error.
        */
        TRACE_OUT((0,"pause_if_no_mains_power check error: %s\n", strerror(errno)));
        TRACE_OUT((0,"disabling further pause_if_no_mains_power checks\n"));
        trigstatics.pause_if_no_mains_power = 0;
      }
    } /* freebsd */
    #elif (CLIENT_OS == OS_NETBSD) && (CLIENT_CPU == CPU_X86)
    {
      struct apm_power_info buff;
      int fd;
      #define _PATH_APM_DEV "/dev/apm"

      fd = open(_PATH_APM_DEV, O_RDONLY);

      if (fd != -1) {
        if (ioctl(fd, APM_IOC_GETPOWER, &buff) == 0) {
          close(fd);
          TRACE_OUT((0,"sps: ACLineStatus = 0x%08x, BatteryFlag = 0x%08x\n",
            buff.ac_state, buff.battery_state));

          if (buff.ac_state == APM_AC_ON)
            return 0;       /* we have AC power */
          if (buff.ac_state == APM_AC_OFF)
            return 1;       /* we don't have AC */
        }
      }
      close(fd);
      // We seem to have no apm driver in the kernel, so disable it.
      trigstatics.pause_if_no_mains_power = 0;
    } /* #if (NetBSD && i386) */
    #elif (CLIENT_OS == OS_MACOSX)
    #if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1070
    /* cheaper, easier check available in 10.7+ */
    if (IOPSGetTimeRemainingEstimate() != kIOPSTimeRemainingUnlimited) {
      return 1; /* we don't have AC */
    } else {
      return 0; /* we have AC power */
    }
    #else
    /* old way for 10.0-10.6 */
    mach_port_t master;
    /* Initialize the connection to IOKit */
    if( IOMasterPort(bootstrap_port, &master) == kIOReturnSuccess )
    {
      io_connect_t pmcon = IOPMFindPowerManagement(master);
      if(pmcon!=0)
      {
        CFArrayRef cfarray;
        /* Get the battery State information */
        if( IOPMCopyBatteryInfo(master, &cfarray) == kIOReturnSuccess)
        {
          long flags;
          CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(cfarray, 0);
          CFNumberRef cfnum = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR(kIOBatteryFlagsKey));
          CFNumberGetValue(cfnum, kCFNumberLongType, &flags);
          CFRelease(cfarray);
          IOServiceClose(pmcon);
          if( flags & kIOBatteryChargerConnect )
            return 0; /* we have AC power */
          else
            return 1; /* we don't have AC */
        } else {
          /* Only do this if we -haven't- executed the if{} above. */
          IOServiceClose(pmcon);
        } /* if( IOPMCopyBatteryInfo(master, &cfarray) == kIOReturnSuccess) */
      } /* if(pmcon!=0)*/
    } /* if( IOMasterPort(bootstrap_port, &master) == kIOReturnSuccess ) */
    /* We dont seem to have a PowerManager, disable battery checking. */
    trigstatics.pause_if_no_mains_power = 0;
    #endif /* #if OS X 10.7+ */
    #elif (CLIENT_OS == OS_MORPHOS)
    {
      int status = morphos_isrunningonbattery();

      if (status == -1) /* disable further checks */
      {
        trigstatics.pause_if_no_mains_power = 0;
      }
      else
      {
        return status;
      }
    }
    #endif
  }
  return -1; /* unknown */
}

// -----------------------------------------------------------------------

#if (CLIENT_OS == OS_MACOSX)
s32 macosx_cputemp();
#elif (CLIENT_OS == OS_DEC_UNIX)
extern "C" int dunix_cputemp();
#elif (CLIENT_CPU == CPU_ATI_STREAM)
int stream_cputemp();
#endif

static int __CPUTemperaturePoll(void) /*returns 0=no, >0=yes, <0=err/unknown*/
{
  s32 lowthresh = (s32)trigstatics.cputemp.lothresh;
  s32 highthresh = (s32)trigstatics.cputemp.hithresh;
  if (lowthresh > 0 && highthresh >= lowthresh) /* otherwise values are invalid */
  {
    /* read the cpu temp in Kelvin. For multiple cpus, gets one
       with highest temp. On error, returns < 0.
       Note that cputemp is in Kelvin, if your OS returns a value in
       Farenheit or Celsius, see _init_cputemp for conversion functions.
       The temperature is encoded in fixed-point format to allow two
       digits after the decimal point.
    */
    s32 cputemp = -1;
    #if (CLIENT_OS == OS_MACOSX)
      cputemp = macosx_cputemp();
      if (cputemp < 0) {
        trigstatics.cputemp.hithresh=0; // disable checking on failure
        trigstatics.cputemp.lothresh=0;
        trigstatics.cputemp.marking_high = 0;
      }
    #elif (CLIENT_OS == OS_DEC_UNIX)
      if ((cputemp = dunix_cputemp()) < 0) {
        trigstatics.cputemp.hithresh=0; // disable checking on failure
        trigstatics.cputemp.lothresh=0;
        trigstatics.cputemp.marking_high = 0;
      }
      else {
        cputemp = cputemp * 100 + 15;   // Convert to fixed-point format
      }
    #elif (CLIENT_CPU == CPU_ATI_STREAM)
      cputemp = stream_cputemp();
    #elif (CLIENT_OS == OS_MORPHOS)
      cputemp = morphos_cputemp();
      if (cputemp < 0) {
        trigstatics.cputemp.hithresh=0; // disable checking on failure
        trigstatics.cputemp.lothresh=0;
        trigstatics.cputemp.marking_high = 0;
      }
    #elif 0 /* other client_os */
      cputemp = fooyaddablahblahbar();
    #endif
    if (cputemp < 0) /* error */
      return -1;
    else if (cputemp >= highthresh)
      trigstatics.cputemp.marking_high = 1;
    else if (cputemp < lowthresh)
      trigstatics.cputemp.marking_high = 0;
  }
  else if (highthresh < lowthresh) /* values are invalid */
  {
    Log("Temperature monitoring disabled (high < low threshold).\n");
    trigstatics.cputemp.hithresh=0; // disable checking on failure
    trigstatics.cputemp.lothresh=0;
    trigstatics.cputemp.marking_high = 0;
    return -1;
  }
  return trigstatics.cputemp.marking_high;
}

// -----------------------------------------------------------------------

#if defined(CLISIGHANDLER_IS_SPECIAL) || \
    (CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_RISCOS) || \
    (CLIENT_OS == OS_DOS)

static void __PollDrivenBreakCheck(int io_cycle_allowed)
{
  /* io_cycle_allowed is non-zero when called through CheckExitRequestTrigger
  ** and is zero when called through CheckExitRequestTriggerNoIO() */
  DNETC_UNUSED_PARAM(io_cycle_allowed);

  #if (CLIENT_OS == OS_RISCOS)
/* This hs no equivalent in Unixlib but strange as it is, it seems to be not
   necessary */
//  if (_kernel_escape_seen())
//      RaiseExitRequestTrigger();
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  ULONG trigs;
  if ( (trigs = amigaGetTriggerSigs()) )  // checks for ^C and other sigs
  {
    if ( trigs & DNETC_MSG_SHUTDOWN )
      RaiseExitRequestTrigger();
    if ( trigs & DNETC_MSG_RESTART )
      RaiseRestartRequestTrigger();
    if ( trigs & DNETC_MSG_PAUSE )
      RaisePauseRequestTrigger();
    if ( trigs & DNETC_MSG_UNPAUSE )
      ClearPauseRequestTrigger();
  }
  #elif (CLIENT_OS == OS_NETWARE)
  if (io_cycle_allowed)
    nwCliCheckForUserBreak(); //in nwccons.cpp
  #elif (CLIENT_OS == OS_WIN16) /* always thread safe */
    w32ConOut("");    /* benign call to keep ^C handling alive */
  #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  if (io_cycle_allowed)
    w32ConOut("");    /* benign call to keep ^C handling alive */
  #elif (CLIENT_OS == OS_DOS)
    _asm mov ah,0x0b  /* benign dos call (kbhit()) */
    _asm int 0x21     /* to keep int23h (^C) handling alive */
  #endif
  return;
}
#endif

// =======================================================================

int OverrideNextConffileChangeTrigger(void)
{
  __assert_statics();
  return (++trigstatics.overrideinifiletime);
}

int CheckExitRequestTrigger(void)
{
  __assert_statics();
  if (!trigstatics.exittrig.laststate && !trigstatics.exittrig.incheck)
  {
    ++trigstatics.exittrig.incheck;
    if ( !trigstatics.exittrig.trigger )
    {
      if (trigstatics.exittrig.pollproc)
        (*trigstatics.exittrig.pollproc)(1 /* io_cycle_allowed */);
    }
    if ( !trigstatics.exittrig.trigger )
      __PollExternalTrigger( &trigstatics.exittrig, 0 );
    if ( !trigstatics.exittrig.trigger )
      __CheckIniFileChangeStuff();
    if ( trigstatics.exittrig.trigger )
    {
      trigstatics.exittrig.laststate = 1;
      if (!trigstatics.doingmodes)
      {
        Log("*Break* %s\n",
          ( ((trigstatics.exittrig.trigger & TRIGSETBY_FLAGFILE)!=0)?
               ("(found exit flag file)"):
           ((trigstatics.huptrig.trigger)?("Restarting..."):("Shutting down..."))
            ) );
      }
    }
    --trigstatics.exittrig.incheck;
  }
  TRACE_OUT((0, "CheckExitRequestTrigger() = %d\n", trigstatics.exittrig.trigger));
  return( trigstatics.exittrig.trigger );
}

// -----------------------------------------------------------------------

int CheckRestartRequestTrigger(void)
{
  CheckExitRequestTrigger(); /* does both */
  return (trigstatics.huptrig.trigger);
}

// -----------------------------------------------------------------------

int CheckPauseRequestTrigger(void)
{
  __assert_statics();
  if ( CheckExitRequestTrigger() )   //only check if not exiting
    return 0;
  if (trigstatics.doingmodes)
    return 0;
  if ( (++trigstatics.pausetrig.incheck) == 1 )
  {
    const char *custom_now_active = "";
    const char *custom_now_inactive = "";
    const char *app_now_active = "";

    #if (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16)
    {
      custom_now_active = "(defrag running)";
      custom_now_inactive = "(defrag no longer running)";
      if (utilGetPIDList( "#MSDefragWClass1", NULL, 0 ) > 0)
        trigstatics.pausetrig.trigger |= TRIGSETBY_CUSTOM;
      else
        trigstatics.pausetrig.trigger &= ~TRIGSETBY_CUSTOM;
    }
    #endif

    if (trigstatics.pauseplist[0] != NULL)
    {
      int idx, nowcleared = -1;
      const char **pp = &trigstatics.pauseplist[0];

      /* the use of "nowcleared" is a hack for the sake of optimization
         so that we can sequence the code for the paused->unpause and
         the unpause->pause transitions and still avoid an extra call
         to utilGetPIDList() if we know the app is definitely not running.
      */
      if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_APPACTIVE) != 0)
      {
        idx = trigstatics.lastactivep;
        TRACE_OUT((+1,"y1: idx=%d,'%s'\n", idx, pp[idx]));
        if (utilGetPIDList( pp[idx], NULL, 0 ) <= 0)
        {
          /* program is no longer running. */
          Log("%s... ('%s' inactive)\n",
              (((trigstatics.pausetrig.laststate
                   & ~TRIGPAUSEBY_APPACTIVE)!=0)?("Pause level lowered"):
              ("Running again after pause")),
              __mangle_pauseapp_name(pp[idx],1 /* unmangle */) );
          trigstatics.pausetrig.trigger &= ~TRIGPAUSEBY_APPACTIVE;
          trigstatics.lastactivep = 0;
          nowcleared = idx;
        }
        TRACE_OUT((-1,"y2: nowcleared=%d\n", nowcleared));
      }
      if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_APPACTIVE) == 0)
      {
        idx = 0;
        while (pp[idx] != NULL)
        {
          TRACE_OUT((+1,"z1: %d, '%s'\n",idx, pp[idx]));
          if (idx == nowcleared)
          {
            /* if this matched, then we came from the transition
               above and know that this is definitely not running.
            */
          }
          else if (utilGetPIDList( pp[idx], NULL, 0 ) > 0)
          {
            /* Triggered program is now running. */
            trigstatics.pausetrig.trigger |= TRIGPAUSEBY_APPACTIVE;
            trigstatics.lastactivep = idx;
            app_now_active = __mangle_pauseapp_name(pp[idx],1 /* unmangle */);
            break;
          }
          TRACE_OUT((-1,"z2\n"));
          idx++;
        }
      }
    }

    {
      int isbat = __IsRunningOnBattery(); /* <0=unknown/err, 0=no, >0=yes */
      if (isbat > 0)
        trigstatics.pausetrig.trigger |= TRIGPAUSEBY_SRCBATTERY;
      else if (isbat == 0)
        trigstatics.pausetrig.trigger &= ~TRIGPAUSEBY_SRCBATTERY;
      /* otherwise error/unknown, so don't change anything */
    }
    {
        int cputemp = __CPUTemperaturePoll(); /* <0=unknown/err, 0=no, >0=yes */
        if (cputemp > 0)
            trigstatics.pausetrig.trigger |= TRIGPAUSEBY_CPUTEMP;
        else if (cputemp == 0)
            trigstatics.pausetrig.trigger &= ~TRIGPAUSEBY_CPUTEMP;
        /* otherwise error/unknown, so don't change anything */
    }

    __PollExternalTrigger( &trigstatics.pausetrig, 1 );

    /* +++ */

    if (trigstatics.pausetrig.laststate != trigstatics.pausetrig.trigger)
    {
      if ((trigstatics.pausetrig.trigger & TRIGSETBY_SIGNAL)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_SIGNAL)==0)
      {
        Log("Pause%sd... (user generated)\n",
             ((trigstatics.pausetrig.laststate)?(" level raise"):("")) );
        trigstatics.pausetrig.laststate |= TRIGSETBY_SIGNAL;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGSETBY_SIGNAL)==0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_SIGNAL)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGSETBY_SIGNAL;
        Log("%s... (user cleared)\n",
            ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
            ("Running again after pause")) );
      }
      if ((trigstatics.pausetrig.trigger & TRIGSETBY_FLAGFILE)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_FLAGFILE)==0)
      {
        Log("Pause%sd... (found flagfile)\n",
              ((trigstatics.pausetrig.laststate)?(" level raise"):("")) );
        trigstatics.pausetrig.laststate |= TRIGSETBY_FLAGFILE;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGSETBY_FLAGFILE)==0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_FLAGFILE)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGSETBY_FLAGFILE;
        Log("%s... (flagfile cleared)\n",
          ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
          ("Running again after pause")) );
      }
      if ((trigstatics.pausetrig.trigger & TRIGSETBY_CUSTOM)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_CUSTOM)==0)
      {
        Log("Pause%sd... %s\n",
             ((trigstatics.pausetrig.laststate)?(" level raise"):("")),
             custom_now_active );
        trigstatics.pausetrig.laststate |= TRIGSETBY_CUSTOM;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGSETBY_CUSTOM)==0 &&
          (trigstatics.pausetrig.laststate & TRIGSETBY_CUSTOM)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGSETBY_CUSTOM;
        Log("%s... %s\n",
          ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
          ("Running again after pause")), custom_now_inactive );
      }
      if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_APPACTIVE)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_APPACTIVE)==0)
      {
        Log("Pause%sd... ('%s' active)\n",
             ((trigstatics.pausetrig.laststate)?(" level raise"):("")),
             ((*app_now_active)?(app_now_active):("process")) );
        trigstatics.pausetrig.laststate |= TRIGPAUSEBY_APPACTIVE;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_APPACTIVE)==0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_APPACTIVE)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGPAUSEBY_APPACTIVE;
        /* message was already printed above
        //Log("%s... %s\n",
        //  ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
        //  ("Running again after pause")), "(process no longer active)" );
        */
      }
      if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_SRCBATTERY)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_SRCBATTERY)==0)
      {
        Log("Pause%sd... (No mains power)\n",
             ((trigstatics.pausetrig.laststate)?(" level raise"):("")) );
        trigstatics.pausetrig.laststate |= TRIGPAUSEBY_SRCBATTERY;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_SRCBATTERY)==0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_SRCBATTERY)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGPAUSEBY_SRCBATTERY;
        Log("%s... (Mains power restored)\n",
          ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
          ("Running again after pause")) );
      }
      if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_CPUTEMP)!=0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_CPUTEMP)==0)
      {
        Log("Pause%sd... (CPU temperature exceeds %u.%02uK)\n",
             ((trigstatics.pausetrig.laststate)?(" level raise"):("")),
               trigstatics.cputemp.hithresh / 100, trigstatics.cputemp.hithresh % 100);
        trigstatics.pausetrig.laststate |= TRIGPAUSEBY_CPUTEMP;
      }
      else if ((trigstatics.pausetrig.trigger & TRIGPAUSEBY_CPUTEMP)==0 &&
          (trigstatics.pausetrig.laststate & TRIGPAUSEBY_CPUTEMP)!=0)
      {
        trigstatics.pausetrig.laststate &= ~TRIGPAUSEBY_CPUTEMP;
        Log("%s... (CPU temperature below %u.%02uK)\n",
            ((trigstatics.pausetrig.laststate)?("Pause level lowered"):
            ("Running again after pause")), trigstatics.cputemp.lothresh / 100, 
            trigstatics.cputemp.lothresh % 100);
      }
      if (trigstatics.pausetrig.laststate == 0)
      {
        /* no longer paused, release all polled procedures that have been on hold */
        RegQueuedPolledProcedures();
      }
    }
  }
  --trigstatics.pausetrig.incheck;
  return CheckPauseRequestTriggerNoIO(); /* return public mask */
}

// =======================================================================

#if (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
#ifdef __amigaos4__
extern "C" void __check_abort(void)
#else
extern "C" void __chkabort(void)
#endif
{
  /* Disable SAS/C / GCC CTRL-C handing */
  return;
}

static void __init_signal_handlers( int doingmodes )
{
  DNETC_UNUSED_PARAM(doingmodes);

  __assert_statics();
  SetSignal(0L, SIGBREAKF_CTRL_C); // Clear the signal triggers
  RegisterPollDrivenBreakCheck( __PollDrivenBreakCheck );
}
#endif

// -----------------------------------------------------------------------

#if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
BOOL WINAPI CliSignalHandler(DWORD dwCtrlType)
{
  if ( dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_CLOSE_EVENT ||
       dwCtrlType == CTRL_SHUTDOWN_EVENT)
  {
    RaiseExitRequestTrigger();
    return TRUE;
  }
  else if (dwCtrlType == CTRL_BREAK_EVENT)
  {
    RaiseRestartRequestTrigger();
    return TRUE;
  }
  return FALSE;
}

static void __init_signal_handlers( int doingmodes )
{
  DNETC_UNUSED_PARAM(doingmodes);

  __assert_statics();
  SetConsoleCtrlHandler( /*(PHANDLER_ROUTINE)*/CliSignalHandler, FALSE );
  SetConsoleCtrlHandler( /*(PHANDLER_ROUTINE)*/CliSignalHandler, TRUE );
  RegisterPollDrivenBreakCheck( __PollDrivenBreakCheck );
}
#endif

// -----------------------------------------------------------------------

#ifndef CLISIGHANDLER_IS_SPECIAL
#if defined(SA_NOCLDSTOP) && (CLIENT_OS != OS_NETWARE)
static void (*SETSIGNAL(int signo, void (*proc)(int)))(int)
{
  struct sigaction sa, osa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  /* sa.sa_handler = proc; cast to get around c to c++ non-portability */
  *((void **)(&(sa.sa_handler))) = *((void **)(&proc));
  #if defined(SA_RESTART) /* SA_RESTART is not _POSIX_SOURCE */
  /* if (!sigismember(&_sigintr, signo)) */
    sa.sa_flags |= SA_RESTART;
  #endif
  if (sigaction(signo, &sa, &osa) < 0)
    return (void (*)(int))(SIG_ERR);
  return (void (*)(int))(osa.sa_handler);
}
#else
#define SETSIGNAL signal
#endif

// -----------------------------------------------------------------------

extern "C" void CliSignalHandler( int sig )
{
  #if defined(TRIGGER_PAUSE_SIGNAL)
  if (sig == TRIGGER_PAUSE_SIGNAL)
  {
    SETSIGNAL(sig,CliSignalHandler);
    RaisePauseRequestTrigger();
    return;
  }
  if (sig == TRIGGER_UNPAUSE_SIGNAL)
  {
    SETSIGNAL(sig,CliSignalHandler);
    ClearPauseRequestTrigger();
    return;
  }
  #endif
  #if defined(SIGHUP)
  if (sig == SIGHUP)
  {
    SETSIGNAL(sig,CliSignalHandler);
    RaiseRestartRequestTrigger();
    return;
  }
  #endif
  ClearRestartRequestTrigger();
//printf("\ngot sig %d\n", sig);
  RaiseExitRequestTrigger();

#if (CLIENT_OS != OS_FREEBSD) && (CLIENT_OS != OS_DEC_UNIX)
  SETSIGNAL(sig,SIG_IGN);
#endif
}
#endif //ifndef CLISIGHANDLER_IS_SPECIAL

#if (CLIENT_OS == OS_OS2) && defined(__WATCOMC__)
#include "dosqss.h"
void pascal __far16 MoreOS2Signals(USHORT sigArg, USHORT sigNumber)
{
  // Ack signal
  DosSetSigHandler(MoreOS2Signals, NULL, NULL, SIGA_ACKNOWLEDGE, sigNumber);
  if (sigNumber == SIG_PFLG_A) /* signals connected to user signal "A" */
  {
    switch (sigArg)
    {
      case DNETC_MSG_PAUSE:   RaisePauseRequestTrigger(); break;
      case DNETC_MSG_UNPAUSE: ClearPauseRequestTrigger(); break;
      case DNETC_MSG_RESTART: RaiseRestartRequestTrigger(); break;
    }
  }
}
#endif

// -----------------------------------------------------------------------

#if (((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS)) || \
   defined(HAVE_MULTICRUNCH_VIA_FORK)
/* forward all signals to the parent process */
static void sig_forward_to_parent( int nsig )
{
  kill(getppid(), nsig);
}
#endif

int TriggersSetThreadSigMask(void)
{
#if (((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS)) || \
    defined(HAVE_MULTICRUNCH_VIA_FORK)
  /* setup signal forwarding */

  int sigs[] = { SIGINT, SIGTERM, SIGKILL, SIGHUP, SIGCONT,
                 SIGTSTP, SIGTTIN, SIGTTOU };
  unsigned int nsig;
  for (nsig = 0; nsig < (sizeof(sigs)/sizeof(sigs[0])); nsig++)
  {
    SETSIGNAL( sigs[nsig], sig_forward_to_parent );
  }

#elif (CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_SUNOS) \
    || defined(_POSIX_THREADS_SUPPORTED)

  int sigs[] = { SIGINT, SIGTERM, SIGKILL, SIGHUP, SIGCONT,
                 SIGTSTP, SIGTTIN, SIGTTOU };
  unsigned int nsig;

  sigset_t signals_to_block;
  sigemptyset(&signals_to_block);

  for (nsig = 0; nsig < (sizeof(sigs)/sizeof(sigs[0])); nsig++)
  {
    sigaddset(&signals_to_block, sigs[nsig] );
  }

  #if defined(_POSIX_THREADS_SUPPORTED) /* must be first */
    #if (((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(_MIT_POSIX_THREADS)) \
       || (CLIENT_OS == OS_DGUX)
    /* nothing - no pthread_sigmask() and no alternative */
    #else
    pthread_sigmask(SIG_BLOCK, &signals_to_block, NULL);
    #endif
  #elif (CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_SUNOS)
  thr_sigsetmask(SIG_BLOCK, &signals_to_block, NULL);
  #elif defined(HAVE_MULTICRUNCH_VIA_FORK) || \
        (((CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID)) && defined(HAVE_KTHREADS))
  sigprocmask(SIG_BLOCK, &signals_to_block, NULL);
  #endif
#endif
  return 0;
}

// -----------------------------------------------------------------------

#ifndef CLISIGHANDLER_IS_SPECIAL
static void __init_signal_handlers( int doingmodes )
{
  DNETC_UNUSED_PARAM(doingmodes);

  __assert_statics();

  #if defined(SIGPIPE) //needed by at least solaris and fbsd
  SETSIGNAL( SIGPIPE, SIG_IGN );
  #endif
  #if (CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_RISCOS)
  RegisterPollDrivenBreakCheck( __PollDrivenBreakCheck );
  #endif
  #if (CLIENT_OS == OS_DOS)
  break_on(); //break on any dos call, not just term i/o
  RegisterPollDrivenBreakCheck( __PollDrivenBreakCheck );
  #endif
  #if defined(SIGHUP)
  SETSIGNAL( SIGHUP, CliSignalHandler );   //restart
  #endif
  #if defined(__unix__) && defined(TRIGGER_PAUSE_SIGNAL)
  if (!doingmodes)                  // signal-based pause/unpause mechanism?
  {
    #if ((CLIENT_OS == OS_QNX) && defined(__QNXNTO__)) || \
         (CLIENT_OS == OS_BEOS)
       /* nothing */
    #else
    // stop the shell from seeing SIGTSTP and putting the client into
    // the background when we '-pause' it.
      #if (CLIENT_OS == OS_NEXTSTEP)
    if( getpgrp( 0 ) != getpid() )
      setpgrp( 0, 0 );
      #else
    // porters : those calls are POSIX.1,
    // - on BSD you might need to change setpgid(0,0) to setpgrp()
    // - on SYSV you might need to change getpgrp() to getpgid(0)
    if( getpgrp() != getpid() )
      setpgid( 0, 0 );
      #endif /* CLIENT_OS == OS_NEXTSTEP */
    #endif /* CLIENT_OS == OS_QNX ... */
    SETSIGNAL( TRIGGER_PAUSE_SIGNAL, CliSignalHandler );  //pause
    SETSIGNAL( TRIGGER_UNPAUSE_SIGNAL, CliSignalHandler );  //continue
  }
  #else /* defined(__unix__) && defined(TRIGGER_PAUSE_SIGNAL) */
    #if (CLIENT_OS == OS_OS2)
      SETSIGNAL( TRIGGER_PAUSE_SIGNAL, CliSignalHandler );  //pause
      SETSIGNAL( TRIGGER_UNPAUSE_SIGNAL, CliSignalHandler );  //continue
    #endif
  #endif /* defined(__unix__) && defined(TRIGGER_PAUSE_SIGNAL) */
  #if (CLIENT_OS == OS_OS2) && defined(__WATCOMC__)
    // Pause/unpause in plain OS/2 works thru "user signal" */
    DosSetSigHandler(MoreOS2Signals, NULL, NULL, SIGA_ACCEPT, SIG_PFLG_A);
  #endif
  #if defined(SIGQUIT)
  SETSIGNAL( SIGQUIT, CliSignalHandler );  //shutdown
  #endif
  #if defined(SIGSTOP)
  SETSIGNAL( SIGSTOP, CliSignalHandler );  //shutdown, maskable some places
  #endif
  #if defined(SIGABRT)
  SETSIGNAL( SIGABRT, CliSignalHandler );  //shutdown
  #endif
  #if defined(SIGBREAK)
  SETSIGNAL( SIGBREAK, CliSignalHandler ); //shutdown
  #endif
  SETSIGNAL( SIGTERM, CliSignalHandler );  //shutdown
  SETSIGNAL( SIGINT, CliSignalHandler );   //shutdown
}
#endif

// =======================================================================

static const char *_init_trigfile(const char *fn, char *buffer, size_t bufsize )
{
  if (buffer && bufsize)
  {
    if (fn)
    {
      while (*fn && isspace(*fn))
        fn++;
      if (*fn)
      {
        size_t len = strlen(fn);
        while (len > 0 && isspace(fn[len-1]))
          len--;
        if (len && len < (bufsize-1))
        {
          strncpy( buffer, fn, len );
          buffer[len] = '\0';
          if (strcmp( buffer, "none" ) != 0)
          {
            fn = GetFullPathForFilename( buffer );
            if (fn)
            {
              if (strlen(fn) < (bufsize-1))
              {
                strcpy( buffer, fn );
                return buffer;
              }
            }
          }
        }
      }
    }
    buffer[0] = '\0';
  }
  return (const char *)0;
}

/* ---------------------------------------------------------------- */

static void _init_cputemp( const char *p ) /* cpu temperature string */
{
  s32 K[2];
  int which;

  trigstatics.cputemp.hithresh = trigstatics.cputemp.lothresh = 0;
  trigstatics.cputemp.marking_high = 0;

  K[0] = K[1] = -1;
  for (which=0;which<2;which++)
  {
    s32 val = 0;
    int len = 0, frac = 0, neg = 0;
    while (*p && isspace(*p))
      p++;
    if (!*p)
      break;
    if (*p == '-' || *p == '+')
      neg = (*p++ == '-');
    while (isdigit(*p))
    {
      val = val*10;
      val += (*p - '0'); /* this is safe in ebcidic as well */
      len++;
      p++;
    }
    if (len == 0) /* bad number */
      return;
    
    if (*p == '.') /* hmm, decimal pt. Allow for two digits after the decimal point */
    {
      p++;
      if (!isdigit(*p))
        return;

      while (frac < 2) 
      {
        val = val*10;
        if (isdigit(*p)) 
        {
          val += (*p - '0');
          p++;
        }
        frac++;
      }
      if (isdigit(*p) && *p >= '5') 
      {
        val++;
        p++;
      }
      while (isdigit(*p))
        p++;
    }
    else 
    {
      val = val*100;
    }
    
    if (neg)
      val = -val;
    while (*p && isspace(*p))
      p++;

    if (*p=='F' || *p=='f' || *p=='R' || *p=='r') /* farenheit or rankine */
    {
      if (*p == 'R' || *p == 'r') /* convert rankine to farenheit first */
        val -= 45967; /* 459.67 */
      val = (((val - 3200) * 5)/9) + 27315 /*273.15*/;  /* F -> K */
      p++;
    }
    else if (*p == 'C' || *p == 'c') /* celcius/centigrade */
    {
      val += 27315 /*273.15*/; /* C -> K */
      p++;
    }
    else if (*p == 'K' || *p == 'k') /* Kelvin */
    {
      p++;
    }
    if (val < 0) /* below absolute zero, uh, huh */
      return;
    K[which] = val;
    while (*p && isspace(*p))
      p++;
    if (*p != ':')
      break;
    p++;
  }
  if (K[0] > 1) /* be gracious :) */
  {
    if (K[1] < 0) /* only single temp provided */
    {
      K[1] = K[0];       /* then make that the high water mark */
      K[0] -= (K[0]/10); /* low water mark is 90% of high water mark */
    }
    TRACE_OUT((0,"cputemp: %d.%02dK:%d.%02dK\n", K[0]/100, K[0]%100, K[1]/100, K[1]%100));

    trigstatics.cputemp.lothresh = K[0];
    trigstatics.cputemp.hithresh = K[1];
  }
  return;
}

/* ---------------------------------------------------------------- */

static void _init_pauseplist( const char *plist )
{
  const char *p = plist;
  unsigned int wpos = 0, idx = 0, len;
  while (*p)
  {
    while (*p && (isspace(*p) || *p == '|'))
      p++;
    if (*p)
    {
      len = 0;
      plist = p;
      while (*p && *p != '|')
      {
        p++;
        len++;
      }
      while (len > 0 && isspace(plist[len-1]))
        len--;
      if (len)
      {
        const char *appname = &(trigstatics.pauseplistbuffer[wpos]);
        if ((wpos + len) >= (sizeof(trigstatics.pauseplistbuffer) - 2))
        {
          break;
        }
        memcpy( &(trigstatics.pauseplistbuffer[wpos]), plist, len );
        wpos += len;
        trigstatics.pauseplistbuffer[wpos++] = '\0';
        trigstatics.pauseplist[idx++] = __mangle_pauseapp_name(appname,0);
        if (idx == ((sizeof(trigstatics.pauseplist)/
                       sizeof(trigstatics.pauseplist[0]))-1) )
        {
          break;
        }
      }
    }
  }
  #if (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16)
  /*
  sorry about this piece of OS-specific uglyness. These were
  pause triggers before the days of utilGetPidlist(), and are now
  tacked on at the end of the guard list for compatibility's sake.
  Yes, they really are needed - I've seen people curse these two
  till they were blue in the face, not realizing what the heck it
  was that so crippled their machines. Yeah, yeah. win9x stinks. :)
  The mangling done above will already have given us unique identifiers,
  so we at least don't have to parse the whole lot again.
  */
  if (winGetVersion() >= 400 && winGetVersion() < 2000) /* win9x only */
  {
    for (len = 0; len < 2; len++)
    {
      const char *clist;
      if (idx == ((sizeof(trigstatics.pauseplist)/
                   sizeof(trigstatics.pauseplist[0]))-1) )
        break;
      clist = __mangle_pauseapp_name( ((len==0)?("defrag"):("scandisk")),0);
      for (wpos = 0; wpos < idx; wpos++)
      {
        if ( strcmp( trigstatics.pauseplist[wpos], clist ) == 0 )
        {
          clist = (const char *)0;
          break;
        }
      }
      if (clist)
        trigstatics.pauseplist[idx++] = clist;
    }
  }
  #endif
  trigstatics.pauseplist[idx] = (const char *)0;
}

/* ---------------------------------------------------------------- */

int InitializeTriggers(int doingmodes,
                       const char *exitfile, const char *pausefile,
                       const char *pauseplist,
                       int restartoninichange, const char *inifile,
                       int watchcputempthresh, const char *cputempthresh,
                       int pauseifnomainspower )
{
  TRACE_OUT( (+1, "InitializeTriggers\n") );
  __assert_statics();
  memset( (void *)(&trigstatics), 0, sizeof(trigstatics) );
  trigstatics.exittrig.pollinterval.whenon = 0;
  trigstatics.exittrig.pollinterval.whenoff = EXITFILE_CHECKTIME;
  trigstatics.pausetrig.pollinterval.whenon = PAUSEFILE_CHECKTIME_WHENON;
  trigstatics.pausetrig.pollinterval.whenoff = PAUSEFILE_CHECKTIME_WHENOFF;
  __init_signal_handlers( doingmodes );

  trigstatics.doingmodes = doingmodes;
  if (!doingmodes)
  {
    trigstatics.exittrig.flagfile = _init_trigfile(exitfile,
                 trigstatics.exitfilebuf, sizeof(trigstatics.exitfilebuf) );
    TRACE_OUT((0, "exitfile: %s\n", trigstatics.exittrig.flagfile));
    trigstatics.pausetrig.flagfile = _init_trigfile(pausefile,
                 trigstatics.pausefilebuf, sizeof(trigstatics.pausefilebuf) );
    TRACE_OUT((0, "pausefile: %s\n", trigstatics.pausetrig.flagfile));
    if (restartoninichange && inifile)
      _init_trigfile(inifile,trigstatics.inifile,sizeof(trigstatics.inifile));
    TRACE_OUT((0, "restartfile: %s\n", trigstatics.inifile));
    _init_pauseplist( pauseplist );
    if (watchcputempthresh && cputempthresh)
      _init_cputemp( cputempthresh ); /* cpu temp string */
    trigstatics.pause_if_no_mains_power = pauseifnomainspower;
  }
  TRACE_OUT( (-1, "InitializeTriggers\n") );
  return 0;
}

/* ---------------------------------------------------------------- */

int DeinitializeTriggers(void)
{
  int huptrig;

  __assert_statics();
  huptrig = trigstatics.huptrig.trigger;

  /* clear everything to ensure we don't use IO after DeInit */
  memset( (void *)(&trigstatics), 0, sizeof(trigstatics) );

  return huptrig;
}

/* ---------------------------------------------------------------- */

