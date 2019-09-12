/*
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: amSupport.c,v 1.8 2008/12/30 20:58:44 andreasb Exp $
 *
 * Created by Oliver Roberts <oliver@futaura.co.uk>
 *
 * ----------------------------------------------------------------------
 * This file contains general Amiga specific support code, including
 * startup initializations.
 * ----------------------------------------------------------------------
*/

/*
** Bump up the priority of the 68k mirror to 2?  Really not required anymore
** since we are running threaded (the cruncher task will always be bumped to
** pri 3), although it's enabled for PowerUp (stability problems otherwise)
*/
#ifdef __POWERUP__
#define CHANGE_MIRROR_TASK_PRI
#endif

#include "amiga.h"
#include "modereq.h"

#ifdef __OS3PPC__
#pragma pack(2)
#endif

#if __MORPHOS__
#define USE_RESETHANDLER 1
#endif

#if USE_RESETHANDLER
#include <exec/io.h>
#include <devices/keyboard.h>
#include <devices/timer.h>
#endif


#include <workbench/startup.h>
#include <proto/locale.h>
#include <proto/timer.h>
#ifndef NO_GUI
#include "proto/dnetcgui.h"
#endif

#ifdef __OS3PPC__
#pragma pack()
#endif

#ifndef __OS3PPC__

#if defined(__amigaos4__)
const char *__stack_string = "$STACK:200000";
#elif defined(__MORPHOS__)
unsigned long __stack = 200000L;
#else
unsigned long __stack = 65536L;
#endif
static struct MsgPort *TriggerPort;

#else

#ifdef CHANGE_MIRROR_TASK_PRI
static LONG Old68kMirrorPri; /* Used to restore shell priority on exit */
#endif
#ifdef __POWERUP__
struct Library *PPCLibBase;
static void *TriggerPort;
#else
static struct MsgPortPPC *TriggerPort;
#endif

#endif

struct TriggerMessage
{
   struct Message tm_ExecMsg;
   ULONG          tm_TriggerType;
};

extern BOOL GlobalTimerInit(VOID);
extern VOID GlobalTimerDeinit(VOID);
extern VOID CloseTimer(VOID);
struct Library *OpenTimer(BOOL isthread);
extern struct Library *SocketBase;
#ifdef __amigaos4__
extern struct Interface *ISocket;
#endif

#ifndef __MORPHOS__
const char *amigaGetOSVersion(void)
{
   static const char *osver[] = { "2.0","2.0x","2.1","3.0","3.1","3.2","3.3","3.4","3.5","3.9","3.9","3.9","3.9","3.9","4.0pre","4.0pre","4.0","4.1" };
   int ver = SysBase->LibNode.lib_Version;
   #ifndef __amigaos4__
   if (ver >= 40 && ver < 50) {   // Detect OS 3.5/3.9
      struct Library *VersionBase;
      if ((VersionBase = OpenLibrary("version.library",0))) {
         ver = VersionBase->lib_Version;
         CloseLibrary(VersionBase);
      }
   }
   #endif
   #ifdef __POWERUP__
   if (FindResident("MorphOS") && ver > 45) ver = 45;
   #endif
   if (ver < 36) ver = 36;
   else if (ver > 53) ver = 53;
   return osver[ver-36];
}
#endif

#if USE_RESETHANDLER

struct rhdata
{
  struct ExecBase   *SysBase;
  struct Task       *Self;
  struct MsgPort    *msgport;
  struct IOStdReq   *ioreq;
  struct timerequest timereq;
  struct Interrupt   is;
  int                handleradded;
  int                rebooting;
  int                timing;
};

static struct rhdata _rhdata, *rhdata = &_rhdata;


static void native_resethandler(struct rhdata *rhdata)
{
  struct ExecBase *SysBase = rhdata->SysBase;

  /* Indicate we're rebooting... */
  rhdata->rebooting = TRUE;

  /* Ask ourself to terminate. */
  Signal(rhdata->Self, SIGBREAKF_CTRL_C);
}

#ifdef __MORPHOS__
static void gate_resethandler(void)
{
  struct rhdata *rhdata = (struct rhdata *) REG_A1;
  native_resethandler(rhdata);
}
static struct EmulLibEntry rhgate  = {TRAP_LIBNR, 0, gate_resethandler};
#define resethandler_code  &rhgate

#else

#error "TODO: You must implement resethandler_code for your OS"

#endif

static void rem_resethandler(void);

static int add_resethandler(void)
{
  int ok = FALSE;

  bzero(rhdata, sizeof(*rhdata));
  rhdata->SysBase = SysBase;
  rhdata->Self    = FindTask(NULL);
  rhdata->msgport = CreateMsgPort();
  if (rhdata->msgport)
  {
    rhdata->ioreq = (struct IOStdReq *) CreateIORequest(rhdata->msgport, sizeof(struct IOStdReq));
    if (rhdata->ioreq)
    {
      if (OpenDevice("keyboard.device", 0, (struct IORequest *) rhdata->ioreq, 0) == 0)
      {
        rhdata->is.is_Node.ln_Type = NT_INTERRUPT;
        rhdata->is.is_Node.ln_Pri  = 64;
        rhdata->is.is_Node.ln_Name = "distributed.net client";
        rhdata->is.is_Data         = (APTR) rhdata;
        rhdata->is.is_Code         = (void (*)(void)) resethandler_code;

        rhdata->ioreq->io_Command = KBD_ADDRESETHANDLER;
        rhdata->ioreq->io_Data    = (APTR) &rhdata->is;
        DoIO((struct IORequest *) rhdata->ioreq);

        rhdata->handleradded = TRUE;

        ok = TRUE;
      }
    }
  }

  if (!ok)
  {
    rem_resethandler();
  }

  return ok;
}

static void rem_resethandler(void)
{
  if (rhdata->ioreq)
  {
    if (rhdata->handleradded)
    {
      if (rhdata->rebooting)
      {
        rhdata->timereq.tr_node.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
        rhdata->timereq.tr_node.io_Message.mn_Node.ln_Pri  = 0;
        rhdata->timereq.tr_node.io_Message.mn_ReplyPort    = rhdata->msgport;
        rhdata->timereq.tr_node.io_Message.mn_Length       = sizeof(rhdata->timereq);

        if (OpenDevice(TIMERNAME, UNIT_VBLANK, &rhdata->timereq.tr_node, 0) == 0)
        {
          rhdata->timereq.tr_node.io_Command = TR_ADDREQUEST;
          rhdata->timereq.tr_time.tv_secs    = 3;
          rhdata->timereq.tr_time.tv_micro   = 0;

          SendIO(&rhdata->timereq.tr_node);
          rhdata->timing = TRUE;
        }

        return;
      }

      rhdata->handleradded = FALSE;

      rhdata->ioreq->io_Command = KBD_REMRESETHANDLER;
      rhdata->ioreq->io_Data    = (APTR) &rhdata->is;
      DoIO((struct IORequest *) rhdata->ioreq);

      CloseDevice((struct IORequest *) rhdata->ioreq);
    }

    DeleteIORequest((struct IORequest *) rhdata->ioreq); rhdata->ioreq = NULL;
  }

  if (rhdata->msgport)
  {
    DeleteMsgPort(rhdata->msgport); rhdata->msgport = NULL;
  }
}

static void finish_resethandler(void)
{
  if (rhdata->rebooting)
  {
    if (rhdata->timing)
    {
      /* Wait until 3 seconds has passed allowing disk buffers to flush. */
      WaitIO(&rhdata->timereq.tr_node);
      CloseDevice(&rhdata->timereq.tr_node);
    }

    /* Tell system it's ok to reboot now. */
    rhdata->ioreq->io_Command = KBD_RESETHANDLERDONE;
    rhdata->ioreq->io_Data    = (APTR) &rhdata->is;
    DoIO((struct IORequest *) rhdata->ioreq);

    rhdata->rebooting = FALSE;
    rem_resethandler();
  }
}


#endif

int amigaInit(int *argc, char **argv[])
{
   int done = TRUE;

   #if !defined(__POWERUP__) && !defined(__amigaos4__) && !defined(__MORPHOS__)
   if (!MemInit()) return FALSE;
   #endif

   #ifdef __OS3PPC__
   /*
   ** PPC
   */

   /* Set the priority of the PPC 68k mirror main task */
   #ifdef CHANGE_MIRROR_TASK_PRI
   Old68kMirrorPri = SetTaskPri(FindTask(NULL), 2);
   #endif

   #ifdef __POWERUP__
   /*
   ** PowerUp
   */
   /* Requires ppc.library 46.30 (or PPCLibEmu), due to timer/signal bugs */
   #define PPCINFOTAG_EMULATION (TAG_USER + 0x1f0ff)
   if (PPCGetAttr(PPCINFOTAG_EMULATION) == 'WARP') {
      if (!((PPCVersion() == 46 && PPCRevision() >= 29) || PPCVersion() > 46)) {
         printf("Requires PPCLibEmu 0.8a or higher!\n");
         done = FALSE;
      }
   }
   else {
      if (!((PPCVersion() == 46 && PPCRevision() >= 30) || PPCVersion() > 46)) {
         printf("Requires ppc.library 46.30 or higher!\n");
         done = FALSE;
      }
   }
   if (done) {
      if (PPCGetTaskAttr(PPCTASKTAG_STACKSIZE) < 200000) {
         printf("Please increase stack size to at least 200000 bytes!\n");
         done = FALSE;
      }
   }
   if (done) {
      done = ((PPCLibBase = OpenLibrary("ppc.library",46)) != NULL);
   }
   if (done) {
      struct TagItem tags[2] = { {PPCPORTTAG_NAME, (ULONG)"dnetc"}, {TAG_END,0} };
      void *port;
      if (!(port = PPCObtainPort(tags))) {
         done = FALSE;
         if ((TriggerPort = PPCCreatePort(tags))) {
            done = TRUE;
         }
      }
      else {
         PPCReleasePort(port);
      }
   }
   #else
   /*
   ** WarpOs
   */
   if (!FindPortPPC("dnetc")) {
      if ((TriggerPort = CreateMsgPortPPC())) {
         TriggerPort->mp_Port.mp_Node.ln_Name = "dnetc";
         TriggerPort->mp_Port.mp_Node.ln_Pri = 0;
         AddPortPPC(TriggerPort);
      }
      else {
         done = FALSE;
      }
   }
   #endif

   #else
   /*
   ** 68K / OS4 / MorphOS
   */

   struct MsgPort *portexists;

   Forbid();

   portexists = FindPort("dnetc");
   if (!portexists) {
      if ((TriggerPort = CreateMsgPort())) {
         TriggerPort->mp_Node.ln_Name = "dnetc";
         TriggerPort->mp_Node.ln_Pri = 0;
         AddPort(TriggerPort);
      }
      else {
         done = FALSE;
      }
   }

   Permit();
   #endif

   if (!TimerBase && done) done = GlobalTimerInit();

   if (!done) amigaExit();

   #ifndef NO_GUI
   /* Workbench startup */
   if (done && *argc == 0) {
      struct WBStartup *wbs = (struct WBStartup *)*argv;
      struct WBArg *arg = wbs->sm_ArgList;
      static char *newargv[1];
      newargv[0] = (char *)arg->wa_Name;

      *argc = 1;
      *argv = newargv;

      if (wbs->sm_NumArgs > 1) {
         /* Started via a project icon */
         arg = &wbs->sm_ArgList[1];
      }

      if (!(amigaGUIInit((char *)wbs->sm_ArgList->wa_Name,arg))) {
         if (!(amigaOpenNewConsole("CON://630/300/distributed.net client/CLOSE/WAIT"))) {
            done = FALSE;
         }
      }
   }
   #endif

   #if USE_RESETHANDLER
   if (done) {
      add_resethandler();
   }
   #endif

   return(done);
}

void amigaExit(void)
{
   #if USE_RESETHANDLER
   rem_resethandler();
   #endif

   #ifndef NO_GUI
   amigaCloseNewConsole();
   amigaGUIDeinit();
   #endif

   #ifdef __amigaos4__
   if (ISocket) DropInterface((struct Interface *)ISocket);
   #else
   CloseLibrary((struct Library *)LocaleBase);
   #endif
   CloseLibrary((struct Library *)SocketBase);

   #ifdef __OS3PPC__
   /*
   ** PPC
   */
   #ifdef CHANGE_MIRROR_TASK_PRI
   SetTaskPri(FindTask(NULL), Old68kMirrorPri);
   #endif
   #ifdef __POWERUP__
   /*
   ** PowerUp
   */
   if (TriggerPort) {
      while (!PPCDeletePort(TriggerPort)) amigaSleep(0,250000);
   }
   CloseLibrary(PPCLibBase);
   #else
   /*
   ** WarpOS
   */
   if (TriggerPort) {
      RemPortPPC(TriggerPort);
      DeleteMsgPortPPC(TriggerPort);
   }
   #endif
   #else
   /*
   ** 68K / OS4 / MorphOS
   */
   if (TriggerPort) {
      RemPort(TriggerPort);
      DeleteMsgPort(TriggerPort);
   }
   #endif

   GlobalTimerDeinit();

   #if USE_RESETHANDLER
   finish_resethandler();
   #endif
}

int amigaThreadInit(void)
{
   return (OpenTimer(TRUE) != NULL);
}

void amigaThreadExit(void)
{
   CloseTimer();
}

ULONG amigaGetTriggerSigs(void)
{
   ULONG trigs = 0, sigr;

   sigr = SetSignal(0L,0L);

   if ( sigr & SIGBREAKF_CTRL_C ) trigs |= DNETC_MSG_SHUTDOWN;

   #ifndef NO_GUI
   if ( DnetcBase && ModeReqIsSet(-1) ) trigs |= dnetcguiHandleMsgs(sigr);
   #endif

   #ifndef __OS3PPC__
   /*
   ** 68K / OS4 / MorphOS
   */
   if ( TriggerPort && sigr & 1L << TriggerPort->mp_SigBit ) {
      struct TriggerMessage *msg;
      while ((msg = (struct TriggerMessage *)GetMsg(TriggerPort))) {
         trigs |= msg->tm_TriggerType;
         ReplyMsg((struct Message *)msg);
      }
   }
   #elif !defined(__POWERUP__)
   /*
   ** WarpOS
   */
   if ( TriggerPort && sigr & 1L << TriggerPort->mp_Port.mp_SigBit ) {
      struct TriggerMessage *msg;
      while ((msg = (struct TriggerMessage *)GetMsgPPC(TriggerPort))) {
         trigs |= msg->tm_TriggerType;
         ReplyMsgPPC((struct Message *)msg);
      }
   }
   #else
   /*
   ** PowerUp
   */
   {
      void *msg;
      while ((msg = PPCGetMessage(TriggerPort))) {
         trigs |= PPCGetMessageAttr(msg,PPCMSGTAG_MSGID);
         PPCReplyMessage(msg);
      }
   }
   #endif

   return(trigs);
}

int amigaPutTriggerSigs(ULONG trigs)
{
   int done = -1;

   #ifndef __OS3PPC__
   /*
   ** 68K / OS4 / MorphOS
   */
   struct TriggerMessage msg;
   if ((msg.tm_ExecMsg.mn_ReplyPort = CreateMsgPort())) {
      struct MsgPort *port;
      msg.tm_TriggerType = trigs;
      msg.tm_ExecMsg.mn_Node.ln_Type = NT_MESSAGE;
      msg.tm_ExecMsg.mn_Length = sizeof(struct TriggerMessage);

      Forbid();
      if ((port = FindPort("dnetc")) && port != TriggerPort) {
         PutMsg(port,(struct Message *)&msg);
      }
      Permit();
      if (port && port != TriggerPort) {
         WaitPort(msg.tm_ExecMsg.mn_ReplyPort);
         GetMsg(msg.tm_ExecMsg.mn_ReplyPort);
         done = 1;
      }
      else {
         done = 0;
      }
      DeleteMsgPort(msg.tm_ExecMsg.mn_ReplyPort);
   }
   #elif !defined(__POWERUP__)
   /*
   ** WarpOS
   */
   struct TriggerMessage msg;
   struct MsgPortPPC *replyport;
   if ((replyport = CreateMsgPortPPC())) {
      msg.tm_ExecMsg.mn_ReplyPort = (struct MsgPort *)replyport;
      msg.tm_ExecMsg.mn_Node.ln_Type = NT_MESSAGE;
      msg.tm_ExecMsg.mn_Length = sizeof(struct TriggerMessage);
      msg.tm_TriggerType = trigs;

      struct MsgPortPPC *port;
      if ((port = FindPortPPC("dnetc")) && port != TriggerPort) {
         PutMsgPPC(port,(struct Message *)&msg);
         WaitPortPPC(replyport);
         GetMsgPPC(replyport);
         done = 1;
      }
      else {
         done = 0;
      }
      DeleteMsgPortPPC(replyport);
   }
   #else
   /*
   ** PowerUp
   */
   struct TagItem tags[2] = { {PPCPORTTAG_NAME, (ULONG)"dnetc"}, {TAG_END,0} };
   void *msg, *replyport, *port;
   if ((replyport = PPCCreatePort(NULL))) {
      if ((msg = PPCCreateMessage(replyport,0))) {
         done = 0;
         if ((port = PPCObtainPort(tags))) {
            if (port != TriggerPort) {
               if (PPCSendMessage(port,msg,NULL,0,trigs)) {
                  PPCWaitPort(replyport);
                  PPCGetMessage(replyport);
                  done = 1;
               }
            }
            PPCReleasePort(port);
         }
         PPCDeleteMessage(msg);
      }
      PPCDeletePort(replyport);
   }
   #endif

   return(done);
}

#if !defined(__amigaos4__) && !defined(__MORPHOS__)
extern unsigned long *__stdfiledes;	// libnix internal

int ftruncate(int fd, int newsize)
{
   int result = SetFileSize(__stdfiledes[fd],newsize,OFFSET_BEGINNING);
   if (result != -1) return 0;
   return(result);
}
#endif

#if defined(__OS3PPC__) && defined(__POWERUP__)
/*
** libnix for PowerUp has broken strncpy().  Was causing NetResolve to trash
** addresses, confrwv.cpp to crash, and probably heaps of other problems!
*/
char *strncpy(char *d, const char *s, size_t n)
{
    char c;
    char *base = d;

    while(n && (c = *s)) {
	*d = c;
	++s;
	++d;
	--n;
    }
    if (n)
	*d = 0;
    return(base);
}

/*
** libnix for PowerUp is missing chmod()
*/
#include <sys/types.h>
#include <sys/stat.h>
#pragma pack(2)
#include <dos/dosextens.h>
#pragma pack()

__BEGIN_DECLS
extern void __seterrno(void);
extern char *__amigapath(const char *path);
__END_DECLS

int chmod(const char *name, mode_t mode)
{ int ret;

  if((name=__amigapath(name))==NULL)
    return -1;

  if ((ret=~(SetProtection((STRPTR)name,((mode&S_IRUSR?0:FIBF_READ)|
                                         (mode&S_IWUSR?0:FIBF_WRITE|FIBF_DELETE)|
                                         (mode&S_IXUSR?0:FIBF_EXECUTE)|
                                         (mode&S_IRGRP?FIBF_GRP_READ:0)|
                                         (mode&S_IWGRP?FIBF_GRP_WRITE|FIBF_GRP_DELETE:0)|
                                         (mode&S_IXGRP?FIBF_GRP_EXECUTE:0)|
                                         (mode&S_IROTH?FIBF_OTR_READ:0)|
                                         (mode&S_IWOTH?FIBF_OTR_WRITE|FIBF_OTR_DELETE:0)|
                                         (mode&S_IXOTH?FIBF_OTR_EXECUTE:0))))))
    __seterrno();
                              
  return ret;
}

/*
** libnix for PowerUp has a broken isatty() - writes to 0x100000!
*/
extern unsigned long *__stdfiledes;	// libnix internal

asm(".section	\".text\"\n\t.align 2\n\t.globl __isatty\n\t.type\t __isatty,@function\n__isatty:\n");
int __isatty(int d)
{
   return IsInteractive(__stdfiledes[d]);
}

/*
** libnix startup code doesn't support Workbench - this does
** (thanks to Peter Annuss <paladin@cs.tu-berlin.de>)
*/
extern int    __argc; /* Defined in startup */
extern char **__argv;
extern char  *__commandline;
extern unsigned long __commandlen;
extern struct WBStartup *_WBenchMsg;

extern char __stdiowin[];

static char *cline=NULL; /* Copy of commandline */

static BPTR cd=0;       /* Lock for Current Directory */
static BPTR window=0;   /* CLI-window for start from workbench */
static BPTR input=0, output=0;

/* This guarantees that this module gets linked in.
   If you replace this by an own reference called
   __nocommandline you get no commandline arguments */

asm(".section	\".text\"\n\t.align 2\n\t.globl __nocommandline\n\t.type\t __nocommandline,@function\n__nocommandline:\n");
void __nocommandline(void)
{
    struct WBStartup *wbs=_WBenchMsg;

    if (wbs!=NULL)
    {
      if(__stdiowin[0])
      { BPTR win;

        if((window=win=PPCOpen(__stdiowin,MODE_OLDFILE))==0l)
          exit(RETURN_FAIL);
        input = SelectInput(win);
        output = SelectOutput(win);
      }

      if(wbs->sm_ArgList!=NULL) /* cd to icon */
        cd=CurrentDir(DupLock(wbs->sm_ArgList->wa_Lock));

      __argc=0;
      __argv=(char **)wbs;
    }
    else
    {
      char **av,*a,*cl=__commandline;
      size_t i=__commandlen;
      int ac;

      if(!(cline=(char *)PPCAllocVec(i+1,MEMF_ANY))) /* get buffer */
        exit(RETURN_FAIL);
  
      for(a=cline,ac=1;;) /* and parse commandline */
      {
        while(i&&(*cl==' '||*cl=='\t'||*cl=='\n'))
        { cl++;
          i--; }
        if(!i)
          break;
        if(*cl=='\"')
        {
          cl++;
          i--;
          while(i)
          {
            if(*cl=='\"')
            {
              cl++;
              i--;
              break;
            }
            if(*cl=='*')
            {
              cl++;
              i--;
              if(!i)
                break;
            }
            *a++=*cl++;
            i--;
          }
        }
        else
          while(i&&(*cl!=' '&&*cl!='\t'&&*cl!='\n'))
          { *a++=*cl++;
            i--; }
        *a++='\0';
        ac++;
      }
        /* NULL Terminated */
      if(!(__argv=av=(char **)PPCAllocVec(((__argc=ac-1)+1)*sizeof(char *),MEMF_ANY|MEMF_CLEAR)))
        exit(RETURN_FAIL);

      for(a=cline,i=1;i<ac;i++)
      { 
        av[i-1]=a;
        while(*a++)
          ; 
      }
    }
}

asm(".section	\".text\"\n\t.align 2\n\t.globl __exitcommandline\n\t.type\t __exitcommandline,@function\n__exitcommandline:\n");
void __exitcommandline(void)
{
  struct WBStartup *wbs=_WBenchMsg;

  if (wbs!=NULL)
  { BPTR file;
    if((file=window)!=0) {
      SelectOutput(output);
      SelectInput(input);
      PPCClose(file);
    }
    if(wbs->sm_ArgList!=NULL) { // set original lock
      UnLock(CurrentDir(cd));
    }
  }
  else
  {
    char *cl=cline;

    if(cl!=NULL)
    { char **av=__argv;

      if(av!=NULL)
      { 
        PPCFreeVec(av);
      }
      PPCFreeVec(cl);
    }
  }
}
  
/* Add these two functions to the lists */
#define ADD2INIT(a,pri) asm(".section .init"); \
                        asm(" .long .text+(" #a "-.text); .long 0x58")
#define ADD2EXIT(a,pri) asm(".section .fini"); \
                        asm(" .long .text+(" #a "-.text); .long 0x58")

ADD2INIT(__nocommandline,-40);
ADD2EXIT(__exitcommandline,-40);

#endif /* defined(__OS3PPC__) && defined(__POWERUP__) */
