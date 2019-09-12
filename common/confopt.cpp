/* 
 * Copyright distributed.net 1997-2015 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
*/
const char *confopt_cpp(void) {
return "@(#)$Id: confopt.cpp,v 1.64 2012/08/08 19:00:49 sla Exp $"; }

/* ----------------------------------------------------------------------- */

#include "cputypes.h" // CLIENT_OS
#include "pathwork.h" // EXTN_SEP
#include "baseincs.h" // NULL
#include "client.h"
#include "util.h"     // projectmap_expand()
#include "confopt.h"  // ourselves

/* ----------------------------------------------------------------------- */

static const char *lurkmodetable[] =
{
  "Normal mode",
  "Dial-up detection mode",
  "Dial-up detection ONLY mode"
};

static const char *__default_loadorder()
{
  static char buf[64];
  strncpy(buf, projectmap_expand(NULL, NULL), sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  return buf;
}

// --------------------------------------------------------------------------

#define CFGTXT(x) (x)
#define ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "Additional buffer-level checking"

struct optionstruct conf_options[CONF_OPTION_COUNT] = {
{ 
  CONF_MENU_MISC_PLACEHOLDER   , /* */
  CFGTXT("General Client Options"),"",
  CFGTXT(""),
  CONF_MENU_MAIN,CONF_TYPE_MENU,NULL,NULL,CONF_MENU_MISC,0,NULL,NULL
},
{ 
  CONF_ID                      , /* CONF_MENU_MISC */
  CFGTXT("Your email address (distributed.net ID)"), "",
  CFGTXT(
  "Completed work sent back to distributed.net are tagged with the email\n"
  "address of the person whose machine completed that work. That address is\n"
  "used as a unique 'account' identifier in four ways :\n"
  "- This is how distributed.net will contact the owner of the machine that\n"
  "  submits the winning key.\n"
  "- This is the address your participant stats page password will be sent to\n"
  "  upon request.\n"
  "- The owner of that address receives credit for completed work which may\n"
  "  then be transferred to a team account.\n"
  "- The number of work-units completed may be used as votes in the selection\n"
  "  of a recipient of the prize-money reserved for a non-profit organization.\n"
  ),CONF_MENU_MISC,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_COUNT                   , /* CONF_MENU_MISC */
  CFGTXT("Complete this many packets, then exit"), "0 (no limit)",
  CFGTXT(
  "This option specifies that you wish to have the client exit after it has\n"
  "crunched a predefined number of packets. Use 0 (zero) to apply 'no limit',\n"
  "or -1 to have the client exit when the input buffer is empty (this is the\n"
  "equivalent to the deprecated -runbuffers command line option.)\n"
  ),CONF_MENU_MISC,CONF_TYPE_INT,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_HOURS                   , /* CONF_MENU_MISC */
  CFGTXT("Run for this long, then exit"), "0:00 (no limit)",
  CFGTXT(
  "This option specifies that you wish to have the client exit after it has\n"
  "crunched a predefined number of hours. Use 0:00 (or clear the field) to\n"
  "specify 'no limit'.\n"
  ),CONF_MENU_MISC,CONF_TYPE_TIMESTR,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_PAUSEFILE               , /* CONF_MENU_MISC */
  CFGTXT("Pause flagfile Path/Name"),"",
  CFGTXT(
  "While running, the client will occasionally look for the the presence of\n"
  "this file. If it exists, the client will immediately suspend itself and\n"
  "will continue to remain suspended as long as the file is present.\n"
  ),CONF_MENU_MISC,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_EXITFILE                , /* CONF_MENU_MISC */
  CFGTXT("Exit flagfile Path/Name"),DEFAULT_EXITFLAGFILENAME,
  CFGTXT(
  "While running, the client will occasionally look for the the presence of\n"
  "this file. If it exists, the client will immediately exit and will not\n"
  "start as long as the file continues to exist.\n"
  ),CONF_MENU_MISC,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_RESTARTONINICHANGE      , /* CONF_MENU_MISC */
  CFGTXT("Enable restart on .ini file change ?"),"no",
  CFGTXT(
  "When enabled, this option will cause the client to restart itself if it\n"
  "finds that the time/date of the .ini file has changed. Command line options\n" 
  "that were in effect when the client started remain in effect, but all other\n"
  "options (including the distributed.net ID) will be reloaded.\n"
  ),CONF_MENU_MISC,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_PAUSEPLIST              , /* CONF_MENU_MISC */
  CFGTXT("\"Pause if running\""),"",
  /*CFGTXT( */
  "The client will pause itself when any of the applications/process names\n"
  "listed here are found to be running. Multiple filenames/process names\n"
  "may be specified by separating them with a '|'.\n"
  "\n"
  #if (CLIENT_OS == OS_MACOSX || CLIENT_OS == OS_IOS)
  "For example, \"DVD Player|Safari\". Providing a path is not permitted.\n"
  "You can invoke the \"ps -acxw\" command to determine what are the real\n"
  "names of the processes currently running on your machine.\n"
  #elif defined(__unix__)
  "For example, \"backup|restore\". Providing a path is not advisable\n"
  "since the process list is obtained from 'ps' and/or the programmatic\n"
  "equivalent thereof.\n"
    #if (CLIENT_OS == OS_HPUX) && (ULONG_MAX == 0xfffffffful) /* 32bit client */
    "\nThis option is not supported by 32bit clients for 64bit HP/UX.\n"
    #endif
  #elif (CLIENT_OS == OS_NETWARE)
  "For example, \"backup.nlm|restore.nlm|\". An extension (.DLL|.NLM etc) is\n"
  "optional and defaults to 'NLM'. Path specifiers, if provided, have no\n"
  "influence on the outcome of a search.\n" 
  #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16)
  "For example, \"backup.exe|restore.exe\".\n"
  "Win9x: path and extension are optional (that is, they won't influence\n"
  "       the outcome of a search if not explicitly specified).\n"
  "WinNT: path and extension are accepted, but ignored (ie, they have no\n"
  "       influence on the outcome of a search).\n"
  "       32bit clients cannot 'see' 16bit applications and vice-versa.\n"
  "Win16: extension is optional and path is ignored.\n"
  "Clients that are not running as a service can also detect by window title\n"
  "and/or by window class. To specify a window title, prefix it with '*', for\n"
  "example, \"*Backup Exec\". Prefix class names with '#'. Searching by window\n"
  "title and/or class name is exact, ie sensitive to case and name length.\n"
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  "The name of a process is case sensitive and must match the internal task name\n"
  "exactly. For example 'DiskSalv|Quake'.\n"
  #else
  "This option is not supported on all platforms\n"
  #endif
  /*)*/,CONF_MENU_MISC,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_PAUSEIFCPUTEMPHIGH      , /* CONF_MENU_MISC */
  CFGTXT("Pause if processor temperature thresholds are exceeded ?"),"no",
  "If this option is enabled, the client will pause itself if it finds that\n"
  "the temperature of a processor exceeds the threshold specified in the\n"
  "\"Processor temperature thresholds\" option.\n"
  #if (CLIENT_OS == OS_MACOSX)
  "\n"
  "Processor temperature checking is only supported under Mac OS with certain\n"
  "PowerPC G3 or G4 processors (those featuring a Thermal Assist Unit, TAU),\n"
  "or on machines that have dedicated temperature sensors.\n"
  #elif (CLIENT_OS == OS_DEC_UNIX)
  "\n"
  "Processor temperature checking is only supported under OSF/1 with certain\n"
  "Alpha systems, and the envmon subsystem must be loaded.\n"
  "\n"
  "\"/sbin/sysconfig -c envmon\" as a privileged user will load the system.\n"
  #endif
  ,CONF_MENU_MISC,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_CPUTEMPTHRESHOLDS       , /* CONF_MENU_MISC */
  CFGTXT("Processor temperature threshold(s)"),"",
  "Processor temperature threshold(s) may be specified either as\n"
  " - a low:high pair: The client will pause when temperature exceeds 'high',\n" 
  "   and remain paused until temperature has fallen below 'low'.\n"
  "   Examples: \"318K:333K\", \"573R:599R\", \"113F:140F\", \"45.6C:50.2C\".\n"
  " - a single value: The client will treat this as a 'high', with 'low' being\n"
  "   90 percent of the K equivalent of 'high'.\n"
  "\n"
  "As noted above, temperatures may be specified in Kelvin (K, the default),\n"
  "Rankine (R), degrees Fahrenheit (F) or degrees Celsius/Centigrade (C).\n"
  "The precision allowed is two digits after the decimal point.\n"
  "\n"
  "Illegal values, for instance a 'low' that is not less or equal than 'high',\n"
  "will render the pair invalid and cause temperature threshold checking to\n"
  "be silently disabled.\n"
  #if (CLIENT_OS == OS_MACOSX)
  "\n"
  "Processor temperature checking is only supported under Mac OS with certain\n"
  "PowerPC G3 or G4 processors (those featuring a Thermal Assist Unit, TAU),\n"
  "or on machines that have dedicated temperature sensors.\n"
  #elif (CLIENT_OS == OS_DEC_UNIX)
  "\n"
  "Processor temperature checking is only supported under OSF/1 with certain\n"
  "Alpha systems, and the envmon subsystem must be loaded.\n"
  "\n"
  "\"/sbin/sysconfig -c envmon\" as a privileged user will load the system.\n"
  #endif
  ,CONF_MENU_MISC,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_PAUSEIFBATTERY          , /* CONF_MENU_MISC */
  CFGTXT("Pause if running on battery power ?"),"yes",
  "If this option is enabled, the client will pause itself if it finds the\n"
  "system is running on battery power or, where more appropriate, if it finds\n"
  "that the system is not running on mains power.\n" 
  "\n"
  #if (CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_ANDROID) || \
      ((CLIENT_CPU == CPU_X86) && ((CLIENT_OS == OS_FREEBSD) || \
      (CLIENT_OS == OS_NETBSD) || (CLIENT_OS == DRAGONFLY)))
  "This option is ignored if power management is disabled or not configured or\n"
  "if /dev/apm cannot be opened for reading (may require superuser privileges).\n"
  #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_MACOSX) || (CLIENT_OS == OS_IOS)
  "This option is ignored if power source detection is not supported by the\n"
  "the operating system or hardware architecture.\n"
  #else
  "This option is not supported on this platform and is ignored.\n"
  #endif
  ,CONF_MENU_MISC,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_QUIETMODE               , /* CONF_MENU_MISC */
  CFGTXT("Run detached/disable all screen output ? (quiet mode)"),"no",
  /*CFGTXT(*/
  "When enabled, this option will cause the client to suppress all screen\n"
  "output and detach itself (run in the background). Because the client is\n"
  "essentially invisible, distributed.net strongly encourages the use of\n"
  "logging to file if you choose to run the client with disabled screen\n"
  "output. This option is synonymous with the -hide and -quiet command line\n"
  "switches and can be overridden with the -noquiet switch.\n"
  "\n"
  #if (CLIENT_OS == OS_DOS)
  "The distributed.net client for DOS cannot detach itself from the console.\n"
  #elif (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32)
  "A client that was started as a win32 service has no screen output.\n"
  #else
  "Clients 'installed' with the -install option, are always configured\n"
  "to start detached.\n"
  #endif
  /*)*/,CONF_MENU_MISC,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_CRUNCHMETER              , /* CONF_MENU_MISC */
  CFGTXT("Crunch-o-meter (progress indicator) style"),"-1 (auto-sense)",
  CFGTXT(
  "-1) auto-sense: Use the 'absolute style' when an OGR cruncher is active or\n"
  "                there are more than 26 crunchers (a...z), otherwise use the\n"
  "                'relative style'.\n"
  " 0) disabled:   Disable the crunch-o-meter entirely.\n"
  " 1) absolute:   Always display the current position within the packet.\n"
  "                Example: \"[...] #1: OGR-P2:24/6-20-9-19 [26,455,914,813]\"\n"
  " 2) relative:   Always display the current position as a percentage relative\n"
  "                to the total amount of work in the packet.\n"
  "                Example: \"....10....20....30....                    \"\n"
  "                Note that the total amount of work in a packet is known in\n"
  "                advance only for linear projects such as RC5, DES and CSC.\n"
  "                Forcing the 'relative style' for other projects may result\n"
  "                in a display that may be confusing or misleading.\n"
  " 3) live-rate:  Display a per-project 'live' crunch rate and, if supported,\n"
  "                an approximation of cruncher throughput.\n"
  "                Example: \"[...] OGR-P2: rate: 3,680,878 nodes/sec\"\n"
  ),CONF_MENU_MISC,CONF_TYPE_INT,NULL,NULL,-1,3,NULL,NULL
},
{ 
  CONF_COMPLETIONSOUNDON       , /* CONF_MENU_MISC */
  CFGTXT("Play sound on packet completion ?"),"no",
  CFGTXT(
  "If enabled, the client will play a sound after each packet is completed.\n"
  "\n"
  "This option requires OS and hardware support and is not supported on all\n"
  "platforms.\n"
  ),CONF_MENU_MISC,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},

/* ------------------------------------------------------------ */

{ 
  CONF_MENU_BUFF_PLACEHOLDER   , /* 15 */
  CFGTXT("Buffer and Buffer Update Options"),"",
  CFGTXT(""),
  CONF_MENU_MAIN,CONF_TYPE_MENU,NULL,NULL,CONF_MENU_BUFF,0,NULL,NULL
},
{  
  CONF_NODISK                  , /* CONF_MENU_BUFF */
  CFGTXT("Buffer in memory only ? (no disk I/O)"),"no",
   CFGTXT(
   "This option is for machines with permanent connections to a keyserver\n"
   "but without local disks.\n"
   "Note : This option will cause all buffered, unflushable work to be lost\n"
   "by a client shutdown.\n"
  ),CONF_MENU_BUFF,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_INBUFFERBASENAME        , /* CONF_MENU_BUFF */
  CFGTXT("In-Buffer Filename Prefix"), BUFFER_DEFAULT_IN_BASENAME,
  CFGTXT(
  "Enter the prefix (the base name, ie a filename without an 'extension')\n"
  "of the buffer files where unfinished work will be stored. The default\n"
  "is \"" BUFFER_DEFAULT_IN_BASENAME "\". The name of the project will be concatenated internally\n"
  "to this base name to construct the full name of the buffer file. For\n"
  "example, \"" BUFFER_DEFAULT_IN_BASENAME "\" becomes \"" BUFFER_DEFAULT_IN_BASENAME "" EXTN_SEP "r72\" for the RC5-72 input buffer.\n"
  "Note : if a path is not specified, the files will be created in the same\n"
  "directory as the .ini file, which, by default, is created in the same\n"
  "directory as the client itself.\n"
  "(A new buffer file format is forthcoming. The new format will have native\n"
  "support for First-In-First-Out packets (this functionality is currently\n"
  "available but is not efficient when used with large buffers); improved\n"
  "locking semantics; all buffers for all projects will be contained in a\n"
  "single file).\n"
  ),CONF_MENU_BUFF,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_OUTBUFFERBASENAME       , /* CONF_MENU_BUFF */
  CFGTXT("Out-Buffer Filename Prefix"), BUFFER_DEFAULT_OUT_BASENAME,
  CFGTXT(
  "Enter the prefix (the base name, ie a filename without an 'extension')\n"
  "of the buffer files where finished work will be stored. The default\n"
  "is \"" BUFFER_DEFAULT_OUT_BASENAME "\". The name of the project will be concatenated internally\n"
  "to this base name to construct the full name of the buffer file. For\n"
  "example, \"" BUFFER_DEFAULT_OUT_BASENAME "\" becomes \"" BUFFER_DEFAULT_OUT_BASENAME "" EXTN_SEP "r72\" for the RC5-72 output buffer\n"
  "Note : if a path is not specified, the files will be created in the same\n"
  "directory as the .ini file, which, by default, is created in the same\n"
  "directory as the client itself.\n"
  "(This option will eventually disappear. Refer to the \"In-Buffer Filename\n"
  "Prefix\" option for details).\n"
  ),CONF_MENU_BUFF,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_CHECKPOINT              , /* CONF_MENU_BUFF */
  CFGTXT("Checkpoint Filename"),"",
  CFGTXT(
  "This option sets the location of the checkpoint file. The checkpoint is\n"
  "where the client writes its progress to disk so that it can recover\n"
  "partially completed work if the client had previously failed to shutdown\n"
  "normally.\n"
  "DO NOT SHARE CHECKPOINT FILES BETWEEN CLIENTS. Avoid the use of checkpoint\n"
  "files unless your client is running in an environment where it might not\n"
  "be able to shutdown properly.\n"
  ),CONF_MENU_BUFF,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_OFFLINEMODE             , /* CONF_MENU_BUFF */
  CFGTXT("Disable buffer updates from/to a keyserver"),"no",
  CFGTXT(
  "Yes: The client will never connect to a keyserver.\n"
  " No: The client will connect to a keyserver as needed.\n"
  ),CONF_MENU_BUFF,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_MENU_NET_PLACEHOLDER    , /* CONF_MENU_BUFF */
  CFGTXT("Keyserver<->client connectivity options"),"",
  CFGTXT(""),CONF_MENU_BUFF,CONF_TYPE_MENU,NULL,NULL,CONF_MENU_NET,0,NULL,NULL
},
{ 
  CONF_REMOTEUPDATEDISABLED    , /* CONF_MENU_BUFF */
  CFGTXT("Disable buffer updates from/to remote buffers"),"no",
  CFGTXT(
  "Yes: The client will not use remote files.\n"
  " No: The client will use remote files if updating from a keyserver is\n"
  "     disabled or it fails or if insufficient packets were sent/received.\n"
  ),CONF_MENU_BUFF,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_REMOTEUPDATEDIR         , /* CONF_MENU_BUFF */
  CFGTXT("Remote buffer directory"),"",
  CFGTXT(
  "When a client runs out of work to do and cannot fetch more work from a\n"
  "keyserver, it will fetch/flush from/to files in this directory.\n"
  "\n"
  "This option specifies a *directory*, and not a filename. The full paths\n"
  "effectively used are constructed from the name of the project and the\n"
  "filename component in the \"[Out|In]-Buffer Filename Prefix\" options.\n"
  "For example, if the \"In-Buffer Filename Prefix\" is \"~/" BUFFER_DEFAULT_IN_BASENAME "\", and\n"
  "the alternate buffer directory is \"/there/\" then the alternate in-buffer\n"
  "file for RC5-72 becomes \"/there/" BUFFER_DEFAULT_IN_BASENAME ".r72\"\n"
  ),CONF_MENU_BUFF,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_LOADORDER               , /* CONF_MENU_BUFF "DES,OGR,RC5" */
  CFGTXT("Load-work precedence"), __default_loadorder(), 
  /* CFGTXT( */
  "The order specified here determines the order the client will look for work\n"
  "each time it needs to load a packet from a buffer.\n"
  "For example, \"OGR-P2,RC5-72,...\" instructs the client to first look for\n"
  "work for OGR-P2 and, if it doesn't find any, to try RC5-72 next, and so on.\n"
  "\n"
  "You can turn off a project by setting \":0\" or \"=0\" after the project's\n"
  "name - for instance, \"XYZ:0\" tells your client not to work on, or request\n"
  "work for, the XYZ project.\n"
  #if (defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2))   \
    && ((CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_WIN16))
  "Note : OGR is automatically disabled for non-preemptive operating\n"
  "environments running on low(er)-end hardware. For details, see\n"
  "http://www.distributed.net/faq/cache/188.html\n"
  #endif
  #if 0
  "\n"
  "Projects not found in the list you enter here will be inserted in their\n"
  "default position.\n"
  #endif
  "\n"
  "It is possible to have the client rotate through this list, updating its\n"
  "buffers only once for each pass. To do so, 'Dialup-link detection'\n"
  "and '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "' must be disabled since a buffer\n"
  "update (new work being made available) would otherwise cause the client\n"
  "to go back to the beginning of the load order.\n"
  /*) */,CONF_MENU_BUFF,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_FREQUENT                , /* CONF_MENU_BUFF */
  CFGTXT(ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME),"4 (update on empty in-buffer)",
  /*CFGTXT(*/
  "The following options are extensions to normal threshold management and are\n"
  "not usually necessary:\n"
  "   0) no additional buffer-level checking.\n"
  "   1) fetch/flush all buffers if any in-buffer is not full.\n"
/*"   1) ensure that there is always work available.\n"*/
  "   2) fetch/flush all buffers if any out-buffer is not empty.\n"
/*"   2) ensure that all completed work is kept flushed.\n" */
  "   3) both 1) and 2). (implied if 'Dialup detection options' are enabled)\n"
  "   4) fetch/flush all buffers if any in-buffer is empty. (default)\n"
/*"   4) update on per-project buffer exhaustion.\n" */
  "\n"
  "Options 1, 2 and 3 will cause the client to frequently check buffers levels.\n"
  "(Frequency/interval is determined by the 'Buffer-level check interval' option)\n"
  "You might want to use them if you have a single computer with a network\n"
  "connection \"feeding\" other clients via a common set of buffers, or if you\n"
  "want to ensure that completed work is flushed immediately.\n"
  "Option 4 is a hint to the client to work on a single project as long as\n"
  "possible (updating per-project buffers individually), rather than loop\n"
  "through all active/enabled projects (one combined update per pass).\n"
  /*)*/,CONF_MENU_BUFF,CONF_TYPE_INT,NULL,NULL,0,4,NULL,NULL
},
{  
  CONF_FREQUENT_FREQUENCY      , /* CONF_MENU_BUFF */
  CFGTXT("Buffer-level check interval"), "0:00 (on buffer change)",
  /*CFGTXT(*/
  "This option determines how often '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'\n"
  "should be performed. (More precisely: how much time must elapse between\n"
  "buffer-level checks)\n" 
  "\n"
  "This setting is meaningful only if one of the extensions to normal threshold\n"
  "management is enabled: either implicitly when 'Dialup detection options' are\n"
  "active or explicitly with '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'.\n"
  "\n"
  "The interval specified here is in hours and minutes, and the default denotes\n"
  "that the client should check buffer-levels whenever it detects a change (by\n"
  "any client) to a buffer file, but not more often than twice per minute.\n"
  /*)*/,CONF_MENU_BUFF,CONF_TYPE_TIMESTR,NULL,NULL,0,0,NULL,NULL
},   
{  
  CONF_FREQUENT_RETRY_FREQUENCY      , /* CONF_MENU_BUFF */
  CFGTXT("Buffer-level check retry interval"), "0:00 (no delay)",
  /*CFGTXT(*/
  "This option determines how often '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'\n"
  "should be retried after failure. (More precisely: how much time must elapse\n"
  "between buffer-level check retries)\n" 
  "\n"
  "This setting is meaningful only if one of the extensions to normal threshold\n"
  "management is enabled: either implicitly when 'Dialup detection options' are\n"
  "active or explicitly with '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'.\n"
  "\n"
  "The interval specified here is in hours and minutes, and the default denotes\n"
  "that the client should retry the buffer-level checks at most twice per minute\n"
  "until it succeeds.\n"
  /*)*/,CONF_MENU_BUFF,CONF_TYPE_TIMESTR,NULL,NULL,0,0,NULL,NULL
},   
{
  CONF_PREFERREDBLOCKSIZE      , /* CONF_MENU_BUFF */
  CFGTXT("Preferred packet size (X*2^32 keys/packet)"), "-1 (auto)",
  /* CFGTXT( */
  "When fetching key-based packets from a server, the client will request\n"
  "packets with the size you specify in this option.\n"
  "The minimum and maximum packet sizes are " _TEXTIFY(PREFERREDBLOCKSIZE_MIN) " and " _TEXTIFY(PREFERREDBLOCKSIZE_MAX) " respectively,\n"
  "and specifying '-1' permits the client to use internal defaults.\n"
  "Note : the number you specify is the *preferred* size. Although the\n"
  "keyserver will do its best to serve that size, there is no guarantee that\n"
  "it will always do so.\n"
  /*)*/,CONF_MENU_BUFF,CONF_TYPE_IARRAY,NULL,NULL,PREFERREDBLOCKSIZE_MIN,PREFERREDBLOCKSIZE_MAX,NULL,NULL
},
{ 
  CONF_THRESHOLDI              , /* CONF_MENU_BUFF */
  CFGTXT("Fetch work threshold"), "0 (default size or determine from time threshold)",
  "This option specifies how many stats units your client will buffer between\n"
  "communications with a keyserver. When the number of stats units in the\n"
  "input buffer reaches 0, the client will attempt to connect to a keyserver,\n"
  "fill the input buffer to the threshold, and send in all completed work.\n"
  "Keep the number to buffer low if you have a fixed connection to the\n"
  "Internet, or the cost of your dialup connection is negligible. While you\n"
  "could theoretically enter any number in the fields here, the client has\n"
  "internal limits on the number of packets that it can safely deal with.\n"
#if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
  "\n"
  "The number of stats units per packet is assumed to be 1 when the number of\n"
  "stats units cannot be determined before the packet is completed (ie for OGR).\n"
#endif
  "\n"
  "A value of 0 for the 'fetch setting' indicates that a time threshold\n"
  "should be used instead. If that too is unspecified, then the client will\n"
  "use defaults.\n"
  "\n"
  "* See also: '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'\n"
  ,CONF_MENU_BUFF,CONF_TYPE_IARRAY,NULL,NULL,1,0xffff,NULL,NULL
},
{ 
  CONF_THRESHOLDT              , /* CONF_MENU_BUFF */
  CFGTXT("Fetch time threshold (in hours)"), "0 (use work threshold)",
  "This option specifies that instead of fetching a specific number of\n"
  "stats units from the keyservers, enough work should be downloaded\n"
  "to keep your client busy for a specified number of hours. This causes\n"
  "the stats unit threshold option to be constantly recalculated based on\n"
  "the current crunch rate.\n\n"
  "For fixed (static) connections, you should set this to a low value, eg\n"
  "three to six hours. For dialup connections, set this to a value high\n"
  "enough to ensure that the client will not prematurely run out of work.\n"
#if defined(HAVE_OGR_CORES) || defined(HAVE_OGR_PASS2)
  "\n"
  "Currently not implemented for OGR because the amount of work in an\n"
  "unprocessed packet cannot be predicted.\n"
#endif
  "\n"
  "* See also: '" ADDITIONAL_BUFFLEVEL_CHECK_OPTION_NAME "'\n"
  ,CONF_MENU_BUFF,CONF_TYPE_IARRAY,NULL,NULL,0,(14*24),NULL,NULL
},

/* ------------------------------------------------------------ */

{ 
  CONF_MENU_PERF_PLACEHOLDER   , /* 28 */
  CFGTXT("Performance related options"),"",
  CFGTXT(""),
  CONF_MENU_MAIN,CONF_TYPE_MENU,NULL,NULL,CONF_MENU_PERF,0,NULL,NULL
},
{ 
  CONF_CPUTYPE                 , /* CONF_MENU_PERF */
  CFGTXT("Core selection"), "-1 (auto-detect)",
  CFGTXT(
  "This option determines core selection. Auto-select is usually best since\n"
  "it allows the client to pick other cores as they become available. Please\n"
  "let distributed.net know if you find the client auto-selecting a core that\n"
  "manual benchmarking shows to be less than optimal.\n"
  "Cores marked as 'n/a' are not applicable to your particular cpu/os.\n"
  ),CONF_MENU_PERF,CONF_TYPE_IARRAY,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_NUMCPU                  , /* CONF_MENU_PERF 0 ... */
  CFGTXT("Number of crunchers to run simultaneously"), "-1 (auto-detect)",
  /* CFGTXT( */
  "This option specifies the number of threads you want the client to work on.\n"
  "On multi-processor machines this should be set to the number of processors\n"
  "available or to -1 to have the client attempt to auto-detect the number of\n"
  "processors. Multi-threaded clients can be forced to run single-threaded by\n"
  "setting this option to zero.\n"
#if (CLIENT_OS == OS_RISCOS) && defined(HAVE_X86_CARD_SUPPORT)
  "Under RISC OS, processor 1 is the ARM, and processor 2 is an x86 processor\n"
  "card, if fitted.\n"
#endif
  /*) */,CONF_MENU_PERF,CONF_TYPE_INT,NULL,NULL,-1,128,NULL,NULL
},
{ 
  CONF_DEVICENUM               , /* CONF_MENU_PERF */
  CFGTXT("Run on the selected device number only"), "-1 (run on any device)",
  /* CFGTXT( */
  "This option specifies a device number for the client to work on.\n"
  "On multi-processor machines this can be set to a specific processor device\n"
  "to run on.\n"
  "Set to -1 to have the client run on any device.\n"
  "Note: if this parameter is set to a specific device, the client is forced\n"
  "to run with one thread.\n"
#if !(CLIENT_CPU == CPU_CUDA || CLIENT_CPU == CPU_ATI_STREAM || CLIENT_CPU == CPU_OPENCL)
  "This option is not currently supported on this platform and is ignored.\n"
#endif
  /*) */,CONF_MENU_PERF,CONF_TYPE_INT,NULL,NULL,-1,128,NULL,NULL
},
{ 
  CONF_NICENESS                , /* CONF_MENU_PERF priority */
  CFGTXT("Priority level to run at"), "0 (lowest/at-idle)",
  /* CFGTXT( */
#if (CLIENT_OS == OS_RISCOS)
  "The priority option is ignored on this machine. The distributed.net client\n"
  "for "CLIENT_OS_NAME_EXTENDED" dynamically adjusts its process priority.\n"
#elif (CLIENT_OS == OS_WIN16) //|| (CLIENT_OS==OS_WIN32)
  "The priority option is ignored on this machine. distributed.net clients\n"
  "for "CLIENT_OS_NAME_EXTENDED" always run at lowest ('idle') priority.\n"
#elif (CLIENT_OS == OS_NETWARE)
  "The priority option for the distributed.net client for NetWare is directly\n"
  "proportionate to the rate at which the client yields.\n"
  "It is a really a matter of experimentation to find the best \"priority\" for\n"
  "your machine, and while \"priority\" zero will probably give you a\n"
  "less-than-ideal crunch rate, it will never be \"wrong\".\n\n"   
  "If you do decide to change this setting, carefully monitor the server's\n"
  "responsiveness under heavy load.\n"
#elif (CLIENT_OS == OS_MACOSX)
  "The higher the client's priority, the greater will be its demand for\n"
  "processor time. The operating system will fulfill this demand only after\n"
  "the demands of other processes with a higher or equal priority are fulfilled\n"
  "first. At priority zero, the client will get less processing time than all\n"
  "other processes. At priority nine, the client will have a better chance to\n"
  "get most of the CPU time unless there is a time-critical process waiting to\n"
  "be run.\n"
  "Besides, a zero priority does not mean that the client will run slower than\n"
  "at a higher priority. It simply means that all other processes have a better\n"
  "chance to get processor time than client. If none of them want/need processor\n"
  "time, the client will get it.\n"
#else
  "The higher the client's priority, the greater will be its demand for\n"
  "processor time. The operating system will fulfill this demand only after\n"
  "the demands of other processes with a higher or equal priority are fulfilled\n"
  "first. At priority zero, the client will get processing time only when all\n"
  "other processes are idle (give up their chance to run). At priority nine, the\n"
  "client will always get CPU time unless there is a time-critical process\n"
  "waiting to be run - this is obviously not a good idea unless the client is\n"
  "running on a machine that does nothing else.\n"
  #if defined(__unix__)
  "On *nix'ish OSs, the higher the priority, the less nice(1) the process.\n"
  #elif (CLIENT_OS == OS_WIN32)
  "*Warning*: Running the Win32 client at any priority level other than zero is\n"
  "destructive to Operating System safety. Win32's thread scheduler is not nice.\n"
  "Besides, a zero priority does not mean that the client will run slower than at\n"
  "a higher priority. It simply means that all other processes have a better\n"
  "*chance* to get processor time than client. If none of them want/need processor\n"
  "time, the client will get it. Do *not* change the value of this option unless\n"
  "you are intimately familiar with the way Win32 thread scheduling works.\n"
  #endif
#endif
  /*)*/,CONF_MENU_PERF, CONF_TYPE_INT, NULL, NULL, 0, 9, NULL,NULL 
},

/* ------------------------------------------------------------ */

{ 
  CONF_MENU_LOG_PLACEHOLDER    , /* 32 */
  CFGTXT("Logging Options"),"",
  CFGTXT(""),
  CONF_MENU_MAIN,CONF_TYPE_MENU,NULL,NULL,CONF_MENU_LOG,0,NULL,NULL
},
{ 
  CONF_LOGTYPE                 , /* CONF_MENU_LOG */
  CFGTXT("Log file type"), "0",
  CFGTXT(
  "This option determines what kind of file-based logging is preferred :\n"
  "\n"
  "0) none      altogether disables logging to file.\n"
  "1) no limit  the size of the file is not limited. This is the default if a\n"
  "             limit is not specified in the \"Log file limit\" option.\n"
  "2) restart   the log will be deleted/recreated when the file size specified\n"
  "             in the \"Log file limit\" option is reached.\n"
  "3) fifo      the oldest lines in the file will be discarded when the size of\n"
  "             the file exceeds the limit in the \"Log file limit\" option.\n"
  "4) rotate    a new file will be created when the rotation interval specified\n"
  "             in the \"Log file limit\" option is exceeded.\n"
  ),CONF_MENU_LOG,CONF_TYPE_INT,NULL,NULL /*logtypes[]*/,0,0,NULL,NULL
},
{ 
  CONF_LOGNAME                 , /* CONF_MENU_LOG */
  CFGTXT("File to log to"), "",
  CFGTXT(
  "The log file name is required for all log types except \"rotate\", for which\n"
  "it is optional.\n"
  "\n"
  "The effective file name used for the \"rotate\" log file type is constructed\n"
  "from a unique identifier for the period (time limit) concatenated to\n"
  "whatever you specify here.\n"
  "\n"
  "If the rotate interval was specified as a number of days, then the name of\n"
  "the log file used will be [file_or_dir_to_log_to]YYMMDD" EXTN_SEP "log, where YYMMDD\n"
  "is the date of the first day of that 'interval'. If the interval were weekly,\n"
  "the name of the log file used will be [file_or_dir_to_log_to]yearweek" EXTN_SEP "log\n"
  ),CONF_MENU_LOG,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_LOGLIMIT                , /* CONF_MENU_LOG */
  CFGTXT("Log file limit/interval"), "",
  CFGTXT(
  "For the \"rotate\" log type, this option determines the interval with which\n"
  "a new file will be opened. The interval may be specified as a number of\n"
  "days, or as \"daily\",\"weekly\",\"monthly\" etc.\n"
  "For other log types, this option determines the maximum file size in\n"
  "kilobytes. The \"fifo\" log type will enforce a minimum of 100kB to avoid\n"
  "excessive file I/O.\n"
  ),CONF_MENU_LOG,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{
  CONF_LOGROTATETIME           , /* CONF_MENU_LOG */
  CFGTXT("Rotate logs at 0:00 UTC"), "no",
  CFGTXT(
  "For the \"rotate\" log type, this option determines whether the logs shall\n"
  "be rotated at 0:00 local time or 0:00 UTC.\n"
  ),CONF_MENU_LOG,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_MESSAGELEN              , /* CONF_MENU_LOG */
  CFGTXT("Log by mail spool size (bytes)"), "0 (mail disabled)",
  CFGTXT(
  "The client is capable of sending you a log of the client's progress by mail.\n"
  "To activate this capability, specify how much you want the client to buffer\n"
  "before sending. The minimum is 2048 bytes, the maximum is approximately\n"
  "125000 bytes. Specify 0 (zero) to disable logging by mail.\n"
  ),CONF_MENU_LOG,CONF_TYPE_INT,NULL,NULL,0,125000,NULL,NULL
},
{ 
  CONF_SMTPSRVR                , /* CONF_MENU_LOG */
  CFGTXT("SMTP server:port"), "",
  /* CFGTXT( */
  "Specify the name or DNS address of the SMTP host via which the client should\n"
  "relay mail logs. For example : \"mercury.pegasus.org:25\"\n"
  "\n"
  "The default hostname is the hostname component of the email address\n"
  "specified in the \"E-mail address that logs will be mailed from\" option.\n"
  "\n"
  #ifdef HAVE_IPV6
  "Literal IPv6 addresses must be enclosed in \"[\" and \"]\". For example :\n"
  "\"[1080::8:800:200C:417A]:1234\".\n"
  "\n"
  #endif
  "The default port specifier is 25.\n"
  /* ) */,CONF_MENU_LOG,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL},
{ 
  CONF_SMTPFROM                , /* CONF_MENU_LOG */
  CFGTXT("E-mail address that logs will be mailed from"),
  "" /* *((const char *)(options[CONF_ID].thevariable)) */,
  CFGTXT(
  "This setting determines what sender address to use for mailing logs.\n"
  "Some servers require this to be a local address.\n"
  "\n"
  "The default is the email address specified in the 'distributed.net ID'\n"
  "option.\n"
  ),CONF_MENU_LOG,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_SMTPDEST                , /* CONF_MENU_LOG */
  CFGTXT("E-mail address to send logs to"),
  "" /* *((const char *)(options[CONF_ID].thevariable)) */,
  CFGTXT(
  "Full name and site eg: you@your.site. Comma delimited list permitted.\n"
  "\n"
  "The default is to send logs to the address specified as the\n"
  "'distributed.net ID'\n"
  ),CONF_MENU_LOG,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},

/* ------------------------------------------------------------ */

{ 
  CONF_NETTIMEOUT              , /* CONF_MENU_NET */
  CFGTXT("Network Timeout (seconds)"), "60 (default)",
  CFGTXT(
  "This option determines the amount of time the client will wait for a network\n"
  "read or write acknowledgement before it assumes that the connection has been\n"
  "broken. Any value between 5 and 300 seconds is valid and setting the timeout\n"
  "to -1 forces a blocking connection.\n"
  ),CONF_MENU_NET,CONF_TYPE_INT,NULL,NULL,-1,300,NULL,NULL
},
{ 
  CONF_AUTOFINDKS              , /* CONF_MENU_NET */
  CFGTXT("Automatically select a distributed.net keyserver ?"), "yes",
  CFGTXT(
  "Set this option to 'Yes' UNLESS your client will be communicating with\n"
  "a personal proxy (instead of one of the main distributed.net keyservers)\n"
  "OR your client will be connecting through an HTTP proxy (firewall) and\n"
  "you have been explicitly advised by distributed.net staff to use a\n"
  "specific IP address.\n"
  ),CONF_MENU_NET,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_KEYSERVNAME             , /* CONF_MENU_NET */
  CFGTXT("Keyserver host name(s)"), "",
  /* CFGTXT( */
  "This is the name(s) or IP address(s) of the machine(s) that your client\n"
  "will obtain keys from and send completed packets to. Avoid IP addresses\n"
  "unless the client has trouble resolving names to addresses.\n"
  "\n"
  "By default, the client will select a distributed.net keyserver in the\n"
  "client's approximate geographic vicinity.\n"
  "\n"
  "Multiple names/addresses may be specified (separated by commas or semi-\n"
  "colon), and may include a port number override. For example :\n"
  "\"keyserv.hellsbells.org, join.the.dots.de:1234\".\n"
  "\n"
  #ifdef HAVE_IPV6
  "Literal IPv6 addresses must be enclosed in \"[\" and \"]\". For example :\n"
  "\"[1080::8:800:200C:417A]:1234\".\n"
  "\n"
  #endif
  "Host names/addresses without port numbers will inherit the port number\n"
  "from the \"Keyserver port\" option.\n"
  /* ) */,CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_KEYSERVPORT             , /* CONF_MENU_NET */
  CFGTXT("Keyserver port"), "", /* atoi("") is zero too. */
  CFGTXT(
  "This field determines the default keyserver port if the client is to\n"
  "select a keyserver automatically, or when no keyserver port was\n"
  "explicitely specified for a host in the \"Keyserver host name(s)\" list\n"
  "(in which case hosts without keyserver port numbers will be appended\n"
  "with the value you specify here).\n"
  "\n"
  "The keyserver port should be left at zero (default) unless :\n"
  "a) You are connecting to a personal proxy is that is *not* listening on\n"
  "   port 2064.\n"
  "b) You are connecting to a keyserver (regardless of type: personal proxy\n"
  "   or distributed.net host) through a firewall, and the firewall does\n"
  "   *not* permit connections to port 2064.\n"
  "\n"
  "The default port number is 80 when using HTTP encoding, and 23 when using\n"
  "UUE encoding, and 2064 otherwise. All keyservers (personal proxy as well\n"
  "as distributed.net hosts) accept all encoding methods (UUE, HTTP, raw) on\n"
  "any/all ports the listen on.\n"
  ),CONF_MENU_NET,CONF_TYPE_INT,NULL,NULL,0,0xFFFF,NULL,NULL
},
{ 
  CONF_NOFALLBACK              , /* CONF_MENU_NET */
  CFGTXT("Disable fallback to a distributed.net keyserver ?"),"no",
  CFGTXT(
  "If the keyserver that your client will be connecting to is a personal\n"
  "proxy inside a protected LAN (inside a firewall), set this option to 'yes'.\n"
  "Otherwise leave it at 'No'.\n"
  "\n"
  "(This option controls whether the client will 'fall-back' to an official\n"
  "distributed.net keyserver after connection failures to the server address\n"
  "you have manually specified.)\n"
  ),CONF_MENU_NET,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_FWALLTYPE               , /* CONF_MENU_NET */
  CFGTXT("Firewall/proxy protocol"), "none/transparent/mapped" /* note: atol("")==0 */,
  CFGTXT(
  "This field determines what protocol to use when communicating via a\n"
  "SOCKS or HTTP proxy.\n"
  ),CONF_MENU_NET,CONF_TYPE_INT,NULL,NULL,0,20,NULL,NULL
},
{ 
  CONF_FWALLHOSTNAME           , /* CONF_MENU_NET */
  CFGTXT("Firewall hostname:port"), "",
  /* CFGTXT( */
  "This field determines the host name or IP address of the firewall proxy\n"
  "through which the client should communicate. The proxy is expected to be\n"
  "on a local network. For example: \"socks.proxy.my:1080\"\n"
  "\n"
  #ifdef HAVE_IPV6
  "Literal IPv6 addresses must be enclosed in \"[\" and \"]\". For example :\n"
  "\"[1080::8:800:200C:417A]:1080\".\n"
  "\n"
  #endif
  "The port specifier defaults to 1080 for SOCKS and 8080 for HTTP.\n"
  /* ) */,CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_FWALLUSERNAME           , /* CONF_MENU_NET */
  CFGTXT("Firewall user name"), "",
  CFGTXT(
  "Specify a user name in this field if your SOCKS host requires\n"
  "authentication before permitting communication through it.\n"
  ),CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_FWALLPASSWORD           , /* CONF_MENU_NET */
  CFGTXT("Firewall password"), "",
  CFGTXT(
  "Specify the password in this field if your SOCKS host requires\n"
  "authentication before permitting communication through it.\n"
  ),CONF_MENU_NET,CONF_TYPE_PASSWORD,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_FORCEHTTP               , /* CONF_MENU_NET */
  CFGTXT("Use HTTP encapsulation even if not using an HTTP proxy ?"),"no",
  CFGTXT(
  "Enable this option if you have an HTTP port-mapped proxy or other\n"
  "configuration that allows HTTP packets but not unencoded packets.\n"
  ),CONF_MENU_NET,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_FORCEUUE                , /* CONF_MENU_NET */
  CFGTXT("Always use UUEncoding ?"),"no",
  CFGTXT(
  "Enable this option if your network environment only supports 7bit traffic.\n"
  ),CONF_MENU_NET,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_LURKMODE                , /* CONF_MENU_NET */
  CFGTXT("Dialup-link detection"),"0",
  CFGTXT(
  "0) Off:  the client will send/receive packets only when it needs to do so.\n"
  "1) Dial-up detection mode : This acts like mode 0, with the addition\n"
  "         that the client will automatically send/receive packets when\n"
  "         a dial-up networking connection is established. Modem users\n"
  "         will probably wish to use this option so that their client\n"
  "         never runs out of packets.\n"
  "2) Dial-up detection ONLY mode : Like the previous mode, this will cause\n"
  "         the client to automatically send/receive packets when connected.\n"
  "         HOWEVER, if the client runs out of packets, it will NOT trigger\n"
  "         auto-dial, and will instead work on random RC5-72 packets until\n"
  "         a connection is detected, or quit if the RC5-72 contest is\n"
  "         disabled or closed.\n"
  ),CONF_MENU_NET,CONF_TYPE_INT,NULL,&lurkmodetable[0],0,2,NULL,NULL
},
{ 
  CONF_CONNIFACEMASK           , /* CONF_MENU_NET */
  CFGTXT("Interfaces to watch"), "",
  /* CFGTXT( */
  "Colon-separated list of interface names to monitor for a connection.\n"
  "For example : \"ppp0:ppp1:eth1\". Wild cards are permitted, ie \"ppp*\".\n"
  "a) An empty list implies all interfaces that are identifiable as dialup,\n"
  "   ie \"ppp*:sl*:...\" (dialup interface names vary from platform to\n"
  "   platform. FreeBSD for example, also includes 'tun*' interfaces. Win32\n"
  "   emulates SLIP as a subset of PPP: sl* interfaces are seen as ppp*).\n"
  "b) if you have an intermittent ethernet connection to the Internet, put\n"
  "   the corresponding interface name in this list, typically 'eth0'\n"
  "c) To include all interfaces, set this option to '*'.\n"
  #if (CLIENT_OS == OS_WIN32)
  "** All Win32 network adapters (regardless of medium; dialup/non-dialup)\n"
  "   also have a unique unchanging alias: 'lan0', 'lan1' and so on. Both\n"
  "   naming conventions can be used simultaneously.\n"
  "** a list of interfaces can be obtained from ipconfig.exe or winipcfg.exe\n"
  "   The first dialup interface is ppp0, the second is ppp1 and so on, while\n"
  "   the first non-dialup interface is eth0, the second is eth1 and so on.\n"
  "** RAS profiles may be specified here too (note: names are case sensitive)\n"
  #else
  "** The command line equivalent of this option is -interfaces\n"
  #endif
  /* ) */,CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{
  CONF_DIALWHENNEEDED          , /* CONF_MENU_NET */
   #if (CLIENT_OS == OS_WIN32)
   CFGTXT("Use a specific DUN profile to connect with ?"),
   #elif ((CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32))
   CFGTXT("Load/unload Winsock to initiate/hang-up net connections ?"),
   #elif (CLIENT_OS == OS_AMIGAOS)
   CFGTXT("Automatically go online via Miami, MiamiDx or Genesis ?"),
   #elif (CLIENT_OS == OS_MORPHOS)
   CFGTXT("Automatically go online via Genesis ?"),
   #else
   CFGTXT("Use scripts to initiate/hang-up dialup connections ?"),
   #endif
   "no",CFGTXT(
   "Select 'yes' to have the client control how network connections and\n"
   "initiated if none is active.\n"
   ),CONF_MENU_NET,CONF_TYPE_BOOL,NULL,NULL,0,1,NULL,NULL
},
{ 
  CONF_CONNPROFILE             , /* CONF_MENU_NET */
  #if (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  CFGTXT("Dial-up Interface Name"),
  #else
  CFGTXT("Dial-up Connection Profile"),
  #endif
  "",
  #if (CLIENT_OS == OS_WIN32)
  CFGTXT(
  "Select the DUN profile to use when it becomes necessary to dial.\n"
  "\n"
  "Note that the name you specify here is used to select the profile to use\n"
  "when *dialing*. It is not used for masking the active connections in the\n"
  "way \"Interfaces to watch\" is used.\n"
  )
  #elif (CLIENT_OS == OS_AMIGAOS)
  CFGTXT(
  "Select which interface name to put online, when dial-up access is necessary.\n"
  "This may be the actual interface name, or the alias that you have assigned\n"
  "to it. This option is only relevant to MiamiDx and Genesis users - Miami\n"
  "users should leave this setting blank.\n\n"
  "If you specify no name, the default interface will be used.\n")
  #elif (CLIENT_OS == OS_MORPHOS)
  CFGTXT(
  "Select which interface name to put online, when dial-up access is necessary.\n"
  "This may be the actual interface name, or the alias that you have assigned\n"
  "to it. This option is only relevant to Genesis users.\n\n"
  "If you specify no name, the default interface will be used.\n")
  #else
  CFGTXT("")
  #endif
  ,CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_CONNSTARTCMD            , /* CONF_MENU_NET */
  CFGTXT("Command/script to start dialup"),"",
  CFGTXT(
  "Enter any valid shell command or script name to use to initiate a network\n"
  "connection. \"Dial the Internet as needed?\" must be enabled for this option\n"
  "to be of any use.\n"
  ),CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
},
{ 
  CONF_CONNSTOPCMD             , /* CONF_MENU_NET */
  CFGTXT("Command/script to stop dialup"),"",
  CFGTXT(
  "Enter any valid shell command or script name to use to shutdown a network\n"
  "connection previously initiated with the script/command specified in the\n"
  "\"Command/script to start dialup\" option.\n"
  ),CONF_MENU_NET,CONF_TYPE_ASCIIZ,NULL,NULL,0,0,NULL,NULL
}
/* ================================================================== */

};
