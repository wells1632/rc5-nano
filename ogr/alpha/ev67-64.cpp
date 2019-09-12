/*
 * Copyright distributed.net 2002-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 */

#include "ansi/ogrp2-64.h"


const char *ogr_ev67_64_cpp(void) {
return "@(#)$Id: ev67-64.cpp,v 1.4 2008/12/30 20:58:43 andreasb Exp $"; }

#if defined(__GNUC__)
  #define OGROPT_HAVE_FIND_FIRST_ZERO_BIT_ASM   2 /* 0-2 - "100% asm"      */
  #define OGROPT_STRENGTH_REDUCE_CHOOSE         0 /* 0/1 - 'yes' (default) */
  #define OGROPT_NO_FUNCTION_INLINE             0 /* 0/1 - 'no'  (default) */
  #define OGROPT_HAVE_OGR_CYCLE_ASM             0 /* 0-2 - 'no'  (default) */
  #define OGROPT_CYCLE_CACHE_ALIGN              0 /* 0/1 - 'no'  (default) */
  //#define OGROPT_ALTERNATE_COMP_LEFT_LIST_RIGHT 1 /* 0/1 - register based */
#else /* Compaq CC */
  #include <c_asm.h>
  #define OGROPT_HAVE_FIND_FIRST_ZERO_BIT_ASM   2 /* 0-2 - "100% asm"      */
  #define OGROPT_STRENGTH_REDUCE_CHOOSE         0 /* 0/1 - 'yes' (default) */
  #define OGROPT_NO_FUNCTION_INLINE             0 /* 0/1 - 'no'  (default) */
  #define OGROPT_HAVE_OGR_CYCLE_ASM             0 /* 0-2 - 'no'  (default) */
  #define OGROPT_CYCLE_CACHE_ALIGN              0 /* 0/1 - 'no'  (default) */
  //#define OGROPT_ALTERNATE_COMP_LEFT_LIST_RIGHT 1 /* 0/1 - register based */
#endif

#define ALPHA_CIX
#define OGR_GET_DISPATCH_TABLE_FXN    ogr_get_dispatch_table_cix_64

#include "baseincs.h" //for endian detection
#include "alpha/alpha-asm.h"
#include "ansi/ogrp2_codebase.cpp"
