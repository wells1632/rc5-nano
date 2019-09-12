/*
 * Copyright distributed.net 1997-2008 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * Created by Cyrus Patel <cyp@fb14.uni-mainz.de> to be able to throw
 * away some very ugly hackery in buffer open code.
 */

/*! \file
 * This module contains functions for setting the "working directory"
 * and pathifying a filename that has no dirspec. Functions need to be
 * initialized from main() with InitWorkingDirectoryFromSamplePaths();
 * [ ...( inipath, apppath) where inipath should not have been previously
 * merged from argv[0] + default, although it doesn't hurt if it has.]
 *
 * The "working directory" is assumed to be the app's directory, unless the
 * ini filename contains a dirspec, in which case the path to the ini file
 * is used. The exception here is win32/win16 which, for reasons of backward
 * compatability, always use the app's directory.
 *
 * GetFullPathForFilename() is ideally intended for use in (or just prior to)
 * a call to fopen(). This obviates the necessity of having to pre-parse
 * filenames or maintain duplicate filename buffers. In addition, each
 * platform has its own code sections to avoid cross-platform assumptions
 * altogether.
*/
const char *pathwork_cpp(void) {
return "@(#)$Id: pathwork.cpp,v 1.26 2008/04/11 06:29:29 jlawson Exp $"; }

// #define TRACE

#include "baseincs.h"
#include "pathwork.h"
#include "util.h"      /* trace, DNETC_UNUSED_* */

#if defined(__unix__)
  #include <pwd.h>       /* getpwnam(), getpwuid(), struct passwd */
  #define HAVE_UNIX_TILDE_EXPANSION
#endif

#if (CLIENT_OS == OS_RISCOS)
#define MAX_FULLPATH_BUFFER_LENGTH (1024)
#else
#define MAX_FULLPATH_BUFFER_LENGTH (512)
#endif

/* ------------------------------------------------------------------------ */

//! Get the offset of the filename component in fully qualified path.
/*!
 * This method was previously called IsFilenamePathified().
 *
 * \param fullpath Buffer containing a possibly fully-qualified filename.
 * \return Returns the offset within the buffer of the start of the 
 *     filename portion of the path.  Returns 0 on error.
 */
unsigned int GetFilenameBaseOffset( const char *fullpath )
{
  char *slash;
  if ( !fullpath )
    return 0;
  #if (CLIENT_OS == OS_VMS)
    slash = strrchr( fullpath, ':' );
    char *slash2 = strrchr( fullpath, '$' );
    if (slash2 > slash) slash = slash2;
  #elif (CLIENT_OS == OS_RISCOS)
    slash = strrchr( fullpath, '.' );
  #elif (CLIENT_OS == OS_DOS) || (CLIENT_OS == OS_WIN16) || \
    (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_OS2)
    slash = strrchr( (char*) fullpath, '\\' );
    char *slash2 = strrchr( (char*) fullpath, '/' );
    if (slash2 > slash) slash = slash2;
    slash2 = strrchr( (char*) fullpath, ':' );
    if (slash2 > slash) slash = slash2;
  #elif (CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_NETWARE6)
    slash = strrchr( fullpath, '\\' );
    char *slash2 = strrchr( fullpath, '//' );
    if (slash2 > slash) slash = slash2;
    slash2 = strrchr( fullpath, ':' );
    if (slash2 > slash) slash = slash2;
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
    slash = strrchr( fullpath, '/' );
    char *slash2 = strrchr( fullpath, ':' );
    if (slash2 > slash) slash = slash2;
  #else
    slash = (char*)strrchr( fullpath, '/' );
  #endif
  return (( slash == NULL ) ? (0) : (( slash - fullpath )+1) );
}

/* ------------------------------------------------------------------------ */

static const char *__finalize_fixup(char *path, unsigned int maxlen)
{
  DNETC_UNUSED_PARAM(maxlen);

#if defined(HAVE_UNIX_TILDE_EXPANSION)
  if (*path == '~')
  {
    char username[64];
    unsigned int usernamelen = 0;
    const char *homedir = (const char *)0;
    char *rempath = &path[1];
    while (*rempath && *rempath != '/' &&
           usernamelen < (sizeof(username)-1))
    {
      username[usernamelen++] = *rempath++;
    }
    if (usernamelen < (sizeof(username)-1))
    {
      struct passwd *pw = (struct passwd *)0;
      username[usernamelen] = '\0';
      if (usernamelen == 0)
        pw = getpwuid(geteuid());
      else
        pw = getpwnam(username);
      if (pw)
        homedir = pw->pw_dir;
    }
    if (homedir)
    {
      unsigned int dirlen = strlen(homedir);
      unsigned int remlen = strlen(rempath);
      if (*rempath == '/' && dirlen > 0 && homedir[dirlen-1] == '/')
      {
        rempath++;
        remlen--;
      }
      if ((remlen+1+dirlen+1) < maxlen)
      {
        memmove( &path[dirlen], rempath, remlen+1 );
        memcpy( &path[0], homedir, dirlen);
      }
    }
  }
#endif
  return path;
}


static char __cwd_buffer[MAX_FULLPATH_BUFFER_LENGTH+1];
static int __cwd_buffer_len = -1; /* not initialized */

//! Compute the "working directory" of the client.
/*!
 * The "working directory" is the executable's directory, unless the ini filename
 * contains a dirspec, in which case the path to the ini file is used.  The
 * computed directory is stored in a static variable for rapid retrieval later.
 *
 * \param inipath
 * \param apppath
 * \return Always returns zero.
 * \sa GetWorkingDirectory
 */
int InitWorkingDirectoryFromSamplePaths( const char *inipath, const char *apppath )
{
  TRACE_OUT((0,"ini: %s app: %s \n",inipath,apppath));

  if ( inipath == NULL ) inipath = "";
  if ( apppath == NULL ) apppath = "";

  __cwd_buffer[0] = '\0';
  #if (CLIENT_OS == OS_VMS)
  {
    strcpy( __cwd_buffer, inipath );
    char *slash, *bracket, *dirend;
    slash = strrchr(__cwd_buffer, ':');
    bracket = strrchr(__cwd_buffer, ']');
    dirend = (slash > bracket ? slash : bracket);
    if (dirend == NULL && apppath != NULL && strlen( apppath ) > 0)
    {
      strcpy( __cwd_buffer, apppath );
      slash = strrchr(__cwd_buffer, ':');
      bracket = strrchr(__cwd_buffer, ']');
      dirend = (slash > bracket ? slash : bracket);
    }
    if (dirend != NULL) *(dirend+1) = 0;
    else __cwd_buffer[0] = 0;  //current directory is also always the apps dir
  }
  #elif (CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_NETWARE6)
  {
    strcpy( __cwd_buffer, inipath );
    char *slash = strrchr(__cwd_buffer, '/');
    char *slash2 = strrchr(__cwd_buffer, '\\');
    if (slash2 > slash) slash = slash2;
    if ( slash == NULL )
    {
      if ( strlen( __cwd_buffer ) < 2 || __cwd_buffer[1] != ':' ) /* dos partn */
      {
        strcpy( __cwd_buffer, apppath );
        slash = strrchr(__cwd_buffer, '/');
        slash2 = strrchr(__cwd_buffer, '\\');
        if (slash2 > slash) slash = slash2;
      }
      if ( slash == NULL && strlen( __cwd_buffer ) >= 2 && __cwd_buffer[1] == ':' )
        slash = &( __cwd_buffer[1] );
    }
    if (slash != NULL) *(slash+1) = 0;
    else __cwd_buffer[0] = 0;
  }
  #elif (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN16) || ( (CLIENT_OS == OS_OS2) && defined(__EMX__) )
  {
    strcpy( __cwd_buffer, inipath );
    char *slash = strrchr(__cwd_buffer, '/');
    char *slash2 = strrchr(__cwd_buffer, '\\');
    if (slash2 > slash) slash = slash2;
    slash2 = strrchr(__cwd_buffer, ':');
    if (slash2 > slash) slash = slash2;
    if ( slash == NULL )
    {
      strcpy( __cwd_buffer, apppath );
      slash = strrchr(__cwd_buffer, '/');
      slash2 = strrchr(__cwd_buffer, '\\');
      if (slash2 > slash) slash = slash2;
      slash2 = strrchr(__cwd_buffer, ':');
      if (slash2 > slash) slash = slash2;
    }
    if (slash != NULL) *(slash+1) = 0;
    else __cwd_buffer[0] = 0;
  }
  #elif (CLIENT_OS == OS_DOS) || ( (CLIENT_OS == OS_OS2) && !defined(__EMX__) )
  {
    strcpy( __cwd_buffer, inipath );
    char *slash = strrchr(__cwd_buffer, '/');
    char *slash2 = strrchr(__cwd_buffer, '\\');
    if (slash2 > slash) slash = slash2;
    slash2 = strrchr(__cwd_buffer, ':');
    if (slash2 > slash) slash = slash2;
    if ( slash == NULL )
    {
      strcpy( __cwd_buffer, apppath );
      slash = strrchr(__cwd_buffer, '\\');
    }
    if ( slash == NULL )
    {
      __cwd_buffer[0] = __cwd_buffer[ sizeof( __cwd_buffer )-2 ] = 0;
      if ( getcwd( __cwd_buffer, sizeof( __cwd_buffer )-2 )==NULL )
        strcpy( __cwd_buffer, ".\\" ); //don't know what else to do
      else if ( __cwd_buffer[ strlen( __cwd_buffer )-1 ] != '\\' )
        strcat( __cwd_buffer, "\\" );
    }
    else
    {
      *(slash+1) = 0;
      if ( ( *slash == ':' ) && ( strlen( __cwd_buffer )== 2 ) )
      {
        char buffer[256];
        buffer[0] = buffer[ sizeof( buffer )-1 ] = 0;
        #if (defined(__WATCOMC__))
        {
          unsigned int drive1, drive2, drive3;
          _dos_getdrive( &drive1 );   /* 1 to 26 */
          drive2 = ( toupper( *__cwd_buffer ) - 'A' )+1;  /* 1 to 26 */
          _dos_setdrive( drive2, &drive3 );
          _dos_getdrive( &drive3 );
          if (drive2 != drive3 || getcwd( buffer, sizeof(buffer)-1 )==NULL)
            buffer[0]=0;
          _dos_setdrive( drive1, &drive3 );
        }
        #else
          #error FIXME: need to get the current directory on that drive
          //strcat( __cwd_buffer, ".\\" );  does not work
        #endif
        if (buffer[0] != 0 && strlen( buffer ) < (sizeof( __cwd_buffer )-2) )
        {
          strcpy( __cwd_buffer, buffer );
          if ( __cwd_buffer[ strlen( __cwd_buffer )-1 ] != '\\' )
            strcat( __cwd_buffer, "\\" );
        }
      }
    }
  }
  #elif ( CLIENT_OS == OS_RISCOS )
  {
    const char *runpath = NULL;
    _kernel_swi_regs regs;

    if (*inipath == '\0')
    {
      inipath = apppath;
      runpath = "Run$Path";
    }
    regs.r[0]=37;
    regs.r[1]=(int)inipath;
    regs.r[2]=(int)__cwd_buffer;
    regs.r[3]=(int)runpath;
    regs.r[4]=NULL;
    regs.r[5]=sizeof __cwd_buffer;
    _kernel_swi(OS_FSControl, &regs, &regs);

    char *slash = strrchr( __cwd_buffer, '.' );
    if ( slash != NULL )
      *(slash+1) = 0;
    else __cwd_buffer[0]=0;
  }
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  {
    strcpy( __cwd_buffer, inipath );
    char *slash = strrchr(__cwd_buffer, ':');
    char *slash2 = strrchr(__cwd_buffer, '/');
    if (slash2 > slash) slash = slash2;
    if (slash == NULL && apppath != NULL && strlen( apppath ) > 0)
    {
      strcpy( __cwd_buffer, apppath );
      slash = strrchr(__cwd_buffer, ':');
    }
    if (slash != NULL) *(slash+1) = 0;
    else __cwd_buffer[0] = 0; // Means we're started from the dir the things are in...
  }
  #else
  {
    strcpy( __cwd_buffer, inipath );
    char *slash = strrchr( __cwd_buffer, '/' );
    if (slash == NULL)
    {
      strcpy( __cwd_buffer, apppath );
      slash = strrchr( __cwd_buffer, '/' );
    }
    if ( slash != NULL )
      *(slash+1) = 0;
    else
      strcpy( __cwd_buffer, "./" );
    __finalize_fixup( __cwd_buffer, sizeof(__cwd_buffer) );
  }
  #endif
  __cwd_buffer_len = strlen( __cwd_buffer );

  TRACE_OUT((0,"GetFullPathForFilename: Working directory is \"%s\"\n", __cwd_buffer));
  #ifdef DEBUG_PATHWORK
  printf( "Working directory is \"%s\"\n", __cwd_buffer );
  #endif
  return 0;
}

/* --------------------------------------------------------------------- */

//! Check if a filename is fully-qualified.
/*!
 * \param fname Buffer containing the filename to check.
 * \return Returns 0 if not fully-qualified, or 1 if so.
 */
static int __is_filename_absolute(const char *fname)
{
  #if (CLIENT_OS == OS_VMS)
  return (*fname == ':');
  #elif (CLIENT_OS == OS_RISCOS)
  return (*fname == '.');
  #elif (CLIENT_OS == OS_DOS) || (CLIENT_OS == OS_WIN16) || \
      (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_OS2)
  return (*fname == '\\' || *fname == '/' || (*fname && fname[1]==':'));
  #elif (CLIENT_OS == OS_NETWARE) || (CLIENT_OS == OS_NETWARE6)
  return (*fname == '\\' || *fname == '/' || (strchr(fname,':')));
  #elif (CLIENT_OS == OS_AMIGAOS) || (CLIENT_OS == OS_MORPHOS)
  return (strchr(fname,':') != NULL);
  #else
  return (*fname == '/');
  #endif
}

/* --------------------------------------------------------------------- */

//! Get the "working directory" of the client.
/*!
 * The returned directory will include a trailing directory separator.
 * This may be relative to system's current directory, and not
 * fully-qualfied.  The InitWorkingDirectoryFromSamplePaths() function
 * must have been already called.
 *
 * \param buffer Buffer to fill with the working directory.
 * \param maxlen Maximum size of the buffer, in bytes.
 * \return Returns NULL on error, otherwise the buffer that was supplied.
 * \sa InitWorkingDirectoryFromSamplePaths
 */
const char *GetWorkingDirectory( char *buffer, unsigned int maxlen )
{
  if ( buffer == NULL )
    return buffer; //could do a malloc() here. naah.
  else if ( maxlen == 0 )
    return "";
  else if ( __cwd_buffer_len <= 0 ) /* not initialized or zero len */
    buffer[0] = 0;
  else if ( ((unsigned int)__cwd_buffer_len) > maxlen )
    return NULL;
  else
    strcpy( buffer, __cwd_buffer );
  /* __cwd_buffer is already fixed up, so __finalize_fixup() is not needed */
  return buffer;
}

/* --------------------------------------------------------------------- */

static char __path_buffer[MAX_FULLPATH_BUFFER_LENGTH+2];

//! Compute a slightly more-qualified path for a relative file.
/*!
 * "GetFullPathForFilename" is a slight misnomer; it should be called
 * "GetWorkingPathForFilename" to reflect that returned paths may not
 * always be fully-qualified if the "working directory" was not.
 *
 * - In general, returns <fname> if <fname> is absolute
 * - otherwise returns <X> + <fname>
 * - where <X> is "" if <fname> is not just a plain basename (has a dir spec)
 * - where <X> is __cwd_buffer + <dirsep> if <fname> is just a plain basename
 *
 * \param filename Input buffer containing the relative filename.
 *    If the filename is already absolute, then that is the result.
 * \return Returns a pointer to a static buffer with the
 *    more-qualified version of the path.  Returns "" on error.
 *    Guaranteed not to return NULL.
 */
const char *GetFullPathForFilename( const char *filename )
{
  const char *outpath;

  if ( filename == NULL )
    outpath = "";
  else if (filename >= &__path_buffer[0] &&
           filename <= &__path_buffer[sizeof(__path_buffer)-1])
    outpath = filename; /* consider already parsed */
  else if (*filename == '\0' || __is_filename_absolute(filename))
    outpath = filename;
  else if (strlen(filename) >= sizeof(__path_buffer))
    outpath = "";
  else if ( GetFilenameBaseOffset( filename ) != 0 ) /* already pathified */
    outpath = __finalize_fixup(strcpy(__path_buffer, filename),sizeof(__path_buffer));
  else if ( !GetWorkingDirectory( __path_buffer, sizeof( __path_buffer ) ) )
    outpath = "";
  else if (strlen(__path_buffer)+strlen(filename) >= sizeof(__path_buffer))
    outpath = "";
  else
    outpath = __finalize_fixup(strcat(__path_buffer, filename),sizeof(__path_buffer));

  TRACE_OUT((0,"GetFullPathForFilename: got \"%s\" returning \"%s\"\n", filename, outpath));
  #ifdef DEBUG_PATHWORK
  printf( "got \"%s\" returning \"%s\"\n", filename, outpath );
  #endif

  return ( outpath );
}

/* --------------------------------------------------------------------- */

/*!
 * - In general, returns <fname> if <fname> is absolute
 * - otherwise returns <dir> + <dirsep> + <fname>
 * - if <dir> is NULL, use __cwd_buffer as <dir>;
 * - if <dir> is "", use ""
 * - if <fname> is NULL, use "" as <fname>
 * 
 * \param fname
 * \param dir
 * \return
 */
const char *GetFullPathForFilenameAndDir( const char *fname, const char *dir )
{
  if (!fname)
    fname = "";
  else if (__is_filename_absolute(fname))
    return fname;
  if (!dir)
    dir = ((__cwd_buffer_len > 0) ? (__cwd_buffer) : "");

  if ( ( strlen( dir ) + 1 + strlen( fname ) + 1 ) >= sizeof(__path_buffer) )
    return "";
  strcpy( __path_buffer, dir );
  if ( strlen( __path_buffer ) > GetFilenameBaseOffset( __path_buffer ) )
  {
    /* dirname is not terminated with a directory separator - so add one */
    #if (CLIENT_OS == OS_VMS)
      strcat( __path_buffer, ":" );
    #elif (CLIENT_OS == OS_RISCOS)
      strcat( __path_buffer, "." );
    #elif (CLIENT_OS == OS_DOS) || (CLIENT_OS == OS_WIN16) || \
      (CLIENT_OS == OS_WIN32) || (CLIENT_OS == OS_WIN64) || (CLIENT_OS == OS_OS2)
      strcat( __path_buffer, "\\" );
    #else
      strcat( __path_buffer, "/" );
    #endif
  }
  strcat( __path_buffer, fname );
  return __finalize_fixup(__path_buffer, sizeof(__path_buffer));
}

