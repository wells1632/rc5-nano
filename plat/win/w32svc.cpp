/*
 * Copyright distributed.net 1997-2011 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Created by Cyrus Patel <cyp@fb14.uni-mainz.de>
*/

const char *w32svc_cpp(void) {
return "@(#)$Id: w32svc.cpp,v 1.12 2012/08/21 18:55:55 sla Exp $"; }

//#define TRACE

#include "cputypes.h"
#include "unused.h"     /* DNETC_UNUSED_* */

#if (CLIENT_OS != OS_WIN32) && (CLIENT_OS != OS_WIN64) /* win16 no longer supported */
#error Unsupported OS type.
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "w32svc.h"   /* ourselves */

#ifndef MAX_PATH      /* win16 */
#define MAX_PATH 256
#endif

#define SLEEP_ON_SERVICE_START 10 /* secs */

#define CLIENT_SERVICE_CONTROL_FIRST    0x80 //service control #
#ifndef CLIENT_SERVICE_CONTROL_RESTART  /* defined in w32svc.h */
#define CLIENT_SERVICE_CONTROL_RESTART  (CLIENT_SERVICE_CONTROL_FIRST+0)
#endif
#define CLIENT_SERVICE_CONTROL_FETCH    (CLIENT_SERVICE_CONTROL_FIRST+1)
#define CLIENT_SERVICE_CONTROL_FLUSH    (CLIENT_SERVICE_CONTROL_FIRST+2)
#define CLIENT_SERVICE_CONTROL_UPDATE   (CLIENT_SERVICE_CONTROL_FIRST+3)
#define CLIENT_SERVICE_CONTROL_CAPSSTAT (CLIENT_SERVICE_CONTROL_FIRST+4)
#define CLIENT_SERVICE_CONTROL_LAST     CLIENT_SERVICE_CONTROL_CAPSSTAT


#ifndef SERVICE_CONTROL_PARAMCHANGE //win2k only
#define SERVICE_CONTROL_PARAMCHANGE 0x00000006
#endif
#ifndef SERVICE_ACCEPT_PARAMCHANGE  //win2k only
#define SERVICE_ACCEPT_PARAMCHANGE 0x00000008
#endif

#if defined(PROXYTYPE)
  #include "settings.h" /* PROXY_FULL|PERSONAL|MASTER constants */
  #include "globals.h"  /* bReloadSettings/bForceConnect/bExitNow */
  void RaiseExitRequestTrigger()      { bExitNow = 1; }
  int  CheckExitRequestTriggerIO()    { return bExitNow != 0; }
  void RaiseRestartRequestTrigger()   { bReloadSettings = 1; }
  int  CheckRestartRequestTriggerIO() { return bReloadSettings != 0; }
  void RaisePauseRequestTrigger()     { /* nothing */ }
  int  CheckPauseRequestTriggerIO()   { return 0; }
  void TriggerUpdate()                { bForceConnect = 1; }
  #if (PROXYTYPE == PROXY_FULL)
  #define SERVICEFOR "keyserver"
  #elif (PROXYTYPE == PROXY_MASTER)
  #define SERVICEFOR "keymaster"
  #elif (PROXYTYPE == PROXY_PERSONAL)
  #define SERVICEFOR "personal proxy"
  #else
  #error Unrecognized PROXYTYPE.
  #endif
  const char *NTSERVICEIDS[]={"dnetd"/*oct99*/,"bovproxy"/*old*/,"bovproxy"/*old*/};
  const char *W9xSERVICEKEY = ""; // don't support 9x for proxy
#else
  #include "triggers.h" //RaiseExitRequestTrigger()
  #include "modereq.h"  //ModeReq(MODEREQ_FETCH|MODEREQ_FLUSH)
  extern int CheckRestartRequestTriggerNoIO(void);
  #if (CLIENT_CPU == CPU_CUDA)
  #define SERVICEFOR  "CUDA-client"
  const char *NTSERVICEIDS[]={"dnetc-CUDA"/*feb09*/};
  const char *W9xSERVICEKEY = "";
  #elif (CLIENT_CPU == CPU_ATI_STREAM)
  #define SERVICEFOR  "ATI-client"
  const char *NTSERVICEIDS[]={"dnetc-ATI"/*feb09*/};
  const char *W9xSERVICEKEY = "";
  #elif (CLIENT_CPU == CPU_OPENCL)
  #define SERVICEFOR  "OpenCL-client"
  const char *NTSERVICEIDS[]={"dnetc-OpenCL"/*Aug12*/};
  const char *W9xSERVICEKEY = "";
  #else
  #define SERVICEFOR  "client"
  const char *NTSERVICEIDS[]={"dnetc"/*oct99*/,"rc5desnt"/*Oct98*/,"bovrc5nt"/*ancient*/};
  const char *W9xSERVICEKEY = "distributed.net client";
  #endif
#endif /* PROXYTYPE or not */
const char *APPDESCRIP = "distributed.net " SERVICEFOR;

#define SERVICEMUTEX "distributed.net " SERVICEFOR " service mutex"

/* ---------------------------------------------------------- */

static int win32cli_servicified = 0;
int win32CliServiceRunning(void)
{
  return win32cli_servicified;
}

/* ---------------------------------------------------------- */

static long __winGetVersion(void)
{
  static long ver = 0;

  if (ver == 0)
  {
    unsigned int versionmajor = 0, versionminor = 0;
    #if defined(_WINNT_)
    {
      OSVERSIONINFO osver;

      osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
      GetVersionEx(&osver);
      versionmajor = (int)osver.dwMajorVersion;
      versionminor = (int)osver.dwMinorVersion;
      if (VER_PLATFORM_WIN32_NT == osver.dwPlatformId)
        versionmajor += 20;
      else if (VER_PLATFORM_WIN32s == osver.dwPlatformId)
      {   /* version number is unusable (1.xx) */
        DWORD dwVersion = GetVersion();
        versionmajor = LOBYTE(LOWORD(dwVersion));
        versionminor = HIBYTE(LOWORD(dwVersion));
      }
    }
    #else
    {
      DWORD dwVersion = GetVersion();
      versionmajor = LOBYTE(LOWORD(dwVersion));
      versionminor = HIBYTE(LOWORD(dwVersion));
      //build number in (HIWORD(dwVersion) & 0x7FFF) in all but plain win3x

      /* heck, even the DOS client has better version detection than this */

      if ((dwVersion & 0x80000000)==0) //winNT or win3x without win32s
      {
        const char *p = (const char *)getenv("OS");
        // NT 3.x returns 3.x, NT 4.x returns 4.x, but win2k returns 3.95,
        // which not only sounds wierd, but is wrong according to
        // http://msdn.microsoft.com/library/psdk/sysmgmt/sysinfo_41bi.htm
        if (versionmajor == 3 && versionminor == 95) /* win2k */
        {
          versionmajor = 5;
          versionminor = 0;
        }
        if (p && strcmp(p,"Windows_NT")==0)
          versionmajor += 20;
        //if ( GetModuleHandle("KERNEL32") != NULL )
        //  versionmajor += 20;
      }
      else if (versionmajor == 3 && versionminor >= 95) /* win9x */
      {
        versionmajor = 4;
        versionminor = 1;
      }
    }
    #endif

    ver = (((long)(versionmajor))*100)+versionminor;
  }
  return ver;
}


/* ---------------------------------------------------------- */

static const char *__constructerrmsg(const char *leadin, DWORD errnum,
                              char *msgbuffer, unsigned int buflen)
{
  LPVOID lpMsgBuf;
  int len = sprintf(msgbuffer, "%s: error %d\n", leadin, (int)errnum );
  FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                 FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                 NULL, errnum, 0 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/,
                 (LPTSTR) &lpMsgBuf, 0, NULL );
  if (lpMsgBuf)
  {
    msgbuffer[len++] = '(';
    strncpy(&msgbuffer[len],((const char *)lpMsgBuf),(buflen-len)-1);
    msgbuffer[buflen-3]='\0';
    strcat(msgbuffer,")");
    LocalFree( lpMsgBuf );
  }
  return (const char *)&msgbuffer[0];
}


/* ---------------------------------------------------------- */

// Not static because also called by w32pre.cpp
int __GetMyModuleFilename(char *buffer, unsigned int len)
{
  if (buffer && len)
  {
    #if defined(PROXYTYPE)
    return GetModuleFileName( NULL, buffer, (WORD)len );
    #else
    //we do it this way for two reasons:
    // win16) we don't have the instance handle here
    //        [we /could/ get the hInst from winGetInstanceHandle() though]
    // win32/win64) the client may have been stubbed with conshim, so
    //        GetModuleHandle() would return the name of the temp file.
    //        [we /could/ get the name from the environment though]
    extern int winGetMyModuleFilename(char *, unsigned int); /* w32pre.cpp */
    return winGetMyModuleFilename(buffer, len);
    #endif
  }
  return 0;
}

/* ---------------------------------------------------------- */

static void __print_message( const char *message, int iserror )
{
  #if defined(PROXYTYPE)
  fprintf((iserror ? stderr : stdout),"%s\n",message);
  #else
  extern int w32ConOutErr(const char *text);
  extern int w32ConOutModal(const char *text);
  if (iserror)
    w32ConOutErr(message);
  else
    w32ConOutModal(message);
  #endif
}

/* ---------------------------------------------------------- */

#if defined(DEBUG) && !defined(TRACE)
  #define TRACE
#endif
#ifndef TRACE
  #define TRACE_OUT(x) /* nothing */
  #define TRACE_LASTERROR(x) /* nothing */
#else
  #if 0 //ifndef PROXYTYPE /* client */
  #include "util.h" /* void trace_out(int indlevel,const char *fmt, ...); */
  #else
  #define TRACE_OUT(x) trace_out x
  #include <time.h>
  static void trace_out(int indlevel, const char *format,...)
  {
    static int indentlevel = 0;
    static char debugfile[MAX_PATH+1];
    static int init = -1;
    FILE *file;
    va_list arglist;
    va_start (arglist, format);

    if (init == -1)
    {
      char *p1, *p2;
      __GetMyModuleFilename(debugfile, sizeof(debugfile));
      p1 = strrchr(debugfile,'/');
      p2 = strrchr(debugfile,'\\');
      if (p2 > p1) p1 = p2;
      p2 = strrchr(debugfile,':');
      if (p2 > p1) p1 = p2;
      strcpy( ((p1) ? (p1+1) : (&debugfile[0])), "svctrace.out" );
      remove( debugfile );
      init = 1;
    }
    if (indlevel < 0)
      indentlevel--;
    file = fopen( debugfile, "a" );
    if (file)
    {
      char buffer[64];
      DWORD ticks = GetTickCount();
      fprintf(file,"%u.%03u: ", ticks/1000, ticks%1000 );
      if (indentlevel > 0)
      {
        size_t spcs = ((size_t)indentlevel) * 2;
        memset((void *)(&buffer[0]),' ',sizeof(buffer));
        while (sizeof(buffer) == fwrite( buffer, 1,
           ((spcs>sizeof(buffer))?(sizeof(buffer)):(spcs)), file ))
          spcs -= sizeof(buffer);
      }
      if (indlevel != 0)
        fwrite((void *)((indlevel < 0)?("end: "):("beg: ")), 1, 5, file );
      vfprintf(file, format, arglist);
      fflush( file );
      fclose( file );
    }
    if (indlevel > 0)
      indentlevel++;
    return;
  }
  #endif

  #define TRACE_LASTERROR(x) trace_lasterror x
  static char __debug_name_buffer[64];
  static const char *debug_getsvcStatusName(DWORD svcstatus)
  {
    if (svcstatus == SERVICE_STOPPED)
      return "SERVICE_STOPPED";
    if (svcstatus == SERVICE_START_PENDING)
      return "SERVICE_START_PENDING";
    if (svcstatus == SERVICE_STOP_PENDING)
      return "SERVICE_STOP_PENDING";
    if (svcstatus == SERVICE_RUNNING)
      return "SERVICE_RUNNING";
    if (svcstatus == SERVICE_CONTINUE_PENDING)
      return "SERVICE_CONTINUE_PENDING";
    if (svcstatus == SERVICE_PAUSE_PENDING)
      return "SERVICE_PAUSE_PENDING";
    if (svcstatus == SERVICE_PAUSED)
      return "SERVICE_PAUSE_PAUSED";
    sprintf(__debug_name_buffer,"*UNKNOWN* (BAD!) (%ld)", svcstatus );
    return __debug_name_buffer;
  }
  static const char *debug_getsvcControlCodeName(DWORD ctrlCode)
  {
    if (ctrlCode == SERVICE_CONTROL_STOP)
      return "SERVICE_CONTROL_STOP";
    if (ctrlCode == SERVICE_CONTROL_PAUSE)
      return "SERVICE_CONTROL_PAUSE";
    if (ctrlCode == SERVICE_CONTROL_CONTINUE)
      return "SERVICE_CONTROL_CONTINUE";
    if (ctrlCode == SERVICE_CONTROL_INTERROGATE)
      return "SERVICE_CONTROL_INTERROGATE";
    if (ctrlCode == SERVICE_CONTROL_SHUTDOWN)
      return "SERVICE_CONTROL_SHUTDOWN";
    if (ctrlCode == SERVICE_CONTROL_PARAMCHANGE)
      return "SERVICE_CONTROL_PARAMCHANGE";
    if (ctrlCode == CLIENT_SERVICE_CONTROL_RESTART)
      return "(custom) CLIENT_SERVICE_CONTROL_RESTART";
    if (ctrlCode == CLIENT_SERVICE_CONTROL_FETCH)
      return "(custom) CLIENT_SERVICE_CONTROL_FETCH";
    if (ctrlCode == CLIENT_SERVICE_CONTROL_FLUSH)
      return "(custom) CLIENT_SERVICE_CONTROL_FLUSH";
    if (ctrlCode == CLIENT_SERVICE_CONTROL_UPDATE)
      return "(custom) CLIENT_SERVICE_CONTROL_UPDATE";
    if (ctrlCode == CLIENT_SERVICE_CONTROL_CAPSSTAT)
      return "(custom) CLIENT_SERVICE_CONTROL_CAPSSTAT";
    sprintf(__debug_name_buffer,"huh? unknown control code %ld\n", ctrlCode);
    return __debug_name_buffer;
  }
  static void trace_lasterror(int tracelev, const char *caption)
  {
    LPVOID lpMsgBuf;
    DWORD errnum = GetLastError();
    FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
       FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
       errnum, 0 /*MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT)*/,
       (LPTSTR) &lpMsgBuf, 0, NULL );// Process any inserts in lpMsgBuf.
    TRACE_OUT((tracelev, "%s: GetLastError()=>%ld (\"%s\")\n", caption, errnum, lpMsgBuf ));
    LocalFree( lpMsgBuf );
    return;
  }

#endif

/* ---------------------------------------------------------- */

#define SVC_CSD_MODE_ISINSTALLED 0
#define SVC_CSD_MODE_START 1
#define SVC_CSD_MODE_ISRUNNING 2

static int __DoModeService( int mode, int argc, char **argv )
{
  int isinstalled = -1; /* assume error */
  int retcode = -1; /* assume error */

  DNETC_UNUSED_PARAM(argc);
  DNETC_UNUSED_PARAM(argv);

  if (__winGetVersion() < 400) /* win16 */
  {
    isinstalled = -1;       // not supported anymore.
  }
  #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  else if (__winGetVersion() >= 2000) /* NT */
  {
    SC_HANDLE myService, scm;
    TRACE_OUT((+0,"opening SCManager\n" ));
    scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    TRACE_OUT((+0,"opened SCManager ok? %d\n", (scm!=0) ));
    if (scm)
    {
      unsigned int i;
      retcode = isinstalled = 0;
      for (i=0;i<(sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0]));i++)
      {
        TRACE_OUT((+0,"opening service \"%s\"\n", NTSERVICEIDS[i] ));
        myService = OpenService(scm, NTSERVICEIDS[i], SERVICE_ALL_ACCESS);
        TRACE_OUT((+0,"opened service \"%s\" ok? %d\n", NTSERVICEIDS[i], (myService!=0) ));
        if (myService)                        /* yes, SERVICE_ALL_ACCESS */
        {
          isinstalled |= (1<<i);
          if (mode == SVC_CSD_MODE_START)
          {
            --argc; ++argv; /* StartService() does not include argv[0] */
            if (StartService(myService,argc,(const char **)argv))
              retcode = +1;
          }
          else if (mode == SVC_CSD_MODE_ISRUNNING)
          {
            SERVICE_STATUS status;
            if (!QueryServiceStatus(myService, &status))
              retcode = -1;
            else if (status.dwCurrentState != SERVICE_STOPPED)
              retcode = +1;
          }
          CloseServiceHandle(myService);
          break;
        }
      }
      CloseServiceHandle(scm);
    }
  }
  #endif
  #if (CLIENT_OS == OS_WIN32)
  else if (__winGetVersion() >= 400 && W9xSERVICEKEY[0] != 0) /* Win9x */
  {
    HKEY srvkey = NULL;
    TRACE_OUT((+1,"opening HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\RunServices\n" ));
    if (RegOpenKey(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
        &srvkey) != ERROR_SUCCESS)
    {
      TRACE_OUT((-1,"failed to open reg\n" ));
    }
    else
    {
      DWORD valuetype = REG_SZ;
      char buffer[260]; /* maximum registry key length */
      DWORD valuesize = sizeof(buffer);
      const char *svcnames[] = { W9xSERVICEKEY, "bovwin32" };
      unsigned int i;

      retcode = isinstalled = 0;
      for (i=0;i<(sizeof(svcnames)/sizeof(svcnames[0]));i++)
      {
        TRACE_OUT((+0,"checking for %s\n", svcnames[i] ));
        if ( RegQueryValueEx(srvkey, svcnames[i], NULL, &valuetype,
               (unsigned char *)(&buffer[0]), &valuesize) != ERROR_SUCCESS )
        {
          TRACE_OUT((+0,"no such key \"%s\"\n", svcnames[i] ));
        }
        else
        {
          TRACE_OUT((+0,"got %s=\"%s\"\n", svcnames[i], buffer ));
          isinstalled |= (1<<i);
          if (mode == SVC_CSD_MODE_START)
          {
            SetEnvironmentVariable("dnet.svcify","1");
            if (WinExec(buffer,SW_HIDE) > 31)
              retcode = +1;
            SetEnvironmentVariable("dnet.svcify",NULL);
          }
          else if (mode == SVC_CSD_MODE_ISRUNNING)
          {
            HANDLE hmutex;
            SECURITY_ATTRIBUTES sa;
            memset(&sa,0,sizeof(sa));
            sa.nLength = sizeof(sa);
            SetLastError(0);
            hmutex = CreateMutex(&sa, FALSE, SERVICEMUTEX);
            if (hmutex)
            {
              if (GetLastError() != 0)  /* ie, it exists */
                retcode = +1;
              ReleaseMutex( hmutex );
              CloseHandle( hmutex );
            }
          }
          break;
        }
      }
      RegCloseKey(srvkey);
      TRACE_OUT((-1,"regclosed. installed=%d, retcode=%d\n", isinstalled, retcode ));
    }
  }
  #endif

  if (mode != SVC_CSD_MODE_ISINSTALLED)
  {
    if (isinstalled > 0)
      return retcode;
  }
  return (isinstalled);
}

int win32CliIsServiceInstalled(void) /* <0=err, 0=no, >0=yes */
{
  int retcode;
  TRACE_OUT((+1,"win32CliIsServiceInstalled()\n" ));
  retcode = __DoModeService(SVC_CSD_MODE_ISINSTALLED,0,NULL);
  TRACE_OUT((-1,"win32CliIsServiceInstalled() => %d\n", retcode ));
  return retcode;
}

int win32CliStartService(int argc, char **argv) /* <0=err, 0=notinst, >0=ok */
{
  int retcode;
  TRACE_OUT((+1,"win32CliStartService()\n" ));
  retcode = __DoModeService(SVC_CSD_MODE_START,argc,argv);
  TRACE_OUT((-1,"win32CliStartService() => %d\n", retcode ));
  return retcode;
}

int win32CliDetectRunningService(void) /* <0=err, 0=no, >0=yes */
{
  if (win32cli_servicified)
    return 1;
  return __DoModeService(SVC_CSD_MODE_ISRUNNING,0,NULL);
}

/* ---------------------------------------------------------- */

//! Remove the service installation of the client.
/*!
 * This removal does not actually delete any files; it only 
 * de-registers the startup of the client as a service.
 *
 * \param quiet used internally by the service itself
 * \return On error, a negative value.  Otherwise zero on 
 *    successfully uninstalled, or possibly no action because
 *    the client was not installed as a service.
 */
int win32CliUninstallService(int quiet)
{
  int retcode = -1;
  const char *msg = "A distributed.net " SERVICEFOR " could not be uninstalled";

  if (__winGetVersion() < 400) /* win16 */
  {
    retcode = -1;     // not supported anymore.
    msg = NULL;
  }
  #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  else if (__winGetVersion() >= 2000) /* NT */
  {
    SC_HANDLE myService, scm;
    SERVICE_STATUS status;
    int foundcount = 0, deletedcount = 0;

    msg = "The Service Control Manager could not be opened.\n"
          "The distributed.net service " SERVICEFOR " could not be uninstalled.";

    scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (scm)
    {
      retcode = 0;
      unsigned int i;
      for (i = 0; i < (sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0])); i++)
      {
        myService = OpenService(scm, NTSERVICEIDS[i], SERVICE_ALL_ACCESS);
        if (myService)
        {
          foundcount++;
          if (QueryServiceStatus(myService, &status) &&
             status.dwCurrentState != SERVICE_STOPPED)
          {
            if (!ControlService(myService, SERVICE_CONTROL_STOP, &status))
            {
              retcode = -1;
              msg = "The distributed.net " SERVICEFOR " is running and "
                    "could not be stopped.\n"
                    "The " SERVICEFOR " has not been uninstalled.";
            }
          }
          CloseServiceHandle(myService);
        }
        if (retcode != 0)
          break;
      }
      if (retcode == 0)
      {
        if (foundcount == 0)
        {
          msg = "The distributed.net " SERVICEFOR " is not installed as an NT service.";
          retcode = 0; //not installed, so retcode zero is ok
        }
        else
        {
          for (i = 0; i < (sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0])); i++)
          {
            myService = OpenService(scm, NTSERVICEIDS[i], SERVICE_ALL_ACCESS|DELETE);
            if (myService)
            {
              if (DeleteService(myService))
              {
                deletedcount++;
              }
              CloseServiceHandle(myService);
            }
          }
          if (deletedcount == foundcount)
          {
            msg = "The distributed.net " SERVICEFOR " has been uninstalled.";
            retcode = 0;
          }
          else
          {
            retcode = -1;
            if (deletedcount != 0)
              msg = "One or more distributed.net services has been uninstalled.\n"
                    "However, one or mode deinstallations failed.";
            else
              msg = "The distributed.net " SERVICEFOR " could not be uninstalled.";
          }
        }
      }
      CloseServiceHandle(scm);
    }
  }
  #endif
  #if (CLIENT_OS == OS_WIN32)
  else if (__winGetVersion() >= 400 && W9xSERVICEKEY[0] !=0 ) /* Win9x */
  {
    HKEY srvkey;

    msg = "Unable to open the service registry.\n"
          "The distributed.net " SERVICEFOR " service entry could not deleted.";

    /* unregister a Win95 "RunService" item */
    if (RegOpenKey(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
        &srvkey) == ERROR_SUCCESS)
    {
      DWORD valuetype = REG_SZ;
      char buffer[260]; /* maximum registry key length */
      DWORD valuesize = sizeof(buffer);
      const char *svcnames[] = { W9xSERVICEKEY, "bovwin32" };
      int i, wasinstalled = 0, wasdeleted = 0;

      for (i=0;i<(sizeof(svcnames)/sizeof(svcnames[0]));i++)
      {
        if ( RegQueryValueEx(srvkey, svcnames[i], NULL, &valuetype,
               (unsigned char *)(&buffer[0]), &valuesize) == ERROR_SUCCESS )
          wasinstalled = 1;
        if ( RegDeleteValue(srvkey, svcnames[i] ) == ERROR_SUCCESS )
          wasdeleted = 1;
      }
      if (wasdeleted)
      {
        msg = "The distributed.net " SERVICEFOR "'s service entry has been deleted.\n" \
              "The " SERVICEFOR " is no longer installed as a service.";
        retcode = 0;
      }
      else if (!wasinstalled)
      {
        msg = "The distributed.net " SERVICEFOR " is not installed as a service.";
        retcode = 0;
      }
      else
        msg = "The distributed.net " SERVICEFOR "'s service entry could not\n" \
              "be deleted. The " SERVICEFOR " has not be uninstalled.";

      RegCloseKey(srvkey);
    }

    /* unregister a Win95 "Run" item */
    if (RegOpenKey(HKEY_LOCAL_MACHINE,
      "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
      &srvkey) == ERROR_SUCCESS)
    {
      RegDeleteValue(srvkey, "bovwin32");
      RegCloseKey(srvkey);
    }
  }
  #endif

  if (msg && !quiet)
    __print_message( msg, (retcode < 0) );

  return retcode;
}

/* ---------------------------------------------------------- */

static int IsShellRunning(void)
{
  /* ******************** HACK * HACK * HACK * HACK * HACK *********** */
  /* this hack is to speed up the load process on NT as well as to check */
  /* whether a 9x client that is installed as a service should actually */
  /* start as a service. */
  /* It assumes that the presence of progman/exploder means we cannot have */
  /* been started as a service. (since progman/exploder starts only after */
  /* service init). */
  /* This hack will not work if M$ decides to rename the exploder's */
  /* window class - but it won't break anything. - cyp */
  if (FindWindow( "TApplication", "LiteStep" ) != NULL)
    return 1;
  return (FindWindow( "Progman", NULL ) != NULL);
  /* ************************** END HACK ***************************** */
}

/* ---------------------------------------------------------- */

/*
** Guess if the application should attempt to try to initialize itself as
** a service.
*/
#if defined(_WINNT_)
static int IsTryNTStartServiceWorthwhile(void) /* returns +1=yes, 0=no, -1=error */
{
  if (!IsShellRunning()) /* no shell, then must be service */
    return +1;

  if (__winGetVersion() >= 2000) /* is windows NT */
  {
    int tryserv = -1; /* assume error */
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    {
      PTOKEN_GROUPS pGroupInfo = (PTOKEN_GROUPS)0;
      DWORD dwSize = 0;

      if (!GetTokenInformation(hToken, TokenGroups, NULL, dwSize, &dwSize))
      {
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
        {
          pGroupInfo = (PTOKEN_GROUPS)LocalAlloc(0, dwSize);
          if (pGroupInfo)
          {
            if (!GetTokenInformation(hToken, TokenGroups,
                                     pGroupInfo, dwSize, &dwSize))
            {
              LocalFree((HLOCAL)pGroupInfo);
              pGroupInfo = (PTOKEN_GROUPS)0;
            }
          }
        }
      }

      if (pGroupInfo) /* got valid token group info */
      {
        int isInteractive = -1, isService = -1;
        SID_IDENTIFIER_AUTHORITY siaNt1 = {SECURITY_NT_AUTHORITY};
        SID_IDENTIFIER_AUTHORITY siaNt2 = {SECURITY_NT_AUTHORITY};
        PSID sid; DWORD nGrp;

        /*
        ** net start for svc as regular user but with wrong passwd.
        **   SECURITY_SERVICE_RID=missing, SECURITY_INTERACTIVE_RID=avail
        **   (StartService() fails of course)
        **
        ** net start for svc as regular user and with right passwd.
        **   SECURITY_SERVICE_RID=avail, SECURITY_INTERACTIVE_RID=missing
        **
        ** net start for Local System (with or without "Interact with Desktop")
        **   SECURITY_SERVICE_RID=missing, SECURITY_INTERACTIVE_RID=missing
        **
        ** SECURITY_LOCAL_SYSTEM_RID doesn't seem to be ever set, so the rule
        ** I'm going with is...
        ** isService = (SECURITY_SERVICE_RID || (!SECURITY_SERVICE_RID &&
        **                                       !SECURITY_INTERACTIVE_RID))
        */

        if (AllocateAndInitializeSid(&siaNt1, 1,
            SECURITY_INTERACTIVE_RID, 0, 0, 0, 0, 0, 0, 0, &sid))
        {
          isInteractive = 0;
          for (nGrp = 0; isInteractive == 0 &&
               nGrp < pGroupInfo->GroupCount; nGrp++)
          {
            if (EqualSid(sid, pGroupInfo->Groups[nGrp].Sid))
              isInteractive = +1;
          }
          FreeSid(sid);
        }
        if (AllocateAndInitializeSid(&siaNt2, 1,
            SECURITY_SERVICE_RID, 0, 0, 0, 0, 0, 0, 0, &sid))
        {
          isService = 0;
          for (nGrp = 0; isService == 0 &&
               nGrp < pGroupInfo->GroupCount; nGrp++)
          {
            if (EqualSid(sid, pGroupInfo->Groups[nGrp].Sid))
              isService = +1;
          }
          FreeSid(sid);
        }
        TRACE_OUT((0,"isService=%d, isInteractive=%d\n",
                      isService, isInteractive ));

        if (isService > 0 || (isService == 0 && isInteractive == 0))
          tryserv = +1;
        else if (isService >= 0 && isInteractive >= 0) /* no error */
          tryserv = 0;

        LocalFree(pGroupInfo);
      } /* if (pGroupInfo) */
      CloseHandle(hToken);
    } /* if (OpenProcessToken(...)) */
    return tryserv;
  } /* if (__winGetVersion() >= 2000) */

  return 0;
}
#endif /* defined(__WINNT__) */

/* ---------------------------------------------------------- */

int win32CliInstallService(int quiet)
{                    /* quiet is used internally by the service itself */
  int retcode = -1;
  char buffer[260]; /* max size of a reg entry */
  const char *msg = NULL;

  TRACE_OUT((+1,"win32CliInstallService().\n" ));

  if (__winGetVersion() < 400) /* win16 */
  {
    msg = NULL;
    retcode = -1;   // not supported anymore
  }
  #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  else if (__winGetVersion() >= 2000) /* NT */
  {
    SC_HANDLE myService, scm;
    DWORD errnum;

    TRACE_OUT((+1,"beginning NT section\n" ));

    msg = "Unable to open the Service Control Manager.\n"
          "The distributed.net " SERVICEFOR " could not be installed "
          "as a service.";

    TRACE_OUT((0,"Doing OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE)...\n" ));
    scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
    {
      errnum = GetLastError();
      TRACE_LASTERROR((0,"OpenSCManager failed!\n" ));
      msg = __constructerrmsg("Unable to open the Service Control Manager",
                        errnum, buffer, sizeof(buffer));
    }
    else
    {
      int reinstalled = 0;
      CloseServiceHandle(scm);
      TRACE_OUT((0,"OpenSCManager opened ok. Closed it again.\n" ));

      retcode = 0;
      if (win32CliIsServiceInstalled())
      {
        TRACE_OUT((0,"Service is installed. Trying uninstall first...\n" ));
        reinstalled = 1;
        if (win32CliUninstallService(1)<0) /* <0=err, 0=ok(or notinstalled) */
        {
          TRACE_OUT((0,"win32CliUninstallService() failed!\n" ));
          msg = "A distributed.net " SERVICEFOR " is already installed as a\n"
                "service and could not be removed prior to re-installation.";
          retcode = -1;
        }
      }
      if (retcode == 0)
      {
        retcode = -1;
#if 0
        if (!IsShellRunning()) /* start will fail */
        {
           sprintf( buffer, "This machine is running a wierd shell.\n"
           "The distributed.net " SERVICEFOR " would not have started correctly\n"
           "and has not been %sinstalled.", ((reinstalled)?("re-"):("")) );
           msg = &buffer[0];
        }
        else
#endif
        {
          int ras_detect_state = -1;

          #if 0 /* don't need this since we depend on TDI group */
          scm = OpenSCManager(NULL, NULL, GENERIC_READ);
          TRACE_OUT((+1,"begin: NT ras check. OpenSCManager()=>%08x\n",scm));
          if (scm)
          {
            ENUM_SERVICE_STATUS  essServices[16];
            DWORD dwResume = 0, cbNeeded = 1;

            while (ras_detect_state < 0 && cbNeeded > 0)
            {
              DWORD index, csReturned = 0;
              if (!EnumServicesStatus(scm, SERVICE_WIN32, SERVICE_ACTIVE,
                 essServices, sizeof(essServices), &cbNeeded, &csReturned,
                 &dwResume))
              {
                if (GetLastError() != ERROR_MORE_DATA)
                {
                  TRACE_OUT((0,"NT ras check. EnumServiceStatus() err=%08x\n",GetLastError()));
                  break;
                }
              }
              for (index = 0; index < csReturned; index++)
              {
                TRACE_OUT((0,"NT ras check. found EnumServiceStatus() =>%s\n",essServices[index].lpServiceName));
                if (0==lstrcmp(essServices[index].lpServiceName,TEXT("RasMan")))
                {
                  // service exists. RAS is installed.
                  ras_detect_state = +1;
                  break;
                }
              }
            }
            CloseServiceHandle (scm);
          }
          TRACE_OUT((-1,"end: NT ras check.\n"));
          #endif

          if ((scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE)) == 0)
          {
            errnum = GetLastError();
            TRACE_LASTERROR((0,"OpenSCManager(0,0,SC_MANAGER_CREATE_SERVICE) failed!\n" ));
            msg = __constructerrmsg("Unable to open the Service Control Manager",
                        errnum, buffer, sizeof(buffer));
          }
          else
          {
            /* TDI=DHCP client+DNS client (depend on PNP_TDI=tcpip etc) */
            const char *dependencies = "+TDI\0+NetworkProvider\0\0";
            if (ras_detect_state > 0)
              dependencies = "RasMan\0+TDI\0+NetworkProvider\0\0";

            __GetMyModuleFilename( &buffer[1], sizeof(buffer)-1);
            buffer[0] = '\"';
            strcat(buffer, "\""); /* "\" -svcrun" */

            TRACE_OUT((+1,"CreateService(,\"%s\",,,,\"%s\")\n", NTSERVICEIDS[0], buffer ));

            myService = CreateService(scm, NTSERVICEIDS[0],
                (char *)APPDESCRIP, SERVICE_ALL_ACCESS,
                SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
                buffer, 0, 0, dependencies, 0, 0);
            if (!myService)
            {
              errnum = GetLastError();
              TRACE_LASTERROR((-1,"CreateServicer() failed!\n" ));
              msg = __constructerrmsg("Unable to create service",
                        errnum, buffer, sizeof(buffer));
            }
            else
            {
              TRACE_OUT((-1,"CreateService() ok!\n" ));

              CloseServiceHandle(myService);
              sprintf( buffer,
                "The distributed.net " SERVICEFOR " has been successfully %sinstalled as\n"
                "an NT service and has been configured to start automatically.",
                (reinstalled)?("re-"):("") );
              msg = &buffer[0];
              retcode = 0;
            }
            CloseServiceHandle(scm);
          } //OpenSCManager ok
        } //shell is there
      } //deinstall (if installed) was ok
    }
    TRACE_OUT((-1,"end NT section\n" ));
  }
  #endif
  #if (CLIENT_OS == OS_WIN32)
  else if (__winGetVersion() >= 400 && W9xSERVICEKEY[0] != 0) /* Win9x */
  {
    // proxy service on Win9x not supported, nor desirable.
    TRACE_OUT((+1,"begin win9x section\n" ));
    TRACE_OUT((+0,"blindly doing win32CliUninstallService() to purge old stuff\n" ));
    win32CliUninstallService(1); //delete old gunk

#if 0
    if (!IsShellRunning()) /* start may fail */
    {
      TRACE_OUT((+0,"huh? no shell?\n" ));
      msg = "This machine is running a wierd shell.\n"
             "The distributed.net " SERVICEFOR " would not have started correctly\n"
             "and has not been installed.";
    }
    else
#endif
    {
      HKEY srvkey=NULL;
      DWORD dwDisp=NULL;

      if (RegCreateKeyEx(HKEY_LOCAL_MACHINE,
          "Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",0,"",
                REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,NULL,
                &srvkey,&dwDisp) != ERROR_SUCCESS)
      {
        msg = "Unable to open registry. The distributed.net " SERVICEFOR " could not be\n"
              "installed as a service.";
        TRACE_LASTERROR((0,"RegCreateKey(HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\RunServices) failed!\n" ));
      }
      else
      {
        TRACE_OUT((+0,"RegCreateKeyEx(HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\RunServices) succeeded.\n" ));
        __GetMyModuleFilename( &buffer[1], sizeof(buffer)-1);
        buffer[0] = '\"';
        strcat( buffer, "\" -hide" );

        TRACE_OUT((+1,"RegSetValueEx(,\"%s\",\"%s\",)\n", W9xSERVICEKEY, buffer ));
        if (RegSetValueEx(srvkey, W9xSERVICEKEY, 0, REG_SZ,
            (unsigned const char *)buffer, strlen(buffer) + 1) != ERROR_SUCCESS)
        {
          TRACE_LASTERROR((-1,"RegSetValueEx() failed!\n" ));
        }
        else
        {
          TRACE_OUT((-1,"RegSetValueEx() succeeded.\n" ));
          msg = "The distributed.net " SERVICEFOR " has been successfully\n"
                   "installed as a Windows 9x service.";
          retcode = 0;
        }
        TRACE_OUT((+0,"RegCloseKey() ok\n" ));
        RegCloseKey(srvkey);
      }
    }
  }
  #endif

  TRACE_OUT((-1,"win32CliInstallService() retcode=%d, msg=\"%s\".\n", retcode, ((msg)?(msg):("")) ));

  if (msg && !quiet)
    __print_message( msg, (retcode < 0) );

  return (retcode);
}

/* ---------------------------------------------------------- */

//! Send a message back to Windows to acknowledge our state.
/*!
 * \return <0=err, 0=none running, >0=msg sent.
 */
int win32CliSendServiceControlMessage(int msg)
{
  int retcode = -1;
  msg = msg;

  int no_ack_needed  = 0;
  DWORD ack_state[2] = {SERVICE_START_PENDING, SERVICE_START_PENDING};
  if (msg == SERVICE_CONTROL_PARAMCHANGE) //translate win2k only cmd
  {
    msg = CLIENT_SERVICE_CONTROL_RESTART; //to one available on all
  }
  if (__winGetVersion() >= 2000) /* NT */
  {
    if (msg >= CLIENT_SERVICE_CONTROL_FIRST &&
        msg <= CLIENT_SERVICE_CONTROL_LAST)
    {
      retcode = 0;
      ack_state[0] = ack_state[1] = SERVICE_START_PENDING;
      no_ack_needed = 1;
    }
    else
    {
      unsigned int acc;
      static DWORD acccond[][3]=
      {
        {SERVICE_CONTROL_STOP,    SERVICE_STOPPED, SERVICE_STOP_PENDING},
        {SERVICE_CONTROL_PAUSE,   SERVICE_PAUSED,  SERVICE_PAUSE_PENDING},
        {SERVICE_CONTROL_CONTINUE,SERVICE_RUNNING, SERVICE_CONTINUE_PENDING}
      };
      for (acc = 0; acc < (sizeof(acccond)/sizeof(acccond[0])); acc++)
      {
        if ( (acccond[acc][0]) == (DWORD)msg )
        {
          ack_state[0] = acccond[acc][1];
          ack_state[1] = acccond[acc][2];
          retcode = 0;
          break;
        }
      }
    }
  }
  if (retcode == 0)
  {
    SC_HANDLE scm = OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    retcode = -1;
    if (scm)
    {
      unsigned int i;
      int foundcount = 0, stoppedcount = 0;
      retcode = 0;
      for (i = 0; i < ( sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0])); i++)
      {
        SC_HANDLE myService = OpenService(scm, NTSERVICEIDS[i], SERVICE_ALL_ACCESS);
        if (myService)
        {
          SERVICE_STATUS status;
          foundcount++;
          if (!QueryServiceStatus(myService, &status))
          {
            if (retcode <= 0)
              retcode = -1;
          }
          else
          {
            if (status.dwCurrentState != SERVICE_STOPPED)
              retcode = foundcount;
            if (status.dwCurrentState == ack_state[0] ||
                status.dwCurrentState == ack_state[1])
              stoppedcount++;
            else if (ControlService(myService, msg, &status))
            {
              if (no_ack_needed) /* private messages */
                stoppedcount++; // always ok
              else if (status.dwCurrentState == ack_state[0] ||
                       status.dwCurrentState == ack_state[1])
                stoppedcount++;
            }
          }
          CloseServiceHandle(myService);
        }
      }
      //if (foundcount == stoppedcount)
      //  retcode = stoppedcount;
      CloseServiceHandle(scm);
    }
  }

  return retcode;
}

/* ---------------------------------------------------------- */

static void ServiceMain(int argc, char *argv[], int (*cmain)(int, char **))
{
  TRACE_OUT((+1,"started ServiceMain(argc,argv,int *(cmain)(int,char **))\n" ));
  {
    int init_ok = 1;
    char *oargv[3];

    HANDLE hmutex;
    SECURITY_ATTRIBUTES sa;
    char filename[260];
    if (__GetMyModuleFilename(filename, sizeof(filename)) < 1)
      return;
    oargv[1] = strrchr(filename, '\\');
    oargv[2] = strrchr(filename, '/');
    if (oargv[2] > oargv[1]) oargv[1] = oargv[2];
    oargv[2] = strrchr(filename, ':');
    if (oargv[2] > oargv[1]) oargv[1] = oargv[2] + 1;
    if (oargv[1])
    {
      argc = *oargv[1];
      *oargv[1] = '\0';
      SetCurrentDirectory(filename);
      *oargv[1] = (char)argc;
    }

    init_ok = 0;
    memset(&sa,0,sizeof(sa));
    sa.nLength = sizeof(sa);
    SetLastError(0);
    hmutex = CreateMutex( &sa, FALSE, SERVICEMUTEX);
    if (hmutex)
    {
      if (GetLastError() == 0)
        init_ok = 1;
    }
    
    argv    = oargv;
    argv[0] = filename;
    argv[1] = "-hide";
    argv[2] = NULL;
    argc    = 2;

    TRACE_OUT((0,"ServiceMain(): init ok? %d\n", init_ok ));
    if (init_ok)
    {
      win32cli_servicified = 1;
      TRACE_OUT((+1,"main(2,{\"%s\",\"-hide\",NULL})\n", filename ));
      #if defined(SLEEP_ON_SERVICE_START)
      /* sleep a bit to allow other services, fs mounts, etc to kick in */
      /* we've posted that we're running so keep an eye out for STOP reqs */
      {
        int secs = SLEEP_ON_SERVICE_START;
        while ((--secs) >= 0 && win32cli_servicified)
          Sleep(1000);
      }
      #endif
      if (win32cli_servicified)
        (*cmain)( argc, argv );
      TRACE_OUT((-1,"main()\n" ));
      win32cli_servicified = 0;
    }

    if (hmutex)
    {
      ReleaseMutex( hmutex );
      CloseHandle( hmutex );
    }
  }
  TRACE_OUT((-1,"ended ServiceMain()\n"));
  return;
}

/* ---------------------------------------------------------- */

static BOOL __SetServiceStatus(SERVICE_STATUS_HANDLE hServiceStatus,
                               LPSERVICE_STATUS lpServiceStatus)
{
  BOOL success;
  TRACE_OUT((+1,"setting ServiceStatus to %s\n", debug_getsvcStatusName(lpServiceStatus->dwCurrentState) ));
  success = SetServiceStatus(hServiceStatus, lpServiceStatus);
  TRACE_OUT((-1,"SetServiceStatus() => %s\n", ((success)?("ok"):("failed")) ));
  return success;
}

/* ---------------------------------------------------------- */

#if 1
  #define SetServiceBits(a,b,c,d) /* nothing */
#else
BOOL __stdcall SetServiceBits( IN SERVICE_STATUS_HANDLE    hServiceStatus,
                               IN DWORD                    dwServiceBits,
                               IN BOOL                     bSetBitsOn,
                               IN BOOL                     bUpdateImm)
{
  BOOL res = FALSE;
  HMODULE hLib = GetModuleHandle("advapi32.dll");
  if (hLib)
  {
    typedef BOOL (__stdcall *someproc)(SERVICE_STATUS_HANDLE,DWORD,BOOL,BOOL);
    someproc func = (someproc) GetProcAddress( hLib, "SetServiceBits" );
    if (func)
    {
      res = ((*func)(hServiceStatus,dwServiceBits,bSetBitsOn,bUpdateImm));
    }
  }
  return res;
}
#endif

/* ---------------------------------------------------------- */

static void __stdcall ServiceCtrlHandler(DWORD controlCode)
{
  static SERVICE_STATUS_HANDLE serviceStatusHandle = 0;
  DWORD svc_bits = 0x3FC00000; /* some arbitrary bits */
  SERVICE_STATUS serviceStatus;

  DNETC_UNUSED(svc_bits);

  if (serviceStatusHandle == 0)
  {
    unsigned int i;
    TRACE_OUT((+1,"RegisterServiceCtrlHandler()\n" ));
    for (i=0; i<(sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0]));i++)
    {
      serviceStatusHandle = RegisterServiceCtrlHandler(NTSERVICEIDS[i],
                                              ServiceCtrlHandler);
      if (serviceStatusHandle != 0)
      {
        SetServiceBits(serviceStatusHandle, svc_bits, TRUE, TRUE );
        break;
      }
    }
    TRACE_OUT((-1,"RegisterServiceCtrlHandler()=%s serviceStatusHandle=%d\n",
                   ((serviceStatusHandle == 0)?("failed"):("ok")),
                   serviceStatusHandle ));
    if (serviceStatusHandle == 0)
      return;
  }

  TRACE_OUT((+1,"ServiceCtrlHandler(%ld) [%s]\n", controlCode, debug_getsvcControlCodeName(controlCode) ));

  serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  serviceStatus.dwCurrentState = SERVICE_STOPPED;
  serviceStatus.dwControlsAccepted = 0;
  serviceStatus.dwWin32ExitCode = NO_ERROR;
  serviceStatus.dwServiceSpecificExitCode = 0;
  serviceStatus.dwCheckPoint = 0;
  serviceStatus.dwWaitHint = 0;

  if (!win32cli_servicified)
    ; //already done above
  else if (controlCode == SERVICE_CONTROL_SHUTDOWN ||
           controlCode == SERVICE_CONTROL_STOP)
  {
    SetServiceBits(serviceStatusHandle, svc_bits, FALSE, TRUE );
    win32cli_servicified = 0;
    serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    serviceStatus.dwWaitHint = 10000;
    RaiseExitRequestTrigger();
    TRACE_OUT((0,"called RaiseExitRequestTrigger()\n"));
  }
  else
  {
    int restarting = CheckRestartRequestTriggerNoIO();
    int paused = CheckPauseRequestTriggerNoIO();
    int stopping = CheckExitRequestTriggerNoIO();

    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    serviceStatus.dwControlsAccepted = (SERVICE_ACCEPT_STOP|
                                        SERVICE_ACCEPT_SHUTDOWN);
    serviceStatus.dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;

    if (__winGetVersion() >= 2500) /* win2k */
    {
      serviceStatus.dwControlsAccepted |= SERVICE_ACCEPT_PARAMCHANGE;
      if (controlCode == SERVICE_CONTROL_PARAMCHANGE)
      {
        controlCode = CLIENT_SERVICE_CONTROL_RESTART;
      }
    }
    if (controlCode == SERVICE_CONTROL_PAUSE && !paused)
    {
      serviceStatus.dwCurrentState = SERVICE_PAUSE_PENDING;
      serviceStatus.dwWaitHint = 500;
      __SetServiceStatus(serviceStatusHandle, &serviceStatus);
      serviceStatus.dwWaitHint = 0;
      RaisePauseRequestTrigger();
      TRACE_OUT((0,"called RaisePauseRequestTrigger()\n"));
      paused = CheckPauseRequestTriggerNoIO();
      //fallthrough
    }
    else if (controlCode == SERVICE_CONTROL_CONTINUE && paused)
    {
      serviceStatus.dwCurrentState = SERVICE_CONTINUE_PENDING;
      serviceStatus.dwWaitHint = 500;
      __SetServiceStatus(serviceStatusHandle, &serviceStatus);
      serviceStatus.dwWaitHint = 0;
      ClearPauseRequestTrigger();
      TRACE_OUT((0,"called ClearPauseRequestTrigger()\n"));
      paused = CheckPauseRequestTriggerNoIO();
      //fallthrough
    }
    else if (controlCode == CLIENT_SERVICE_CONTROL_RESTART && !stopping)
    {
      serviceStatus.dwCurrentState = SERVICE_START_PENDING;
      serviceStatus.dwWaitHint = 500;
      __SetServiceStatus(serviceStatusHandle, &serviceStatus);
      serviceStatus.dwWaitHint = 0;
      RaiseRestartRequestTrigger();
      restarting = CheckRestartRequestTriggerNoIO();
      //fallthrough
    }
    else if (controlCode == CLIENT_SERVICE_CONTROL_CAPSSTAT)
    {
      DWORD caps = 0; /* capabilities */
      DWORD stat = 0; /* status */

      caps = (1<<( CLIENT_SERVICE_CONTROL_RESTART - CLIENT_SERVICE_CONTROL_FIRST))
          || (1<<( CLIENT_SERVICE_CONTROL_UPDATE  - CLIENT_SERVICE_CONTROL_FIRST));
      if (CheckRestartRequestTrigger())
      {
        stat = (1<<( CLIENT_SERVICE_CONTROL_RESTART - CLIENT_SERVICE_CONTROL_FIRST));
      }
      #if defined(PROXYTYPE)
      if (bForceConnect)
      { /* not really, but oh well */
        stat |= (1<<(CLIENT_SERVICE_CONTROL_UPDATE - CLIENT_SERVICE_CONTROL_FIRST ));
      }
      #else
      {
        int modes = ModeReqIsSet(-1);
        caps |= (1<<(CLIENT_SERVICE_CONTROL_FETCH - CLIENT_SERVICE_CONTROL_FIRST ))
              || (1<<(CLIENT_SERVICE_CONTROL_FLUSH - CLIENT_SERVICE_CONTROL_FIRST ));
        if ((modes & (MODEREQ_FETCH|MODEREQ_FLUSH))==(MODEREQ_FETCH|MODEREQ_FLUSH))
          stat |= (1<<(CLIENT_SERVICE_CONTROL_UPDATE - CLIENT_SERVICE_CONTROL_FIRST ));
        if ((modes & MODEREQ_FETCH)!=0)
          stat |= (1<<(CLIENT_SERVICE_CONTROL_FETCH - CLIENT_SERVICE_CONTROL_FIRST ));
        if ((modes & MODEREQ_FLUSH)!=0)
          stat |= (1<<(CLIENT_SERVICE_CONTROL_FLUSH - CLIENT_SERVICE_CONTROL_FIRST ));
      }
      #endif
      serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
      serviceStatus.dwServiceSpecificExitCode = (((DWORD)caps)|(((DWORD)stat)<<16));
      //fallthrough
    }
    else if (controlCode >= CLIENT_SERVICE_CONTROL_FIRST &&
             controlCode <= CLIENT_SERVICE_CONTROL_LAST && !stopping)
    {
      static struct { DWORD controlCode; void *fxn; int arg; } ctrlOps[] =
      {
        #if defined(PROXYTYPE)
        { CLIENT_SERVICE_CONTROL_FETCH, (void *)TriggerUpdate,  0 },
        { CLIENT_SERVICE_CONTROL_FLUSH, (void *)TriggerUpdate,  0 },
        { CLIENT_SERVICE_CONTROL_UPDATE, (void *)TriggerUpdate, 0 }
        #else
        { CLIENT_SERVICE_CONTROL_FETCH, (void *)ModeReqSet,  MODEREQ_FETCH },
        { CLIENT_SERVICE_CONTROL_FLUSH, (void *)ModeReqSet,  MODEREQ_FLUSH },
        { CLIENT_SERVICE_CONTROL_UPDATE, (void *)ModeReqSet, MODEREQ_FETCH|MODEREQ_FLUSH }
        #endif
      };
      unsigned int op;
       serviceStatus.dwCurrentState = SERVICE_START_PENDING;
      serviceStatus.dwWaitHint = 500;
      __SetServiceStatus(serviceStatusHandle, &serviceStatus);
      serviceStatus.dwWaitHint = 0;
      for (op = 0; op <= (sizeof(ctrlOps)/sizeof(ctrlOps[0])); op++)
      {
        if (ctrlOps[op].controlCode == controlCode)
        {
          if (ctrlOps[op].fxn == ((void *)ModeReqSet))
            ModeReqSet(ctrlOps[op].arg);
          else
            (*((void (*)(void))(ctrlOps[op].fxn)))();
          TRACE_OUT((0,"called ctrlOps[%u].fxn\n",op));
          break;
        }
      }
      restarting = CheckRestartRequestTriggerNoIO();
      //fallthrough
    }
    //fallthrough (SERVICE_CONTROL_INTERROGATE)
    {
      static DWORD laststate = SERVICE_RUNNING; /* some non-pending state */

      if (win32cli_servicified == 0) //already stopped
      {
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwControlsAccepted = 0;
      }
      else if (restarting)
      {
        /* we toggled it above. Now we *have* to switch back to
        ** _RUNNING (not _PENDING), otherwise the service won't
        ** be sent any more control commands.
        ** "The service cannot accept control messages at this time"
        */
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
      }
      else if (stopping)
      {
        static DWORD lastchkpt = 0;
        serviceStatus.dwControlsAccepted = 0;
        serviceStatus.dwWaitHint = 10000;
        serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        serviceStatus.dwCheckPoint = 1;
        if (laststate == serviceStatus.dwCurrentState)
          serviceStatus.dwCheckPoint = ++lastchkpt;
        lastchkpt = serviceStatus.dwCheckPoint;
      }
      else if (paused)
      {
        serviceStatus.dwCurrentState = SERVICE_PAUSED;
      }
      else
      {
        serviceStatus.dwCurrentState = SERVICE_RUNNING;
      }
      laststate = serviceStatus.dwCurrentState;
    }
  }
  __SetServiceStatus(serviceStatusHandle, &serviceStatus);

  TRACE_OUT((-1,"ServiceCtrlHandler(%ld) [%s]\n", controlCode, debug_getsvcControlCodeName(controlCode) ));
  return;
}

/* ---------------------------------------------------------- */

static struct { int (*proc)(int, char **); } ntsvc = {0};

static void WINAPI _NTServiceMain(DWORD Argc, LPTSTR *Argv)
{
  TRACE_OUT((+1,"started _NTServiceMain(%d,{\"%s\",...})\n", Argc, Argv[0]));

  win32cli_servicified = 1; /* turn on for ctrlhandler */
  ServiceCtrlHandler(SERVICE_CONTROL_INTERROGATE); /* set RUNNING */
  ServiceMain( (int)Argc, (char **)Argv, ntsvc.proc );
  win32cli_servicified = 0; /* turn off for ctrlhandler */
  ServiceCtrlHandler(SERVICE_CONTROL_INTERROGATE); /* set STOPPED */

  TRACE_OUT((-1,"ended _NTServiceMain\n" ));
}

/* ---------------------------------------------------------- */

int win32CliInitializeService(int argc, char *argv[],
                              int (*realmain)(int, char **) )
{
  static int is_started = 0;  /* static as recursion guard */

  TRACE_OUT((+1,"win32CliInitializeService(). Already running?=%d\n", is_started ));

  /* for win9x we need to ensure that we don't start as a
     service when we shouldn't.
     On NT, this is not critical, since attempting to init
     as a service isn't possible unless the ServiceManager
     started us - our only motivation is to try to avoid
     getting "hung" for 20 seconds.
  */
  if (!is_started && realmain)
  {
    if (__winGetVersion() < 400) /* win16 */
    {
      // not supported anymore
    }
    #if (CLIENT_OS == OS_WIN32)
    else if (__winGetVersion() < 2000) /* not NT */
    {
      TRACE_OUT((+1,"non-winnt win32CliInitializeService()\n" ));
      if (!getenv("dnet.svcify") && IsShellRunning())
      {
        TRACE_OUT((0,"found shell running. won't servicify.\n" ));
      }
      else if (win32CliIsServiceInstalled() <= 0)
      {
        TRACE_OUT((0,"svc not installed (or error). won't servicify.\n" ));
      }
      else if ((argc < 2) || (!argv) || strcmp(argv[1], "-svcrun") == 0
                                     || strcmp(argv[1], "-hide") == 0 )
      {
        HMODULE kernl = GetModuleHandle("KERNEL32");
        if (kernl)
        {
          typedef DWORD (CALLBACK *ULPRET)(DWORD,DWORD);
          ULPRET func = (ULPRET) GetProcAddress(kernl,
                                        "RegisterServiceProcess");
          if (func)
          {
            if ((*func)(0, 1))
              is_started = 1;
          }
        }
        if (is_started)
        {
          TRACE_OUT((0,"RegisterServiceProcess()=ok\n" ));
          ServiceMain(argc,argv,realmain);
        }
      }
      TRACE_OUT((-1,"non-winnt win32CliInitializeService()\n" ));
    }
    #endif
    #if (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    else if (__winGetVersion() >= 2000) /* NT */
    {
      TRACE_OUT((+1,"beginning winNT section.\n" ));
      int startable = -1; /* dunno */
      startable = IsTryNTStartServiceWorthwhile(); /* +1=yes, 0=no, -1=error */

      if (startable == 0) /* don't try */
      {
        TRACE_OUT((0,"IsTryNTStartServiceWorthwhile()=>0. Won't servicify\n"));
        startable = 0;
      }
      else if (win32CliIsServiceInstalled() == 0) /* not inst, no error */
      {
        TRACE_OUT((0,"svc not installed. won't servicify.\n" ));
        startable = 0;
      }
      if (startable != 0) /* either unknown or positive */
      {
        unsigned int inst;
        SERVICE_TABLE_ENTRY serviceTable[] =
        {
          { NULL, (LPSERVICE_MAIN_FUNCTION)_NTServiceMain },
          { NULL, NULL }
        };
        ntsvc.proc = realmain;
        for (inst=0;inst<(sizeof(NTSERVICEIDS)/sizeof(NTSERVICEIDS[0]));inst++)
        {
          serviceTable[0].lpServiceName = (char *)(NTSERVICEIDS[inst]);
          TRACE_OUT((+1,"StartServiceCtrlDispatcher(\"%s\")\n",serviceTable[0].lpServiceName));
          if (StartServiceCtrlDispatcher(serviceTable))
          {
            TRACE_OUT((-1,"StartServiceCtrlDispatcher()=ok\n" ));
            is_started = 1;
            break;
          }
          TRACE_LASTERROR((-1,"StartServiceCtrlDispatcher failed!\n" ));
        }
      }
      TRACE_OUT((-1,"ended winNT section.\n" ));
    }
    #endif
  }

  TRACE_OUT((-1,"win32CliInitializeService(). svc running?=%d\n", is_started));

  if (is_started)
    return 0;
  return -1;
}

/* ---------------------------------------------------------- */
