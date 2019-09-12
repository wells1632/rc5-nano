/*
 * Copyright distributed.net 1997-2016 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: common.h,v 1.3 2007/10/22 16:48:30 jlawson Exp $
 *
 * Created by Oliver Roberts <oliver@futaura.co.uk>
 *
 * ----------------------------------------------------------------------
 * ReAction GUI module for AmigaOS clients - common defines
 * ----------------------------------------------------------------------
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/cghooks.h>
#include <intuition/icclass.h>
#include <workbench/icon.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <clib/alib_protos.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/utility.h>
#include <proto/dos.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Our custom library structure

struct LibBase
{
   struct Library		lb_LibNode;		// Exec link
   UWORD			lb_Pad;			// Longword alignment

   struct Library *		lb_SysBase;		// Exec library
   struct Library *		lb_DOSBase;		// Dos library
   struct Library *		lb_IntuitionBase;	// Intuition library
   struct Library *		lb_GfxBase;		// Graphics library
   struct Library *		lb_UtilityBase;		// Utility library

   BPTR				lb_SegList;		// Library segment pointer
   struct SignalSemaphore	lb_BaseLock;
   struct SignalSemaphore	lb_GUILock;
};


#define NO_LIBBASE_REMAPPING

#ifndef NO_LIBBASE_REMAPPING
#define SysBase			lb->lb_SysBase
#define DOSBase			lb->lb_DOSBase
#define IntuitionBase		lb->lb_IntuitionBase
#define GfxBase			lb->lb_GfxBase
#define UtilityBase		lb->lb_UtilityBase
#endif

#define LIBFUNC SAVEDS ASM

/******************************************************************************/

// Preprocessor tricks

#ifdef __VBCC__

#define ASM	
#define __asm
#define __stdargs
#define __regargs
#define __aligned
#define INLINE
#define REG(x,y) __reg(# x) y

#elif defined(__SASC)

#define ASM __asm
#define REG(x,y) register __ ## x y
#define INLINE __inline

#elif defined(__GNUC__)

#define ASM
#undef SAVEDS
#define SAVEDS
#define INLINE __inline__
#ifndef __amigaos4__
#define REG(x,y) y __asm(#x)
#define ALIAS(a,b) __asm(".stabs \"_" #a "\",11,0,0,0\n\t.stabs \"_" #b "\",1,0,0,0")
#else
#define ALIAS(a,b)
#endif

#endif


/******************************************************************************/

// Prototypes

struct ClientGUIParams
{
   ULONG signals;
};

#undef NewObject

#ifdef __amigaos4__
struct Library *OpenLibraryIFace( STRPTR name, ULONG version, APTR *iface );
VOID CloseLibraryIFace( struct Library *lib, APTR iface );
#define DoMethod IDoMethod
#define NewObject IIntuition->NewObject

#ifdef CreateMsgPort
#undef CreateMsgPort
#endif
#define CreateMsgPort() AllocSysObject(ASOT_PORT,NULL)
#ifdef DeleteMsgPort
#undef DeleteMsgPort
#endif
#define DeleteMsgPort(msgPort) FreeSysObject(ASOT_PORT,msgPort)

LIBFUNC ULONG dnetcguiOpen(struct Interface *self, ULONG cpu, UBYTE *programname, struct WBArg *iconname, const char *vstring);
LIBFUNC BOOL dnetcguiClose(struct Interface *self, struct ClientGUIParams *params);
LIBFUNC VOID dnetcguiConsoleOut(struct Interface *self, ULONG cpu, UBYTE *output, BOOL overwrite);
LIBFUNC ULONG dnetcguiHandleMsgs(struct Interface *self, ULONG signals);
#else

#define OpenLibraryIFace(name,version,iface) OpenLibrary(name,version)
#define CloseLibraryIFace(lib,iface) CloseLibrary(lib)
#define VARARGS68K

LIBFUNC ULONG dnetcguiOpen(REG(d0,ULONG cpu),REG(a0,UBYTE *programname),REG(a1,struct WBArg *iconname),REG(a2,const char *vstring),REG(a6,struct LibBase *lb));
LIBFUNC BOOL dnetcguiClose(REG(a0,struct ClientGUIParams *params),REG(a6,struct LibBase *libbase));
LIBFUNC ULONG dnetcguiHandleMsgs(REG(d0,ULONG signals),REG(a6,struct LibBase *lb));
LIBFUNC VOID dnetcguiConsoleOut(REG(d0,ULONG cpu),REG(a0,UBYTE *output),REG(d1,BOOL overwrite));
#endif
