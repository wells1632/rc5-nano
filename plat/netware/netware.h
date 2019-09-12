/* 
 * Copyright distributed.net 1997-2002 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Catch-all header file for the distributed.net client for NetWare 
 * written by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
 * $Id: netware.h,v 1.2 2002/09/02 00:35:51 andreasb Exp $
*/

#ifndef __NETWARE_CLIENT_H__
#define __NETWARE_CLIENT_H__ 

#include "nwlemu.h"   /* kernel/clib portability stubs */
#include "nwlcomp.h"  /* ANSI/POSIX compatibility functions */

#ifdef __cplusplus
extern "C" {
#endif
#include <nwmpk.h>    /* emulation library for <nwmpk.h> */
extern void __MPKDelayThread( unsigned int millisecs );
extern int  __MPKSetThreadAffinity( int cpunum );
extern int  __MPKEnableThreadPreemption( void );
extern int  __MPKDisableThreadPreemption( void );
#ifdef __cplusplus
}
#endif

#include "nwccons.h"  /* client console management */
#include "nwcmisc.h"  /* misc other functions called from client code */
#include "nwcconf.h"  /* client-for-netware specific configuration options */

#if defined(HAVE_POLLPROC_SUPPORT)
#include "nwcpprun.h"
#endif

#endif /* __NETWARE_CLIENT_H__ */
