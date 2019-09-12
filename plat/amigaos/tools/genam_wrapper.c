/*
 * Copyright distributed.net 1997-2002 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * $Id: genam_wrapper.c,v 1.2 2002/09/02 00:35:50 andreasb Exp $
 *
 * Created by Oliver Roberts <oliver@futaura.co.uk>
 *
 * ----------------------------------------------------------------------
 * Simple wrapper to convert the asm command line generated by configure
 * to be compatible with genam (-o in genam requires that no space is
 * inserted between -o and the filename)
 * ----------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdlib.h>
#include <proto/dos.h>

int main(int argc, char **argv)
{
   int ret = 20;
   UBYTE cmd[512];

   if (argc >= 4) {
      sprintf(cmd,"genam %s to %s -l",argv[1],argv[3]);
      ret = System(cmd,NULL);
   }

   return(ret);
}
