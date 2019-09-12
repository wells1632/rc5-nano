/* Hey, Emacs, this a -*-C-*- file !
 *
 * Copyright distributed.net 1997-2003 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
*/

#ifndef __PACK1_H__
# define __PACK1_H__ "@(#)$Id: pack1.h,v 1.3 2007/10/22 16:48:26 jlawson Exp $"

# include "pack.h"
#endif

/* make sure we don't get confused by predefined macros and initialise
** DNETC_PACKED to do packing to one byte boundaries */
#undef DNETC_PACKED
#define DNETC_PACKED DNETC_PACKED1

#ifdef DNETC_USE_PACK
# pragma pack(1)
#endif

/* end of file */
