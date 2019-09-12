/*
 * Copyright distributed.net 1997-2016 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
*/
const char *client_cpp(void) {
return "@(#)$Id: client.cpp,v 1.272 2016/02/01 16:08:35 stream Exp $"; }

/* ------------------------------------------------------------------------ */

//#define TRACE

#include "cputypes.h"  // CLIENT_OS, CLIENT_CPU
#include "version.h"   // CLIENT_CONTEST, CLIENT_BUILD, CLIENT_BUILD_FRAC
#include "baseincs.h"  // basic (even if port-specific) #includes
#include "projdata.h"  // general project data: ids, flags, states; names, ...
#include "client.h"    // Client class
#include "cliident.h"  // CliGetFullVersionDescriptor()
#include "clievent.h"  // ClientEventSyncPost(),_CLIENT_STARTED|FINISHED
#include "random.h"    // InitRandom()
#include "pathwork.h"  // EXTN_SEP
#include "util.h"      // projectmap_build(), trace, utilCheckIfBetaExpired
#include "modereq.h"   // ModeReqIsSet()/ModeReqRun()
#include "cmdline.h"   // ParseCommandLine() and load config
#include "clitime.h"   // [De]InitializeTimers(),CliTimer()
#include "netbase.h"   // net_[de]initialize()
#include "buffbase.h"  // [De]InitializeBuffers()
#include "triggers.h"  // [De]InitializeTriggers(),RestartRequestTrigger()
#include "logstuff.h"  // [De]InitializeLogging(),Log()/LogScreen()
#include "console.h"   // [De]InitializeConsole(), ConOutErr()
#include "selcore.h"   // [De]InitializeCoreTable()
#include "cpucheck.h"  // GetNumberOfDetectedProcessors()

#if (CLIENT_CPU == CPU_ATI_STREAM)
#include "amdstream_setup.h" // AMDStreamInitialize();
#include "adl.h"             // Thermal control functions
#endif

#if (CLIENT_CPU == CPU_CUDA)
#include "cuda_setup.h"         // InitializeCUDA();
#endif

#if (CLIENT_CPU == CPU_OPENCL)
#include "ocl_setup.h"		// InitializeOpenCL()
#endif

/* ------------------------------------------------------------------------ */

void ResetClientData(Client *client)
{
  /* everything here should also be validated in the appropriate subsystem,
     so if zeroing here causes something to break elsewhere, the subsystem
     init needs fixing.
     When creating new variables, name them such that the default is 0/"",
     eg 'nopauseonbatterypower'.
  */

  int contest;
  memset((void *)client,0,sizeof(Client));

  client->connectoften=4;
  client->nettimeout=60;
  client->autofindkeyserver=1;
  client->crunchmeter=-1;
  projectmap_build(client->project_order_map, client->project_state, "");
  client->numcpu = -1;
  client->devicenum = -1;
  client->corenumtotestbench = -1;
  for (contest=0; contest<CONTEST_COUNT; contest++)
    client->coretypes[contest] = -1;
}

// --------------------------------------------------------------------------

static const char *GetBuildOrEnvDescription(void)
{
  /*
  <cyp> hmm. would it make sense to have the client print build data
  when starting? running OS, static/dynamic, libver etc? For idiots who
  send us log extracts but don't mention the OS they are running on.
  Only useful for win-weenies I suppose.
  */

#if (CLIENT_OS == OS_DOS)
  return dosCliGetEmulationDescription(); //if in win/os2 VM
#elif ((CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16))
  static char buffer[32]; long ver = winGetVersion(); /* w32pre.cpp */
  sprintf(buffer,"Windows%s %u.%u", (ver>=2000)?("NT"):(""), (ver/100)%20, ver%100 );
  return buffer;
#elif (CLIENT_OS == OS_NETWARE)
  static char buffer[40];
  const char *speshul = "";
  int major, minor, revision, servType, loaderType;
  major = GetFileServerMajorVersionNumber();
  minor = GetFileServerMinorVersionNumber();
  revision = GetFileServerRevisionNumber();
  #if 0 /* hmm, this is wrong. NetWare 4.02 != 4.2 */
  if (minor < 10)
    minor *= 10; /* .02 => .20 */
  #endif
  revision = ((revision == 0 || revision > 26)?(0):(revision + ('a'-1)));
  GetServerConfigurationInfo(&servType, &loaderType);
  if (servType == 0 && loaderType > 1) /* OS/2/UnixWare/etc loader */
    speshul = " (nondedicated)";
  else if (servType == 1)
    speshul = " (SFT III IOE)";
  else if (servType == 2)
    speshul = " (SFT III MSE)";
  sprintf(buffer, "NetWare%s %u.%u%c", speshul, major, minor, revision );
  return buffer;
#elif (CLIENT_OS == OS_AMIGAOS)
  static char buffer[40];
  #ifdef __OS3PPC__
    #ifdef __POWERUP__
    #define PPCINFOTAG_EMULATION (TAG_USER + 0x1f0ff)
    sprintf(buffer,"OS %s, PowerUp%s %ld.%ld",amigaGetOSVersion(),((PPCGetAttr(PPCINFOTAG_EMULATION) == 'WARP') ? " Emu" : ""),PPCVersion(),PPCRevision());
    #else
    #define LIBVER(lib) *((UWORD *)(((UBYTE *)lib)+20))
    #define LIBREV(lib) *((UWORD *)(((UBYTE *)lib)+22))
    sprintf(buffer,"OS %s, WarpOS %d.%d",amigaGetOSVersion(),LIBVER(PowerPCBase),LIBREV(PowerPCBase));
    #endif
  #elif defined(__PPC__)
  sprintf(buffer,"OS %s, PowerPC",amigaGetOSVersion());
  #else
  sprintf(buffer,"OS %s, 68K",amigaGetOSVersion());
  #endif
  return buffer;
#elif (CLIENT_OS == OS_MORPHOS)
  static char buffer[40];
  #define __ALTIVEC__ 1 /* crude hack till we have real gcc-altivec */
  #include <exec/resident.h>
  char release[32];
  int mosVer = 0;
  struct Resident *m_res;
  m_res = FindResident("MorphOS");
  if (m_res)
  {
    mosVer = m_res->rt_Version;
    if (m_res->rt_Flags & RTF_EXTENDED)
    {
      sprintf(release, "%d.%d", mosVer, m_res->rt_Revision);
    }
    else
    {
      /* Anchient MorphOS: parse string (example): MorphOS ID 0.4 (dd.mm.yyyy) */
      const char *ps = (const char *) m_res->rt_IdString;
      const char *p = ps;
      char *d = release;
      if (p && *p)
      {
        while (*p && *p != '.') p++;
        if (*p)
        {
          while (p > ps && p[-1] != ' ') p--;

          while (*p && *p != ' ')
          {
            *d++ = *p++;
          }
          *d = '\0';
        }
      }
      if (d == release)
      {
        /* oops, no clue of revision */
        sprintf(release, "%d.?", mosVer);
      }
    }
  }
  else
  {
    strcpy(release, "?.?");
  }
  sprintf(buffer,"MorphOS %s", release);
  return buffer;
#elif defined(__unix__) /* uname -sr */
  struct utsname ut;
  if (uname(&ut)==0)
  {
    #if (CLIENT_OS == OS_AIX)
    // on AIX version is the major and release the minor
    static char buffer[sizeof(ut.sysname)+1+sizeof(ut.release)+1+sizeof(ut.version)+1];
    return strcat(strcat(strcat(strcat(strcpy(buffer,ut.sysname)," "),ut.version),"."),ut.release);
    #elif (CLIENT_OS == OS_QNX) && !defined(__QNXNTO__)
    static char buffer[sizeof(ut.sysname)+1+
		       sizeof(ut.version)+1+
		       sizeof(ut.release)+1];
    return strcat(strcat(strcat(strcpy(buffer,ut.sysname)," "),
			 ut.version),ut.release);;
    #elif (CLIENT_OS == OS_NEXTSTEP)
    kernel_version_t v;
    static char buf[sizeof(ut.nodename)+ sizeof(ut.sysname) + sizeof(ut.version) +
                    sizeof(ut.release) + sizeof(ut.machine) +sizeof(v) + 6];

    strcpy(buf, ut.sysname);
    strcat(buf, " ");
    strcat(buf, ut.version);
    strcat(buf, ".");
    strcat(buf, ut.release);
    strcat(buf, " ");
    strcat(buf, ut.machine);
    strcat(buf, "\n\t");

    if (host_kernel_version(host_self(), v) != KERN_SUCCESS)
      return "version inquiry failed";

    if (v[strlen(v) - 1] == '\n')
      v[strlen(v) - 1] = '\0';

    if (strstr(v, "; root"))
      *strstr(v, "; root") = '\0';

    strcat(buf, v);
    return buf;
    #else
    static char buffer[sizeof(ut.sysname)+1+sizeof(ut.release)+1];
    return strcat(strcat(strcpy(buffer,ut.sysname)," "),ut.release);
    #endif
  }
  return "";
#elif (CLIENT_OS == OS_RISCOS)
  return riscos_version();
#else
  return "";
#endif
}

/* ---------------------------------------------------------------------- */

static void PrintBanner(const char *dnet_id,int level,int restarted,int logscreenonly)
{
  restarted = 0; /* yes, always show everything */
  int logto = LOGTO_RAWMODE|LOGTO_SCREEN;
  if (!logscreenonly)
    logto |= LOGTO_FILE|LOGTO_MAIL;

  if (!restarted)
  {
    if (level == 0)
    {
      LogScreenRaw( "\ndistributed.net client for " CLIENT_OS_NAME_EXTENDED " "
                    "Copyright 1997-2016, distributed.net\n");
#if defined HAVE_RC5_72_CORES
      #if (CLIENT_CPU == CPU_ARM)
        LogScreenRaw( "ARM assembly by Peter Teichmann\n");
      #endif
      #if (CLIENT_CPU == CPU_68K)
      LogScreenRaw( "RC5-72 68K assembly by Malcolm Howell and John Girvin\n");
      #endif
      #if (CLIENT_CPU == CPU_POWERPC) || (CLIENT_CPU == CPU_CELLBE)
        #if (CLIENT_CPU == CPU_POWERPC)
        LogScreenRaw( "RC5-72 PowerPC assembly by Malcolm Howell and Didier Levet\n"
                      "Enhancements for 604e CPUs by Roberto Ragusa\n");
        #endif
        #if defined(HAVE_ALTIVEC)
          #if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
            LogScreenRaw( "RC5-72 Altivec and OGR assembly by Didier Levet\n");
          #else
            LogScreenRaw( "RC5-72 Altivec assembly by Didier Levet\n");
          #endif
        #endif
      #endif
      #if (CLIENT_CPU == CPU_CELLBE)
      LogScreenRaw( "RC5-72 and OGR-P2 SPE assembly by Decio Luiz Gazzoni Filho\n");
        #ifdef HAVE_OGR_CORES
	  LogScreenRaw( "OGR-NG SPE assembly by Roman Trunov\n");
        #endif
      #endif
      #if (CLIENT_CPU == CPU_SPARC)
      LogScreenRaw( "RC5-72 SPARC assembly by Didier Levet and Andreas Beckmann\n");
      #endif
#endif
      #if (CLIENT_OS == OS_DOS)
      LogScreenRaw( "PMODE DOS extender Copyright 1994-1998, Charles Scheffold and Thomas Pytel\n");
      #endif

      LogScreenRaw( "Please visit http://www.distributed.net/ for up-to-date contest information.\n");
      LogScreenRaw( "Start the client with '-help' for a list of valid command line options.\n" );
      LogScreenRaw( "\n" );
    }
    else if ( level == 1 || level == 2 )
    {
      const char *msg = GetBuildOrEnvDescription();
      if (msg == NULL) msg="";

      LogTo( logto, "\n%s%s%s%s.\n",
                    CliGetFullVersionDescriptor(), /* cliident.cpp */
                    ((*msg)?(" ("):("")), msg, ((*msg)?(")"):("")) );
      LogScreenRaw( "Please provide the *entire* version descriptor "
                    "when submitting bug reports.\n");
      LogScreenRaw( "The distributed.net bug report pages are at "
                    "http://bugs.distributed.net/\n");
      if (dnet_id[0] != '\0' && strcmp(dnet_id,"rc5@distributed.net") !=0 &&
          utilIsUserIDValid(dnet_id) )
        LogTo( logto, "Using email address (distributed.net ID) \'%s\'\n",dnet_id);
      else if (level == 2)
        LogTo( logto, "\n* =========================================================================="
                      "\n* The client is not configured with your email address (distributed.net ID) "
                      "\n* Work done cannot be credited until it is set. Please run '%s -config'"
                      "\n* =========================================================================="
                      "\n", utilGetAppName());
      LogTo( logto, "\n");
      #if CLIENT_OS == OS_IRIX
      /*
       * Irix 6.5 has a kernel bug that will cause the system
       * to freeze if we're running as root on a multi-processor
       * machine and we try to use more than one CPU.
       */
      if (geteuid() == 0) {
	       LogScreenRaw("* Cannot run as the superuser on Irix.\n"
		                  "* Please run the client under a non-zero uid.\n\n");
         exit(1);
      }
      #endif /* CLIENT_OS == OS_IRIX */

      if (CliIsTimeZoneInvalid()) /*clitime.cpp (currently DOS,OS/2,WIN[16] only)*/
      {
        LogScreenRaw("Warning: The TZ= variable is not set in the environment. "
         "The client will\nprobably display the wrong time and/or select the "
         "wrong keyserver.\n");
      }
    }
  }
  return;
}
/* ---------------------------------------------------------------------- */

// --------------------------------------
// Do co-processor specific initialization
// This function can be called many times from restart loop,
// but GPU init must be done only once. Protect it here too
// (don't rely on GPU init code).
// --------------------------------------
static void InitializeCoProcessorsOnce(Client *client)
{
  static char done;

  if (!done)
  {
    done = 1;

    // GPU init must return <0 on error
    #if (CLIENT_CPU == CPU_ATI_STREAM)
      #define GPU_NAME "AMD STREAM"
      #define GPU_INIT AMDStreamInitialize()
    #endif

    #if (CLIENT_CPU == CPU_CUDA)
      #define GPU_NAME "CUDA"
      #define GPU_INIT InitializeCUDA()
    #endif

    #if (CLIENT_CPU == CPU_OPENCL)
      #define GPU_NAME "OpenCL"
      #define GPU_INIT InitializeOpenCL()
    #endif

    #ifdef GPU_NAME
    if (GPU_INIT < 0)
      Log("Unable to initialize " GPU_NAME "\n");
    else if (GetNumberOfDetectedProcessors() < 1)  /* Must be positive on GPU */
      Log("No " GPU_NAME " compatible devices found\n");
    #undef GPU_NAME
    #undef GPU_INIT
    #endif
  }

  // Now it's ok to call GetNumberOfDetectedProcessors().
  // Do delayed check of -devicenum option now.
  // Style of error messages will differs a bit, but it was hackish
  // from the beginning...
  int nDev = GetNumberOfDetectedProcessors();
  if (nDev < 0)  // <0 if autodetection failed. 0 is valid for GPU
    nDev = 1;
  if (client->devicenum >= nDev)
  {
    Log("Device ID %d exceed number of detected devices (%d), ignored\n", client->devicenum, nDev);
    client->devicenum = -1;
  }
}

static int ClientMain( int argc, char *argv[] )
{
  Client *client = (Client *)0;
  int retcode = 0;
  int restart = 0;

  TRACE_OUT((+1,"Client.Main()\n"));

  {
    // sanity check for forgetful people :)
    char buf[32];
    const char *ver = CliGetFullVersionDescriptor();
    while (*ver && !isdigit(*ver))
      ++ver;
    sprintf(buf, "%d.%d%02d-%d-", CLIENT_MAJOR_VER, CLIENT_CONTEST,
                                 CLIENT_BUILD, CLIENT_BUILD_FRAC);
    if (strncmp(buf, ver, strlen(buf)) != 0)
    {
      ConOutErr( "Version mismatch." );
      return -1;
    }
  }

  if (InitializeTimers()!=0)
  {
    ConOutErr( "Unable to initialize timers." );
    return -1;
  }
  client = (Client *)malloc(sizeof(Client));
  if (!client)
  {
    DeinitializeTimers();
    ConOutErr( "Unable to initialize client. Out of memory." );
    return -1;
  }
  srand( (unsigned) time(NULL) );
  InitRandom();

  do
  {
    int restarted = restart;
    restart = 0;

    ResetClientData(client); /* reset everything in the object */
    ClientEventSyncPost( CLIEVENT_CLIENT_STARTED, &client, -1 );
    //ReadConfig() and parse command line - returns !0 if shouldn't continue

    TRACE_OUT((0,"Client.parsecmdline restarted?: %d\n", restarted));
    if (!ParseCommandline(client,0,argc,(const char **)argv,&retcode,restarted))
    {
      int domodes = ModeReqIsSet(-1); /* get current flags */
      TRACE_OUT((0,"initializetriggers\n"));
      if (InitializeTriggers(domodes,
                             ((domodes)?(""):(client->exitflagfile)),
                             client->pausefile,
                             client->pauseplist,
                             ((domodes)?(0):(client->restartoninichange)),
                             client->inifilename,
                             client->watchcputempthresh, client->cputempthresh,
                             (!client->nopauseifnomainspower) ) == 0)
      {
        TRACE_OUT((0,"CheckExitRequestTrigger()=%d\n",CheckExitRequestTrigger()));
        // no messages from the following CheckExitRequestTrigger() will
        // ever be seen. Tough cookies. We're not about to implement pre-mount
        // type kernel msg logging for the few people with exit flagfile.
        if (!CheckExitRequestTrigger())
        {
          TRACE_OUT((0,"initializeconnectivity\n"));
          if (net_initialize() == 0) //do global initialization
          {
            #ifdef LURK /* start must come just after initializeconnectivity */
            TRACE_OUT((0,"LurkStart()\n")); /*and always before initlogging*/
            LurkStart(client->offlinemode, &(client->lurk_conf));
            #endif
            TRACE_OUT((0,"initializeconsole\n"));
            if (InitializeConsole(&(client->quietmode),domodes) == 0)
            {
              //some plats need to wait for user input before closing the screen
              int con_waitforuser = 0; //only used if doing modes (and !-config)

              TRACE_OUT((+1,"initializelogging\n"));
              InitializeLogging( (client->quietmode!=0),
                                 client->crunchmeter,
                                 0, /* nobaton */
                                 client->logname,
                                 client->logrotateUTC,
                                 client->logfiletype,
                                 client->logfilelimit,
                                 ((domodes)?(0):(client->messagelen)),
                                 client->smtpsrvr, 0,
                                 client->smtpfrom,
                                 client->smtpdest,
                                 client->id );
              TRACE_OUT((-1,"initializelogging\n"));

              // GPU init is complex, it need logging to catch many possible errors but
              // must be done early because GetNumberOfDetectedProcessors() depends on it.
              //
              // Approach #1. Call it here, soon after InitializeLogging(). Safe, but this
              // could make some messages printed before banner and hard to see.
              //
              // Approach #2 (used). Put two separate init calls before ModeReqRun()
              // and ClientRun(). Nice messages (everything is after banner), good user
              // experience, but too much code above which may require CPU info.
              // E.g. now, InitializeCoreTable() asks for CPU info on AIX platform.
              // It'll not affect GPUs but be warned that this code is more fragile.
              // Don't call GetNumberOfDetectedProcessors() & Co. on early stages.
              //
              if (BufferInitialize(client) == 0)
              {
//              InitializeCoProcessorsOnce(client);  /* Approach #1 - see comment above */

                InitRandom2( client->id );

                TRACE_OUT((+1,"initcoretable\n"));
                if (InitializeCoreTable( &(client->coretypes[0]) ) == 0)
                {
                  if ((domodes & MODEREQ_CONFIG)==0)
                  {
                    PrintBanner(client->id,0,restarted,0);
       
                    TRACE_OUT((+1,"parsecmdline(1)\n"));
                    if (!client->quietmode)
                    {                 //show overrides
                      ParseCommandline( client, 1, argc, (const char **)argv,
                                        &retcode, restarted );
                    }
                    TRACE_OUT((-1,"parsecmdline(1)\n"));
                  }
                  if (domodes)
                  {
                    con_waitforuser = ((domodes & ~MODEREQ_CONFIG)!=0);
                    if ((domodes & MODEREQ_CONFIG)==0)
                    { /* avoid printing/logging banners for nothing */
                      PrintBanner(client->id,1,restarted,((domodes & MODEREQ_CMDLINE_HELP)!=0));
                    }
                    InitializeCoProcessorsOnce(client);  /* Approach #2 - see comment above */
                    TRACE_OUT((+1,"modereqrun\n"));
                    ModeReqRun( client );
                    TRACE_OUT((-1,"modereqrun\n"));
                    restart = 0; /* *never* restart when doing modes */
                  }
                  else if (!utilCheckIfBetaExpired(1)) /* prints message */
                  {
                    con_waitforuser = 0;
                    PrintBanner(client->id,2,restarted,0);
                    InitializeCoProcessorsOnce(client);  /* Approach #2 - see comment above */
                    TRACE_OUT((+1,"client.run\n"));
                    retcode = ClientRun(client);
                    TRACE_OUT((-1,"client.run\n"));
                    restart = CheckRestartRequestTrigger();
                  }
                  TRACE_OUT((0,"deinit coretable\n"));
                  DeinitializeCoreTable();
                }
                else {
                  ConOutErr( "Unable to initialize cores.");
                }
                BufferDeinitialize(client);
              }

              TRACE_OUT((0,"deinitialize logging\n"));
              DeinitializeLogging();
              TRACE_OUT((0,"deinitialize console (waitforuser? %d)\n",con_waitforuser));
              DeinitializeConsole(con_waitforuser);
            } /* if (InitializeConsole() == 0) */
            #ifdef LURK
            TRACE_OUT((0,"LurkStop()\n"));
            LurkStop(); /* just before DeinitializeConnectivity() */
            #endif
            TRACE_OUT((0,"deinitialize connectivity\n"));
            net_deinitialize(!restart /* final call */);
          } /* if (InitializeConnectivity() == 0) */
        } /* if (!CheckExitRequestTrigger()) */
        TRACE_OUT((0,"deinitialize triggers\n"));
        DeinitializeTriggers();
      }
    }
    ClientEventSyncPost( CLIEVENT_CLIENT_FINISHED, &restart, sizeof(restart) );
    TRACE_OUT((0,"client.parsecmdline restarting?: %d\n", restart));
  } while (restart);

  free((void *)client);
  DeinitializeTimers();

  #if (CLIENT_CPU == CPU_ATI_STREAM)
  ADLdeinit();
  #endif

  TRACE_OUT((-1,"Client.Main()\n"));
  return retcode;
}

/* ---------------------------------------------------------------------- */

#if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
//if you get compile or link errors it is probably because you compiled with
//STRICT, which is a no-no when using cpp (think 'overloaded')
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "w32pre.h"       // prelude
int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszCmdLine, int nCmdShow)
{ /* parse the command line and call the bootstrap */
  TRACE_OUT((+1,"WinMain()\n"));
  int rc=winClientPrelude( hInst, hPrevInst, lpszCmdLine, nCmdShow, ClientMain);
  TRACE_OUT((-1,"WinMain()\n"));
  return rc;
}
#elif defined(__unix__)
int main( int argc, char *argv[] )
{
  /* the SPT_* constants refer to sendmail source (conf.[c|h]) */
  const char *defname = utilGetAppName(); /* 'rc5/des' or 'dnetc' etc */
  int needchange = 0;
  if (argv && argv[0])
  {
    char *p = strrchr( argv[0], '/' );
    needchange = (strcmp( ((p)?(p+1):(argv[0])), defname )!=0);

    #if (CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_PS2LINUX) || (CLIENT_OS == OS_ANDROID)
    /* discard dir component from argv[0] when started from init.d */
    if (!needchange && *argv[0] == '/' && strlen(argv[0]) > (strlen(defname)+10))
    {
      int i;
      for (i = 1; i < (argc-1); i++)
      {
        if (strcmp(argv[i],"-ini")==0 || strcmp(argv[i],"--ini")==0)
        {
          needchange = 1;
          break;
        }
      }
    }
    #endif
  }
  #if (CLIENT_OS == OS_HPUX)                         //SPT_TYPE == SPT_PSTAT
  if (needchange)
  {
    union pstun pst;
    pst.pst_command = (char *)defname;
    pstat(PSTAT_SETCMD,pst,strlen(defname),0,0);
  }
  #elif (CLIENT_OS == OS_SCO)                        //SPT_TYPE == SPT_SCO
  #include <sys/user.h>                      // PSARGSZ
  #include <sys/immu.h>                      // UVUBLK
  if (needchange)
  {
    #ifndef _PATH_KMEM /* should have been defined in <paths.h> */
    # define _PATH_KMEM "/dev/kmem"
    #endif
    int kmem = open(_PATH_KMEM, O_RDWR, 0);
    if (kmem >= 0)
    {
      //pid_t kmempid = getpid();
      struct user u;
      off_t seek_off;
      char buf[PSARGSZ];
      (void) fcntl(kmem, F_SETFD, 1);
      memset( buf, 0, PSARGSZ );
      strncpy( buf, defname, PSARGSZ );
      buf[PSARGSZ - 1] = '\0';
      seek_off = UVUBLK + (off_t) u.u_psargs - (off_t) &u;
      if (lseek(kmem, (off_t) seek_off, SEEK_SET) == seek_off)
        (void) write(kmem, buf, PSARGSZ);
      /* yes, it stays open */
    }
  }
  #elif (CLIENT_OS == OS_MACOSX) || (CLIENT_OS == OS_IOS)     /* SPT_TYPE==SPT_PSSTRINGS */
  /* NOTHING - PS_STRINGS are no longer supported and there's no way to
  ** change the name of our process.
  if (needchange)
  {
    PS_STRINGS->ps_nargvstr = 1;
    PS_STRINGS->ps_argvstr = (char *)defname;
  }
  */
  #elif 0                                     /*  SPT_TYPE == SPT_SYSMIPS */
  if (needchange)
  {
    sysmips(SONY_SYSNEWS, NEWS_SETPSARGS, buf);
  }
  #else //SPT_TYPE == SPT_CHANGEARGV (mach) || SPT_NONE (OS_DGUX|OS_DYNIX)
        //|| SPT_REUSEARGV (the rest)
  /* [Net|Free|Open|BSD[i]] are of type SPT_BUILTIN, ie use setproctitle()
     which is a stupid smart-wrapper for SPT_PSSTRINGS (see above). The result
     is exactly the same as when we reexec(): ps will show "appname (filename)"
     so we gain nothing other than avoiding the exec() call, but the way /we/
     would use it is non-standard, so we'd better leave it be:
     __progname = defname; setproctitle(NULL); //uses default, ie __progname
  */
  #if (CLIENT_OS != OS_FREEBSD) && (CLIENT_OS != OS_NETBSD) && \
      (CLIENT_OS != OS_BSDOS) && (CLIENT_OS != OS_OPENBSD) && \
      (CLIENT_OS != OS_DGUX) && (CLIENT_OS != OS_DYNIX) && \
      (CLIENT_OS != OS_NEXTSTEP) && (CLIENT_OS != OS_DRAGONFLY)
  /* ... all the SPT_REUSEARGV types */
  if (needchange && strlen(argv[0]) >= strlen(defname))
  {
    char *q = "RC5PROG";
    int didset = 0;
    #if (CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_IRIX) || \
        (CLIENT_OS == OS_AIX) || (CLIENT_OS == OS_BEOS) || \
        (CLIENT_OS == OS_DEC_UNIX)
    char *m = (char *)malloc( strlen(q)+1+strlen(argv[0])+1 );
    if (m) {
      didset=(0==putenv(strcat(strcat(strcpy(m,q),"="),argv[0]))); //BSD4.3
      free((void *)m);
    }
    #else
    didset = (setenv( q, argv[0], 1 ) == 0); //SYSV7 and posix
    #endif
    if (didset)
    {
      if ((q = (char *)getenv(q)) != ((char *)0))
      {
        int padchar = 0; /* non-linux/qnx/aix may need ' ' here */
        memset(argv[0],padchar,strlen(argv[0]));
        memcpy(argv[0],defname,strlen(defname));
        argv[0] = q;
        needchange = 0;
      }
    }
  }
  #endif
  if (needchange)
  {
    char buffer[512];
    unsigned int len;  char *p;
    if (getenv("RC5INI") == NULL)
    {
      int have_ini = 0;
      for (len=1;len<((unsigned int)argc);len++)
      {
        p = argv[len];
        if (*p == '-')
        {
          if (p[1]=='-')
            p++;
          if (strcmp(p,"-ini")==0)
          {
            have_ini++;
            break;
          }
        }
      }
      if (!have_ini)
      {
        strcpy( buffer, "RC5INI=" );
        strncpy( &buffer[7], argv[0], sizeof(buffer)-7 );
        buffer[sizeof(buffer)-5]='\0';
        strcat( buffer, ".ini" );
        #if (CLIENT_OS == OS_SOLARIS) || (CLIENT_OS == OS_IRIX) || \
            (CLIENT_OS == OS_AIX) || (CLIENT_OS == OS_BEOS) || \
	    (CLIENT_OS == OS_DYNIX) || (CLIENT_OS == OS_DEC_UNIX)
        putenv( buffer );                 //BSD4.3
        #else
        setenv("RC5INI", &buffer[7], 1 ); //SYSV7 and posix
        #endif
      }
    }

    p = argv[0];
    ((const char **)argv)[0] = defname;
    if ((strlen(p) + 5) < sizeof(buffer))
    {
      char *s;
      strcpy( buffer, p );
      s = strrchr( buffer, '/' );
      if (s != NULL)
      {
        s[1]='\0';
        strcat( buffer, defname );
        argv[0] = buffer;
      }
    }

#if (CLIENT_OS == OS_QNX) && !defined(__QNXNTO__)
    if ( execvp( p, (char const * const *)&argv[0] ) == -1)
#elif (CLIENT_OS == OS_NEXTSTEP)
    if ( execvp( p, (const char **)&argv[0] ) == -1)
#else
    if ( execvp( p, &argv[0] ) == -1)
#endif
    {
      ConOutErr("Unable to restart self");
      return -1;
    }
    return 0;
  }
  #endif
  return ClientMain( argc, argv );
}
#elif (CLIENT_OS == OS_RISCOS)
const char* const __dynamic_da_name = "dnetc heap";
int __riscosify_control=__RISCOSIFY_NO_PROCESS;

int main( int argc, char *argv[] )
{
  riscos_in_taskwindow = riscos_check_taskwindow();
  if (riscos_find_local_directory(argv[0]))
  {
    printf("Unable to determine local directory.\n");
    return -1;
  }
  return ClientMain( argc, argv );
}
#elif (CLIENT_OS == OS_NETWARE)
int main( int argc, char *argv[] )
{
  int rc = 0;
  if ( nwCliInitClient( argc, argv ) != 0) /* set cwd etc */
    return -1;
  rc = ClientMain( argc, argv );
  nwCliExitClient(); // destroys AES process, screen, polling procedure
  return rc;
}
#elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
int main( int argc, char *argv[] )
{
  int rc = 20;
  if (amigaInit(&argc,&argv))
  {
    rc = ClientMain( argc, argv );
    if (rc) rc = 5; //Warning
    amigaExit();
  }
  return rc;
}
#else
int main( int argc, char *argv[] )
{
  int rc;
  TRACE_OUT((+1,"main()\n"));
  rc = ClientMain( argc, argv );
  TRACE_OUT((-1,"main()\n"));
  return rc;
}
#endif

