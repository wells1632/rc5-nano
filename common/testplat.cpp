/*
 * Copyright distributed.net 1997-2015 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * This file contains functions used from ./configure
 * Specify 'build_dependencies' as argument
 * (which is all this needs to do anymore)
 *
 * $Id: testplat.cpp,v 1.17 2015/06/27 21:52:52 zebe Exp $
*/
#include <stdio.h>   /* fopen()/fclose()/fread()/fwrite()/NULL */
#include <string.h>  /* strlen()/memmove() */
#include <stdlib.h>  /* malloc()/free()/atoi() */


static int fileexists( const char *filename ) /* not all plats have unistd.h */
{
  FILE *file = fopen( filename, "r" );
  if (file) fclose( file );
  return ( file != NULL );
}

static void __fixup_pathspec_for_locate(char *foundbuf)
{
  foundbuf = foundbuf;
  #if defined(__riscos)
  unsigned int n;
  for (n=0; foundbuf[n]; n++)
  {
    if (foundbuf[n] == '/') /* dirsep */
      foundbuf[n] = '.';
    else if (foundbuf[n] == '.') /* suffix */
      foundbuf[n] = '/';
  }
  #elif defined(amigaos)
  char *ptr;
  if (foundbuf[0] == '.' && foundbuf[1] == '/')
    memmove( foundbuf, &foundbuf[2], strlen(&foundbuf[2])+1 );
  /* suppress "Insert volume" requesters from netbase.cpp */
  if ((ptr = strstr(foundbuf,"multinet_root:")))
    ptr[13] = '_';
  #endif
}

static void __fixup_pathspec_for_makefile(char *foundbuf)
{
  #if defined(__riscos)
  unsigned int n;
  for (n=0; foundbuf[n]; n++)
  {
    if (foundbuf[n] == '.')
      foundbuf[n] = '/';
    else if (foundbuf[n] == '/')
      foundbuf[n] = '.';
  }
  #else
  if (foundbuf[0] == '.' && foundbuf[1] == '/')
    memmove( foundbuf, &foundbuf[2], strlen(&foundbuf[2])+1 );
  #endif
}

static int is_trace_checked(const char *filename)
{
  filename = filename;
#if 0
  if (strcmp(filename,"ogr/x86/ogr-a.cpp") == 0
      || strcmp(filename,"ogr/x86/ogr-b.cpp") == 0) ;
  return 1;
#endif
  return 0;
}

struct name_list {
  struct name_list *next;
  char *name;
};

/* returns 1 if name was found in list, otherwise adds name to list and returns zero */
static int find_or_add(name_list **list, const char *name)
{
  if (!list)
    return -1;
  while (*list) {
    if (strcmp(name, (*list)->name) == 0)
      return 1;
    list = &((*list)->next);
  }
  name_list *next = (name_list *)malloc(sizeof(name_list));
  next->next = NULL;
  next->name = strdup(name);
  *list = next;
  return 0;
}

static void delete_list(name_list **list)
{
  if (list) {
    name_list *curr = *list;
    while (curr) {
      name_list *next = curr->next;
      free(curr->name);
      free(curr);
      curr = next;
    }
    *list = NULL;
  }
}

struct name_list *seen_files = NULL;
struct name_list *seen_deps = NULL;

static unsigned int build_dependencies( const char *cppname, /* ${TARGETSRC} */
                                        const char **include_dirs,
                                        unsigned int count )
{
  char linebuf[512], pathbuf[64], foundbuf[64];
  char *p, *r;
  unsigned int l;
  FILE *file;

  // skip this file if it was parsed before
  if (find_or_add(&seen_files, cppname))
    return count;
  
  file = fopen( cppname, "r" );

  //fprintf(stderr,"cppname='%s', file=%p\n",cppname,file);

  if ( file )
  {
    int debug = is_trace_checked(cppname);
    if (debug)
      fprintf(stderr, "File: %s\n", cppname);

    strcpy( pathbuf, cppname );
    p = strrchr( pathbuf, '/' ); /* input specifiers are always unix paths */
    if ( p == NULL )
      pathbuf[0]=0;
    else *(++p)=0;

    while ( fgets( linebuf, sizeof( linebuf ), file ) != NULL )
    {
      p = linebuf;
      while ( *p == ' ' || *p == '\t' )
        p++;
      if ( *p == '#' && strncmp( p, "#include", 8 ) == 0 &&
           (p[8]==' ' || p[8] =='\t'))
      {
        p+=8;
        while ( *p == ' ' || *p == '\t' )
          p++;
        if ( *p == '\"' || *p == '<' )
        {
          r = linebuf;
          foundbuf[0]= ((*p == '<') ? ('>') : ('\"'));
          while (*(++p) != foundbuf[0] )
            *r++ = *p;
          *r = 0;
          if (debug)
            fprintf(stderr, "'#include %s'\n", linebuf);
          if (linebuf[0])
          {
            strcpy( foundbuf, linebuf );
            /* include specifiers are always unix form */
            //if ( strchr( linebuf, '/' ) == NULL )
            {
              strcpy( foundbuf, pathbuf );
              strcat( foundbuf, linebuf );
              l = 0;
              for (;;)
              {
                char origbuf[sizeof(foundbuf)];
                strcpy(origbuf, foundbuf);
                __fixup_pathspec_for_locate(foundbuf);
                if (debug)
                  fprintf(stderr, "%d) '%s'%s\n", l, foundbuf, fileexists(foundbuf)?" *":"");
                if (fileexists( foundbuf ))
                {
                  count = build_dependencies( origbuf, include_dirs, count );
                  break;
                }
                if (!include_dirs)
                  break;
                if (!include_dirs[l])
                  break;
                strcpy( foundbuf, include_dirs[l] );
                if (foundbuf[strlen(foundbuf)-1] != '/') /* always unix */
                  strcat( foundbuf, "/" ); /* always unix */
                strcat( foundbuf, linebuf );
                l++;
              }
            }
            if ( fileexists( foundbuf ) )
            {
              __fixup_pathspec_for_makefile(foundbuf);
              if (find_or_add(&seen_deps, foundbuf) == 0) {
                printf( "%s%s", ((count!=0) ? (" ") : ("")), foundbuf );
                count++;
              }
            }
          }
        }
      }
    }
    fclose(file);
  }
  return count;
}

static const char **get_include_dirs(int argc, char *argv[])
{
  char **idirs; int i;
  unsigned int bufsize = 0;
  for (i = 0; i < argc; i++)
    bufsize += strlen(argv[i])+5;
  idirs = (char **)malloc((argc*sizeof(char *)) + bufsize);
  if (idirs)
  {
    int numdirs = 0, in_i_loop = 0;
    char *buf = (char *)idirs;
    buf += (argc * sizeof(char *));
    for (i = 1; i < argc; i++)
    {
      const char *d = argv[i];
      //fprintf(stderr,"d='%s'\n",d);
      int is_i = in_i_loop;
      in_i_loop = 0;
      if (*d == '-' && d[1] != 'I')
        is_i = 0;
      else if (*d == '-' && d[1] == 'I')
      {
        d += 2;
        is_i = (*d != '\0');
        in_i_loop = !is_i;
      }
      if (is_i)
      {
        while (*d)
        {
          const char *s = d;
          while (*d && *d != ':')
            d++;
          if (d != s)
          {
            idirs[numdirs++] = buf;
            while (s < d)
              *buf++ = *s++;
            if ((*--s) != '/')
              *buf++ = '/';
            *buf++ = '\0';
            //fprintf(stderr,"\"-I%s\"\n", idirs[numdirs-1]);
          }
          while (*d == ':')
            d++;
        } /* while (*d) */
      }  /* if (is_i) */
    } /* for (i = 1; i < argc; i++) */
    idirs[numdirs] = (char *)0;
  } /* if (idirs) */
  return const_cast<const char**>(idirs);
}


int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    fprintf(stderr,"Specify 'build_dependencies' as argument.\n");
    return -1;
  }
  if (strcmp(argv[1], "build_dependencies" )== 0)
  {
    const char **idirs;
    //fprintf(stderr,"%s 1\n", argv[2] );
    idirs = get_include_dirs(argc,argv);
    //fprintf(stderr,"%s 2\n", argv[2] );
    build_dependencies( argv[2], idirs, 0 );
    printf("\n");
    //fprintf(stderr,"%s 3\n", argv[2] );
    if (idirs)
      free((void *)idirs);
    delete_list(&seen_files);
    delete_list(&seen_deps);
  }
  return 0;
}
