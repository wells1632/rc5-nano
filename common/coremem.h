/* Hey, Emacs, this a -*-C-*- file !
 *
 * Copyright distributed.net 1997-2003 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Allocation/free of memory used by crunchers. For crunchers that were
 * created with fork(), the memory is shared between client and cruncher.
 * Created March 2001 by Cyrus Patel <cyp@fb14.uni-mainz.de>
 *
 * ** header may be included by cores, so guard around c++ constructs **
*/

#ifndef __COREMEM_H__
#define __COREMEM_H__ "@(#)$Id: coremem.h,v 1.5 2007/10/22 16:48:25 jlawson Exp $"

#ifdef __cplusplus /* header may be included by cores */
extern "C" {
#endif

/* only for HAVE_MULTICRUNCH_VIA_FORK */
void cmem_select_allocator(int which);

void *cmem_alloc(unsigned int sz);
int cmem_free(void *mem);

#ifdef __cplusplus
}
#endif

#endif /* __COREMEM_H__ */

