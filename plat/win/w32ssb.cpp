/*
 * Copyright distributed.net 1997-2002 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Win32/Win16 Screen Saver boot stuff.
 * Written by Cyrus Patel <cyp@fb14.uni-mainz.de>
*/

#if (!defined(lint) && defined(__showids__))
const char *w32ssb_cpp(void) {
return "@(#)$Id: w32ssb.cpp,v 1.2 2002/09/02 00:35:53 andreasb Exp $"; }
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern int PASCAL SSMain(HINSTANCE, HINSTANCE, LPSTR, int);

int PASCAL WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpszCmdLine, int nCmdShow)
{ 
  return SSMain(hInst, hPrev, lpszCmdLine, nCmdShow); 
}
