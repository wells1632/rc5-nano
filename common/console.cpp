/*
 * Copyright distributed.net 1997-2015 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * ----------------------------------------------------------------------
 * This module contains the screen i/o primitives/wrappers for all
 * those GUIs and less-than-G UIs we have been threatened with :)
 * and we pretty much have scattered about anyway.
 *
 * Implementation guidelines: none. see what the neighbour did.
 * Keep the functions small (total less than 25 lines) or make calls
 * to functions in modules in your own platform area.   - cyp
 * ----------------------------------------------------------------------
*/
const char *console_cpp(void) {
return "@(#)$Id: console.cpp,v 1.85 2011/03/31 05:07:28 jlawson Exp $"; }

/* -------------------------------------------------------------------- */

#include "cputypes.h"
#include "baseincs.h"
#include "version.h"  //CLIENT_VERSIONSTRING
#include "clitime.h"
#include "triggers.h"
#include "util.h"     //utilGetAppName(), DNETC_UNUSED_*
#include "sleepdef.h" //usleep()
#include "console.h"  //ourselves

#if (CLIENT_OS == OS_NETWARE6)
#include <screen.h>
#endif

#if !defined(NOTERMIOS) && ((CLIENT_OS==OS_SOLARIS) || (CLIENT_OS==OS_IRIX) \
  || (CLIENT_OS==OS_LINUX) || (CLIENT_OS==OS_NETBSD) || (CLIENT_OS==OS_BEOS) \
  || (CLIENT_OS==OS_FREEBSD) || ((CLIENT_OS==OS_OS2) && defined(__EMX__)) \
  || (CLIENT_OS==OS_AIX) || (CLIENT_OS==OS_DEC_UNIX) || (CLIENT_OS==BSDOS) \
  || (CLIENT_OS==OS_OPENBSD) || (CLIENT_OS==OS_HPUX) || (CLIENT_OS==OS_SUNOS) \
  || (CLIENT_OS==OS_MACOSX) || (CLIENT_OS==DRAGONFLY) \
  || ((CLIENT_OS==OS_QNX) && defined(__QNXNTO__)) \
  || (CLIENT_OS==OS_DYNIX)) || (CLIENT_OS == OS_PS2LINUX) \
  || (CLIENT_OS == OS_HAIKU) || (CLIENT_OS == OS_IOS) \
  || (CLIENT_OS == OS_ANDROID)
#include <termios.h>
#define HAVE_TERMIOS
#elif (CLIENT_OS == OS_SCO)
#define _SVID3 /* need it for ECHOPRT and ECHOCTL */
#include <termios.h>
#define HAVE_TERMIOS
#endif

#if !defined(NOSGTTY) && (CLIENT_OS == OS_NEXTSTEP)
#include <sgtty.h>
#define HAVE_SGTTY 1
#endif

#if defined(__unix__) || (CLIENT_OS == OS_VMS) || (CLIENT_OS == OS_OS390) \
  || (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
#define HAVE_ANSICOMPLIANTTERM /* tty understands basic ansi sequences */
#endif
#if defined(__unix__)
#include <sys/ioctl.h>
#endif
#if (CLIENT_OS == OS_RISCOS)
extern "C" void riscos_backspace();
#endif
#if defined(__EMX__)
#include <sys/video.h>
#endif
/* ---------------------------------------------------- */

static struct
{
  int initlevel;
  int runhidden;
  int conisatty;
} constatics = {0,0,0};

/* ---------------------------------------------------- */

int DeinitializeConsole(int waitforuser)
{
  /* 'waitforuser' is set if client ran modes (so that the user
     can see the results before the screen/window disappears)
  */
  waitforuser = waitforuser;
  if (constatics.initlevel == 1)
  {
    if (constatics.runhidden || !constatics.conisatty)
      waitforuser = 0;
    #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    w32DeinitializeConsole(waitforuser);
    #elif (CLIENT_OS == OS_NETWARE)
    nwCliDeinitializeConsole(waitforuser);
    #elif (CLIENT_OS == OS_OS2) && defined(OS2_PM)
    os2CliDeinitializeConsole(waitforuser);
    #endif
  }
  constatics.initlevel--;
  return 0;
}

/* ---------------------------------------------------- */

int InitializeConsole(int *runhidden,int doingmodes)
{
  int retcode = 0;

  DNETC_UNUSED_PARAM(doingmodes);

  if ((++constatics.initlevel) == 1)
  {
    memset( (void *)&constatics, 0, sizeof(constatics) );
    constatics.initlevel = 1;
    constatics.runhidden = *runhidden;

    #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    retcode = w32InitializeConsole(constatics.runhidden,doingmodes);
    #elif (CLIENT_OS == OS_NETWARE)
    retcode = nwCliInitializeConsole(constatics.runhidden,doingmodes);
    #elif (CLIENT_OS == OS_OS2)
     #if defined(OS2_PM)
     retcode = os2CliInitializeConsole(constatics.runhidden,doingmodes);
     #endif
     #if defined(__EMX__)
     v_init();
     #endif
    #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    retcode = amigaInitializeConsole(constatics.runhidden,doingmodes);
    #endif

    if (retcode != 0)
      --constatics.initlevel;
    else if (ConIsGUI())
      constatics.conisatty = 1;
    else if (!constatics.runhidden)
    {
      #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
        constatics.conisatty = w32ConIsScreen();
      #elif (CLIENT_OS == OS_RISCOS)
        constatics.conisatty = 1;
      #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
        constatics.conisatty = amigaConIsScreen();
      #else
        constatics.conisatty = (isatty(fileno(stdout)));
      #endif
    }
  } /* constatics.initlevel == 1 */

  return retcode;
}

/* ---------------------------------------------------- */

int ConIsGUI(void)
{
  #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
  /* => 'C'=native console, 'c'=pipe console, 'g'=lite GUI, 'G'=fat GUI */
  return ((w32ConGetType() & 0xff)=='G' || (w32ConGetType() & 0xff)=='g');
  #elif (CLIENT_OS == OS_OS2) && defined(OS2_PM)
  return 1;
  #elif (CLIENT_OS == OS_RISCOS)
  extern int guiriscos;
  return (guiriscos!=0);
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  return amigaConIsGUI();
  #else
  return 0;
  #endif
}

/* ---------------------------------------------------- */

int ConIsScreen(void)
{
  return (constatics.initlevel > 0 && constatics.conisatty );
}

/* ---------------------------------------------------- */

/*
** ConBeep() does uh..., well..., like... causes the console speaker to beep
*/
int ConBeep(void)
{
  if (constatics.initlevel > 0 && constatics.conisatty) /*can't beep to file*/
  {
    #if (CLIENT_OS == OS_OS390)
    ConOut("\a");
    #else
    ConOut("\007");
    #endif
    return 0;
  }
  return -1;
}

/* ---------------------------------------------------- */

/*
** ConOut() does what printf("%s",str) would do
*/
int ConOut(const char *msg)
{
  if (constatics.initlevel > 0 /*&& constatics.conisatty*/ )
  {
    #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
      w32ConOut(msg);
    #elif (CLIENT_OS == OS_OS2 && defined(OS2_PM))
      os2conout(msg);
    #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
      amigaConOut(msg);
    #else
      fwrite( msg, sizeof(char), strlen(msg), stdout);
      fflush(stdout);
    #endif
    return 0;
  }
  return -1;
}

/* ---------------------------------------------------- */

/*
** ConOutModal() should only be used when the console is known to be
** uninitialized and should be avoided. Not affected by -hidden/-quiet mode
*/
int ConOutModal(const char *msg)
{
  #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    w32ConOutModal(msg);
  #elif (CLIENT_OS == OS_OS2) && defined(OS2_PM)
    WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, msg,
       "distributed.net client " CLIENT_VERSIONSTRING "",
       NULL, MB_OK | MB_INFORMATION | MB_MOVEABLE );
  #elif (CLIENT_OS == OS_NETWARE)
    ConsolePrintf( "%s\r\n", msg );
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    amigaConOutModal(msg);
  #else
    fprintf( stderr, "%s\n", msg );
    fflush( stderr );
  #endif
  return 0;
}

/* ---------------------------------------------------- */

/*
** ConOutErr() does what fprintf(stderr, "APPNAME: %s\n",msg) would do.
** Can be blocking. Note the trailing newline.
*/

int ConOutErr(const char *msg)
{
  #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    w32ConOutErr(msg);
  #elif (CLIENT_OS == OS_OS2) && defined(OS2_PM)
     WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, (PSZ)msg,
           "distributed.net client " CLIENT_VERSIONSTRING "",
           NULL, MB_OK | MB_APPLMODAL | MB_ERROR | MB_MOVEABLE );
  #elif (CLIENT_OS == OS_NETWARE)
    ConsolePrintf( "%s: %s\r\n", utilGetAppName(), msg );
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    amigaConOutErr(msg);
  #else
    fprintf( stderr, "%s: %s\n", utilGetAppName(), msg );
    fflush( stderr );
  #endif
  return 0;
}

/* ---------------------------------------------------- */

/*
** ConInKey() does what a (non-blocking and polling) DOS-ish getch() would do
** key is not echoed. timeout ==> 0 == don't wait, -1 == wait forever.
*/

int ConInKey(int timeout_millisecs) /* Returns -1 if err. 0 if timed out. */
{
  int ch = -1;

  if (constatics.initlevel > 0 && constatics.conisatty)
  {
    timeval timenow, timestop;
    timestop.tv_sec = 0;
    timestop.tv_usec = 0;

    if (timeout_millisecs > 0)
    {
      if (CliClock(&timestop) != 0)
        return -1;
      timestop.tv_sec += timeout_millisecs/1000;
      timestop.tv_usec += ( timeout_millisecs % 1000 )*1000;
      timestop.tv_sec += ( timestop.tv_usec / 1000000 );
      timestop.tv_usec %= 1000000;
    }
    ch = 0;

    do
    {
      #if (CLIENT_OS == OS_RISCOS)
      {
          ch = _kernel_osrdch();
      }
      #elif (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
      {
        if (w32ConKbhit())
        {
          ch = w32ConGetch();
          if (!ch)
            ch = (w32ConGetch() << 8);
        }
      }
      #elif (CLIENT_OS == OS_NETWARE)
      {
        if (nwCliKbHit())
        {
          ch = nwCliGetCh();
          if (!ch)
            ch = (nwCliGetCh()<<8);
        }
      }
	  #elif (CLIENT_OS == OS_NETWARE6)
      {
        if (kbhit()==0)
        {
          ch = getcharacter();
        }
      }
      #elif (CLIENT_OS == OS_DOS)
      {
        fflush(stdout);
        if (kbhit())
        {
          ch = getch();
          if (!ch)
            ch = (getch() << 8);
        }
      }
      #elif (CLIENT_OS == OS_OS2)
      {
        #ifdef __WATCOMC__
        /* Cannot use KbdPeek due to strange default KBD mode. Simple getch()
           work fine */
        if (kbhit())
        {
          ch = getch();
          if (!ch)
            ch = (getch() << 8);
        }
        #else
          ch = getch();
          if (!ch) ch = (getch() << 8);
        #endif
      }
      #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
      {
        fflush(stdout);
        ch = getch();
      }
      #elif (CLIENT_OS == OS_VMS)
      {
        fflush(stdout);       /* OpenVMS doesn't support select() */
        ch = getchar();       /* for non-socket devices such as   */
        if (ch == EOF)        /* stdin.  Use getchar() instead.   */
          ch = 0;
      }
      #elif (defined(HAVE_TERMIOS))
      {
        struct termios stored;
        struct termios newios;
        int fd = fileno(stdin);
        fflush(stdout);
        tcgetattr(fd,&stored); /* Get the original termios configuration */
        memcpy(&newios,&stored,sizeof(struct termios));
        #if (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_HAIKU)
        newios.c_lflag &= ~(ECHO|ECHONL);  /* BeOS/Haiku do not have (non-Posix?) ECHOPRT and ECHOCTL */
        #else
        #if (CLIENT_OS == OS_QNX) && defined(__QNXNTO__)
        newios.c_lflag &= ~(ECHO|ECHONL|ECHOCTL);
        #else
        newios.c_lflag &= ~(ECHO|ECHONL|ECHOPRT|ECHOCTL); /* no echo at all */
        #endif
        #endif
        newios.c_lflag &= ~(ICANON);     /* not linemode and no translation */
        newios.c_cc[VTIME] = 0;          /* tsecs inter-char gap */
        newios.c_cc[VMIN] = 1;           /* number of chars to block for */
        tcsetattr(0,TCSANOW,&newios);    /* Activate the new settings */
        ch = getchar();                  /* Read a single character */
        tcsetattr(fd,TCSAFLUSH,&stored); /* Restore the original settings */
        if (ch == EOF) ch = 0;
      }
      #elif (defined(HAVE_SGTTY))
      {
        struct sgttyb stored;
        int fd = fileno(stdin);

        fflush(stdout);
        if (gtty(fd, &stored) == 0) {
          struct sgttyb newsg;
          memcpy(&newsg, &stored, sizeof(newsg));

          /* it's in the bits we don't set: no echo, delay or cooking
          ** actually we might want CBREAK here since that leaves most
          ** control sequences intact but it sometimes still buffers
          ** whole lines (on NeXTstep at least) */
          newsg.sg_flags = RAW;

          stty(fd, &newsg);
          ch = getchar();
          stty(fd, &stored);

          if (ch == EOF) ch = 0;
        }
      }
      #else
      {
        setvbuf(stdin, (char *)NULL, _IONBF, 0);

        int fd = fileno(stdin);
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET( fd, &rfds );

        fflush(stdout);
        fflush(stdin);
        errno = 0;
        if ( select( fd+1, &rfds, NULL, NULL, NULL) && errno != EINTR )
        {
          ch = fgetc( stdin );
          if (ch == EOF)
            ch = 0;
        }
      }
      #endif
      if (ch || timeout_millisecs == 0 || CheckExitRequestTriggerNoIO())
        break;
      usleep(50*1000);

      CliTimer(&timenow);

    } while ( (timeout_millisecs == -1) ||
              (( timenow.tv_sec < timestop.tv_sec ) ||
              (( timenow.tv_sec == timestop.tv_sec ) &&
              ( timenow.tv_usec < timestop.tv_usec ))));
  }
  return ch;
}

/* ---------------------------------------------------- */

/* ConInStr() is similar to readline(); buffer is always '\0' terminated.
** Returns -1 if err (stdin is not a tty)
*/

int ConInStr(char *buffer, unsigned int buflen, int flags )
{
  int ch;
  int exitreq;
  unsigned int pos;
  int asbool, boolistf, boolval, redraw;

  if (constatics.initlevel < 1 || !constatics.conisatty)
    return -1;

  if (!buffer || !buflen)
    return 0;

  //if ((flags & CONINSTR_ASPASSWORD) != 0)
  //  flags = CONINSTR_ASPASSWORD;

  redraw = asbool = boolval = boolistf = 0;
  if ((flags & CONINSTR_ASBOOLEAN) != 0)
  {
    asbool = 1;
    if ((flags & CONINSTR_BYEXAMPLE) != 0)
    {
      if (buffer[0] =='t' || buffer[0]=='T')
        boolistf = boolval = 1;
      else if (buffer[0] == 'f' || buffer[0] == 'F')
        boolistf = 1;
      else if (buffer[0] == 'y' || buffer[0] == 'Y')
        boolval = 1;
      else if (buffer[0] == 'n' || buffer[0] == 'N') /* default */
        boolval = 0;
    }
    flags = CONINSTR_ASBOOLEAN; /* strip by_example */
    if (buflen > 1) /* we only return a single char */
      buffer[1] = 0;
    redraw = 1;
  }

  if ((flags & CONINSTR_BYEXAMPLE) != 0)
  {
    pos = strlen( buffer );
    if ((flags & CONINSTR_ASPASSWORD)!=0)
    {
      char scratch[2];
      scratch[1] = 0; scratch[0] = '*';
      for (ch = 0; ch < ((int)(pos)); ch++)
        ConOut(scratch);
    }
    else
    {
      ConOut(buffer);
    }
  }
  else
  {
    pos = 0;
    buffer[pos] = 0;
  }

  do
  {
    if (asbool && redraw)
    {
      char scratch[8];
      strcpy(scratch, (boolval)?("yes  "):("no   ") );
      #if (CLIENT_OS == OS_RISCOS)
      if (redraw) /* not the first round */
        riscos_backspace();
      #endif

      ConOut(scratch);

      for (ch = 0; scratch[ch] != 0; ch++)
      {
        #ifdef HAVE_ANSICOMPLIANTTERM
        ConOut("\033" "[1D" );
        #elif (CLIENT_OS == OS_RISCOS)
        if (scratch[ch+1]!=0) /* not the first char */
         riscos_backspace();
        #else
        ConOut("\b");
        #endif
      }
      buffer[0] = ((boolval)?('y'):('n'));
      pos = 1;
      redraw = 0;
    }

    ch = ConInKey(-1);
    exitreq = (CheckExitRequestTriggerNoIO() != 0 ? 1 : 0);

    if (!exitreq)
    {
      if (ch == '\n' || ch == '\r')
      {
        ConOut("\n");
        exitreq = 1;
      }
      else if (asbool)
      {
        if (ch == 'y' || ch == 'Y')
        {
          redraw = (boolistf || !boolval);
          boolistf = 0; boolval = 1;
        }
        else if (ch == 't' || ch == 'T')
        {
          redraw = (!boolistf || !boolval);
          boolistf = boolval = 1;
        }
        else if (ch == 'n' || ch == 'N')
        {
          redraw = (boolistf || boolval);
          boolistf = boolval = 0;
        }
        else if (ch == 'f' || ch == 'F')
        {
          redraw = (!boolistf || boolval);
          boolistf = 1; boolval = 0;
        }
        else
        {
          ConBeep();
        }
      }
      else if (ch == 0x08 || ch == '\177') /* backspace */
      {
        if (pos > 0)
        {
          #ifdef HAVE_ANSICOMPLIANTTERM
          ConOut("\033" "[1D" " " "\033" "[1D");
          #elif (CLIENT_OS == OS_RISCOS)
          riscos_backspace();
          #else
          ConOut("\b \b");
          #endif
          pos--;
        }
      }
      else if (pos < (buflen-1))
      {
        char x[2];
        buffer[pos++] = (char)ch;
        if ((flags & CONINSTR_ASPASSWORD) != 0)
          ch = '*';
        else if (!(isalpha(ch) || isspace(ch) || isdigit(ch) || ispunct(ch)))
          ch = '?';
        x[0]=(char)ch;
        x[1]=0;
        ConOut(x);
      }
    }
  } while (!exitreq);

  ConOut(""); /* flush */
  buffer[pos] = 0;
  return pos; /* length */
}


/* ---------------------------------------------------- */

int ConGetPos( int *col, int *row )  /* zero-based */
{
  if (constatics.initlevel > 0 && constatics.conisatty)
  {
    #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    return w32ConGetPos(col,row);
    #elif (CLIENT_OS == OS_NETWARE)
    unsigned short r, c;
    if (GetPositionOfOutputCursor( &r, &c ) == 0)
    {
      if (row) *row = (int)r;
      if (col) *col = (int)c;
    }
    return 0;
    #elif (CLIENT_OS == OS_DOS)
    return dosCliConGetPos( col, row );
    #elif (CLIENT_OS == OS_OS2)
    USHORT r, c;
    HVIO hvio = 0;
    if (VioGetCurPos( &r, &c, hvio) == 0)
    {
      if (row) *row = (int)r;
      if (col) *col = (int)c;
      return 0;
    }
    #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    return amigaConGetPos(col,row);
    #else
    return ((row == NULL && col == NULL) ? (0) : (-1));
    #endif
  }
  return -1;
}

/* ---------------------------------------------------- */

int ConSetPos( int col, int row )  /* zero-based */
{
  if (constatics.initlevel > 0 && constatics.conisatty)
  {
    #if defined(HAVE_ANSICOMPLIANTTERM)
    printf("\033" "[%d;%dH", row+1, col+1 );
    return 0;
    #elif (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    return w32ConSetPos(col,row);
    #elif (CLIENT_OS == OS_NETWARE)
    gotoxy( ((unsigned short)col), ((unsigned short)row) );
    return 0;
    #elif (CLIENT_OS == OS_DOS)
    return dosCliConSetPos( col, row );
    #elif (CLIENT_OS == OS_OS2)
    HVIO hvio = 0;
    return ((VioSetCurPos(row, col, hvio) != 0)?(-1):(0));
    #else
    return -1;
    #endif
  }
  return -1;
}

/* ---------------------------------------------------- */

int ConGetSize(int *widthP, int *heightP) /* one-based */
{
  int width = 0, height = 0;

  if (constatics.initlevel <= 0 || !constatics.conisatty)
    return -1;

  #if (CLIENT_OS == OS_RISCOS)
  {
    static const int var[3] = { 133, 135, -1 };
    int value[3];
    _kernel_swi_regs regs;

    if (!riscos_in_taskwindow)
    {
      regs.r[0]=(int)var;
      regs.r[1]=(int)value;
      if (_kernel_swi(XOS_Bit|OS_ReadVduVariables, &regs, &regs) == 0)
      {
        height = value[0] - value[1] + 1;
      }
    }
  }
  #elif (CLIENT_OS == OS_DOS)
    if (dosCliConGetSize( &width, &height ) < 0)
      height = width = 0;
  #elif (CLIENT_OS == OS_OS2)
    #if !defined(__EMX__)
    VIOMODEINFO viomodeinfo = {0};
    HVIO   hvio = 0;
    viomodeinfo.cb = sizeof(VIOMODEINFO);
    if(!VioGetMode(&viomodeinfo, hvio))
       {
       height = viomodeinfo.row;
       width = viomodeinfo.col;
       }
    #else
    v_init();
    v_dimen(&width, &height);
    #endif
  #elif (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
    if ( w32ConGetSize(&width,&height) < 0 )
      height = width = 0;
  #elif (CLIENT_OS == OS_NETWARE)
    unsigned short h, w;
    if (GetSizeOfScreen( &h, &w ) == 0)
      height = h; width = w;
  #elif (CLIENT_OS == OS_LINUX) || (CLIENT_OS == OS_SOLARIS) || \
        (CLIENT_OS == OS_SUNOS) || (CLIENT_OS == OS_IRIX) || \
        (CLIENT_OS == OS_HPUX)  || (CLIENT_OS == OS_AIX) || \
        (CLIENT_OS == OS_BEOS) || (CLIENT_OS == OS_NEXTSTEP) || \
        (CLIENT_OS == OS_DEC_UNIX) || (CLIENT_OS == OS_MACOSX) || \
        (CLIENT_OS == OS_DYNIX) || (CLIENT_OS == OS_PS2LINUX) || \
        ( (CLIENT_OS == OS_QNX) && !defined( __QNXNTO__ ) ) || \
        (CLIENT_OS == OS_SCO) || (CLIENT_OS == OS_HAIKU) || \
        (CLIENT_OS == OS_OPENBSD) || \
        ((CLIENT_OS == OS_FREEBSD) && (__FreeBSD__ >= 8)) || \
        (CLIENT_OS == OS_IOS) || (CLIENT_OS == OS_ANDROID)
    /* good for any flavour? */
    struct winsize winsz;
    winsz.ws_col = winsz.ws_row = 0;
    ioctl (fileno(stdout), TIOCGWINSZ, &winsz);
    if (winsz.ws_col && winsz.ws_row){
      width   = winsz.ws_col;
      height  = winsz.ws_row;
    }
  #elif ((CLIENT_OS == OS_FREEBSD) && (__FreeBSD__ < 8)) || \
        (CLIENT_OS == OS_BSDOS) || \
        (CLIENT_OS == OS_NETBSD) || (CLIENT_OS == OS_DRAGONFLY)
    struct ttysize winsz;
    winsz.ts_lines = winsz.ts_cols = winsz.ts_xxx = winsz.ts_yyy = 0;
    ioctl (fileno(stdout), TIOCGWINSZ, &winsz);
    if (winsz.ts_lines && winsz.ts_cols){
      width   = winsz.ts_cols;
      height  = winsz.ts_lines;
    }
  #elif (CLIENT_OS == OS_QNX) && defined(__QNXNTO__)
    tcgetsize(fileno(stdout), &height, &width);
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    amigaConGetSize( &width, &height);
  #elif defined(TIOCGWINSZ)
    #error please add support for TIOCGWINSZ to avoid the following stuff
  #else
  {
    char *envp = getenv( "LINES" );
    if (envp != NULL)
    {
      height = atoi(envp);
      if (height <= 0 || height >= 300)
        height = 0;
    }
    envp = getenv( "COLUMNS" );
    if (envp != NULL)
    {
      width = atoi(envp);
      if (width <= 0 || width >= 300)
        width = 0;
    }
    #if 0
    if (height == 0 && width == 0)
    {
      unsigned int loc = 0;
      char *terminfo_locations[] =
      {
        // grrr... terminfo database location is installation dependent
        // search some standard (!?) locations
        "/usr/share/terminfo",       // ncurses 1.9.9g defaults
        "/usr/local/share/terminfo", //
        "/usr/lib/terminfo",         // Debian 1.3x use this one
        "/usr/share/lib/terminfo",   // ex. AIX (link to /usr/lib)
        "/usr/local/lib/terminfo",   // linux variation
        "/etc/terminfo",             // another linux variation
        "~/.terminfo",               // last resort
        NULL                         // stop tag
      };
      envp = NULL;
      do
      {
        int termerr;
        char ebuf[128];
        if ( envp )
        {
          #if (CLIENT_OS == OS_IRIX)
          putenv(strcat(strcpy(ebuf,"TERMINFO="),envp));
          #else
          setenv( "TERMINFO", envp, 1);
          #endif
        }
        if (setupterm( NULL, 1, &termerr ) != ERR)
        {
          if (termerr == 1)
          {
            height = tigetnum( "lines" );
            if (height <= 0 || height >= 300)
              height = 0;
            width = tigetnum( "columns" );
            if (width <= 0 || width >= 300)
              width = 0;
            if (width != 0 && height != 0)
            {
              #if (CLIENT_OS == OS_IRIX)
              sprintf( ebuf, "LINES=%d", height );
              putenv( ebuf );
              sprintf( ebuf, "COLUMNS=%d", width );
              putenv( ebuf );
              #else
              sprintf( ebuf, "%d", height );
              setenv( "LINES", ebuf, 1);
              sprintf( ebuf, "%d", width );
              setenv( "COLUMNS", ebuf , 1);
              #endif
              break; /* get out of do{}while loop */
            }
          }
        }
        envp = terminfo_locations[loc++];
      } while (envp != NULL);
    }
    #endif
  }
  #endif

  if (height <= 0 || height >= 300)
    height = 25;
  if (width <=0 || width >=300)
    width = 80;

  if (heightP) *heightP = height;
  if (widthP)  *widthP = width;
  return 0;
}

/* ---------------------------------------------------- */

/*
** ConClear() clears the screen.
** Returns -1 if err
*/

int ConClear(void)
{
  if (constatics.initlevel > 0 && constatics.conisatty)
  {
    #if (CLIENT_OS == OS_WIN16) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64)
      return w32ConClear();
    #elif (CLIENT_OS == OS_OS2)
      #ifndef __EMX__
      USHORT attrib = 0x0720; /* white space on black background */
      USHORT row = 0, col = 0;
      HVIO hvio = 0;

      VioScrollUp(0, 0, (USHORT)-1, (USHORT)-1, (USHORT)-1, (PBYTE)&attrib, hvio);
      VioSetCurPos(row, col, hvio);      /* move cursor to upper left */
      return 0;
      #else
      v_clear();
      v_gotoxy(0,0);
      #endif
    #elif (CLIENT_OS == OS_DOS)
      return dosCliConClear();
    #elif (CLIENT_OS == OS_NETWARE)
      clrscr();
      return 0;
    #elif (CLIENT_OS == OS_RISCOS)
      riscos_clear_screen();
      return 0;
    #elif defined(HAVE_ANSICOMPLIANTTERM)
      printf("\033" "[2J" "\033" "[H" "\r       \r" );
      /* ANSI cls  '\r space \r' is in case ansi is not supported */
      return 0;
    #endif
  }
  return -1;
}

/* ---------------------------------------------------- */

