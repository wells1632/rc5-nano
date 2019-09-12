/* Hey, Emacs, this is *not* a -*-C++-*- file !
 *
 * Copyright distributed.net 2000-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * TCP/IP network base functions with automatic stack and
 * dialup-device initialization and shutdown.
 * Written October 2000 by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
 * This module was written for maximum portability - No net API
 * specific structures or functions need to be known outside this
 * module.
*/
#ifndef __NETBASE_H__
#define __NETBASE_H__ "@(#)$Id: netbase.h,v 1.8 2012/06/05 22:12:55 snikkel Exp $"

#include "cputypes.h" /* u32 */

#if !defined(INVALID_SOCKET)
typedef int SOCKET;
  #define INVALID_SOCKET ((SOCKET)-1)
#endif

/* lower case error names are 'source markers' and not meaningful outside netbase */
#define ps_stdneterr    -1 /* look at errno/WSAGetLastError()/sock_errno() etc */
#define ps_stdsyserr    -2 /* look at errno */
#define ps_bsdsockerr   -3 /* look at getsockopt(fd,SOL_SOCKET,SO_ERROR,...) */
#define ps_oereserved   -4 /* special to net_open() */
#define ps_EBADF        -5
#define ps_ENETDOWN     -6
#define ps_EINVAL       -7
#define ps_EINTR        -8
#define ps_ETIMEDOUT    -9
#define ps_EDISCO      -10
#define ps_ENOSYS      -11 /* function not implemented */
#define ps_ENODATA     -12 /* Valid name, no data record of requested type */
#define ps_ENOENT      -13 /* no entry for requested name */
#define ps_EINPROGRESS -14
#define ps_ENOENT_host_cmd  -15 /* "Perhaps the 'host' command was not found?" */
#define ps_ELASTERR ps_ENOENT_host_cmd

/* all functions that return int, return zero on success or an error code
   on failure. The error code can then be translated with net_strerror().
*/

#ifdef __cplusplus
extern "C" {
#endif

/* one shot init/deinit. Must be called to init once before any network
 * I/O (anywhere) can happen, and and once to deinit before application
 * shutdown. Multiple init/deinit calls are ok as long as they are in
 * init/deinit pairs and the very last deinit has 'final_call' set.
*/
int net_initialize(void);
int net_deinitialize(int final_call);

/* get a descriptive error message for an error number returned by one
 * of the net_xxx() functions that return 'int'
*/
const char *net_strerror(int /*ps_*/ errnum, SOCKET fd);

/* create/close a tcp endpoint. May cause an api library to be
 * loaded/unloaded. open may implicitely (lib load) or exlicitely
 * (user wants dialup control) cause a dialup connection. If
 * a dialup was explicitely caused, net_close() will disconnect.
 * If local_port is non-zero, a listener is created.
*/
#ifdef HAVE_IPV6
int net_open( SOCKET *sockP, const char *srv_hostname, int srv_port,
                 int iotimeout /* millisecs */ );
#else
int net_open( SOCKET *sockP, u32 local_addr, int local_port );
#endif
int net_close( SOCKET sock );

/* read/write from an endpoint. read() will return as soon as any
*  data is available (or error or timeout). write() will return
*  as soon as the data has been queued completely (which may require
*  some of the data to be sent over the wire first). On timeout (no
*  data was sent/recvd), both functions returns zero and *bufsz will
*  be zero. This is believed to be more useful than returning a
*  'timedout' error code.
*/

int net_read( SOCKET sock, char *data, unsigned int *bufsz,
              int iotimeout );
int net_write( SOCKET sock, const char *__data, unsigned int *bufsz,
               int iotimeout );

#ifndef HAVE_IPV6
/* connect to a peer. that_address and that_port are the address to
 * connect to. The authoritative peer address and port will be returned.
 * If a specific local address is to be connected() from, then pass that
 * address via this_address, otherwise pass zero/null. The effective
 * local address and port will be returned via this_address/this_port.
 * Note that, for client connections, this is the only way to obtain the
 * address of the local and/or peer endpoints.
*/
int net_connect( SOCKET sock, u32 *that_address, int *that_port,
                 u32 *this_address, int *this_port,
                 int iotimeout /* millisecs */ );
#endif

/* NETDB name to address resolution. Returns error code on error, 0 on success
 * On entry, max_addrs contains the number of address slots in addr_list, on
 * return, it will contain the number of sucessfully obtained addresses.
*/
int net_resolve_v4( const char *hostname, u32 *addr_list, unsigned int *max_addrs);

/* get the name of the local host.
 * Returns error code on error, 0 on success.
*/
int net_gethostname(char *buffer, unsigned int len);

/* convert an IP address in string form to a numeric address and vice
 * versa. Unlike inet_addr(), the address is considered invalid if it
 * does not contain 4 parts.
*/
int net_aton_v4( const char *cp, u32 *inp );
const char *net_ntoa_v4( u32 addr );

#ifdef __cplusplus
}
#endif

#endif /* __NETBASE_H__ */
