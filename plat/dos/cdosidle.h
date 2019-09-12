/* Hey, Emacs, this a -*-C++-*- file !
 *
 * Copyright distributed.net 1997-2002 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 * 
 * Written by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
*/
#ifndef __CLIDOS_IDLE_H__
#define __CLIDOS_IDLE_H__ "@(#)$Id: cdosidle.h,v 1.2 2002/09/02 00:35:50 andreasb Exp $"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INCLUDING_CDOSIDLE_CPP
#define __USLEEP_RET unsigned int
#define __SLEEP_RET unsigned int
#define __DELAY_RET unsigned int
#elif defined(__WATCOMC__) 
#define __USLEEP_RET void
#define __SLEEP_RET void
#define __DELAY_RET void
#else
#define __USLEEP_RET unsigned int /* guess */
#define __SLEEP_RET unsigned int /* guess */
#define __DELAY_RET unsigned int /* guess */
#endif

extern __DELAY_RET delay(unsigned int msecs); 
extern __USLEEP_RET usleep(unsigned int usecs);
extern __SLEEP_RET sleep(unsigned int secs);

#undef __USLEEP_RET
#undef __SLEEP_RET
#undef __DELAY_RET

#ifdef __cplusplus
}
#endif

#endif /* __CLIDOS_IDLE_H__ */
