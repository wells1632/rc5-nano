/*
 * Copyright distributed.net 1999-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: ogrng-ppc.cpp,v 1.4 2008/03/29 21:54:12 kakace Exp $
*/

#include "ansi/ogrng-32.h"


#if defined(ASM_PPC) || defined(__PPC__) || defined(__POWERPC__)

  #define OGROPT_HAVE_FIND_FIRST_ZERO_BIT_ASM     2 /* 0-2 - '100% asm'      */
  #define OGROPT_ALTERNATE_CYCLE                  1 /* 0-1 - '100% asm'      */

  /*========================================================================*/

  #include "asm-ppc.h"

  #if (OGROPT_HAVE_FIND_FIRST_ZERO_BIT_ASM == 2) && defined(__CNTLZ__)
    #define __CNTLZ(x) (__CNTLZ__(~(x))+1)
  #endif


  /*
  ** Define the name of the dispatch table.
  */
  #define OGR_NG_GET_DISPATCH_TABLE_FXN    ogrng_get_dispatch_table

  #include "ansi/ogrng_codebase.cpp"


  /*
  ** Check the settings again since we have to make sure ogr_create()
  ** produces compatible datas.
  */
  #if defined(HAVE_FLEGE_PPC_CORES) && (OGROPT_ALTERNATE_CYCLE > 0)

    #ifdef __cplusplus
    extern "C" {
    #endif
    int cycle_ppc_scalar_256(struct OgrState *state, int *pnodes, const u16 *choose);
    #ifdef __cplusplus
    }
    #endif

    static int ogr_cycle_256(struct OgrState *oState, int *pnodes,
                             const u16* pchoose)
    {
      return cycle_ppc_scalar_256(oState, pnodes, pchoose);
    }
  #endif

#else
  #error use this only with ppc since it may contain ppc assembly
#endif
