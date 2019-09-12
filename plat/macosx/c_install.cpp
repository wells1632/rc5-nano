/*
 * Copyright distributed.net 2002-2004 - All Rights Reserved
 * For use in distributed.net projects only.
 * Any other distribution or use of this source violates copyright.
 *
 * -install and -uninstall support for MacOS X
 * A test main() is at the end of this file.
 *
 * int macosx_uninstall(const char *argv0, int quietly);
 * int macosx_install(const char *argv0, int argc, const char *argv[],
 *                      int quietly); // argv[1..(argc-1)] are boot options
 * Both functions return 0 on success, else -1.
 *
 * For more information about Mac OS X StartupItems go to http://www.opensource
 * .apple.com/projects/documentation/howto/html/SystemStarter_HOWTO.html
 *
 * See also
 * http://developer.apple.com/documentation/MacOSX/Conceptual/BPSystemStartup/Concepts/BootProcess.html
 *
 * $Id: c_install.cpp,v 1.2 2007/10/22 16:48:31 jlawson Exp $
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <errno.h>

#include "unused.h"     /* DNETC_UNUSED_* */

#define STARTUPITEMS_HOST "/Library/StartupItems"
#define STARTUPITEMS_DIR (STARTUPITEMS_HOST"/dnetc")
#define START_SCRIPT_PATH (STARTUPITEMS_HOST"/dnetc/dnetc")
#define START_PARAM_PATH (STARTUPITEMS_HOST"/dnetc/StartupParameters.plist")

int macosx_uninstall(const char *argv0, int quietly);
int macosx_install(const char *argv0, int argc, const char *argv[],
                  int quietly); /* argv[1..(argc-1)] are boot options */


/* ----------------------------------------------------------------------- */

static int create_start_script(const char *script_name,
                               const char *basename,
                               const char *appname,
                               int argc, const char *argv[],
                               int quietly)
{
  int i, n;
  FILE *file;
  const char *p;

  file = fopen(script_name, "w");
  if (!file) {
    if (!quietly) {
      fprintf(stderr, "%s:\tUnable to create '%s'\n\t%s\n",
                     basename, script_name, strerror(errno) );
    }
    return -1;
  }

  fprintf(file,
    "#!/bin/sh\n"
    "#\n"
    "# distributed.net client startup script.\n"
    "# generated by '%s -install'. Use '%s -uninstall' to\n"
    "# stop the client from being started automatically.\n"
    "#\n"
    "# Don't forget to change buffer/.ini file perms if you wish to run suid'd.\n"
    "\n"
    ". /etc/rc.common\n"
    "\n"
    "CLIENT_DIR=\"", basename, basename );

  p = strrchr(appname,'/');
  if (!p) {
    fprintf(file, "./");
  }
  else {
    while (appname < p)
      fputc(*appname++, file);
    appname = p + 1;
  }

  fprintf(file,"\"\n"
    "CLIENT_FNAME=\"%s\"\n"
    "CLIENT_OPTS=\"", appname );
  n = 0;

  /*
  ** Parse optional switches
  */

  for (i = 1; i < argc; i++) {
    p = argv[i];
    if (*p == '-' && p[1] == '-')
      p++;
    if (strcmp(p, "-quiet") == 0 || strcmp(p, "-hide") == 0)
      continue;
    if (strcmp(p, "-ini") == 0) {
      if (i == argc - 1)
        break;
    }
    if (n++)
      fprintf(file, " ");
    if (strchr(argv[i], ' ') || strchr(argv[i], '\t'))
      fprintf(file, "\'%s\'", p);
    else
      fprintf(file, "%s", p);
  }

  fprintf( file, "\" # specify any optional switches for dnetc\n"
    "CLIENT_RUNAS=\"%s\" # specifiy the owner of the dnetc process\n", getlogin());

  /*
  ** Write SystemStarter's entry points (functions)
  */

  fprintf( file, "\nStartService ()\n"
    "{\n"
    "\tif [ -x \"${CLIENT_DIR}/${CLIENT_FNAME}\" ]; then\n"
    "\t  ConsoleMessage \"Starting distributed.net client\"\n"
    "\t  # stop any other instances that may be running.\n"
    "\t  \"${CLIENT_DIR}/${CLIENT_FNAME}\" -quiet -shutdown\n"
    "\t  cd \"${CLIENT_DIR}\"\n"
    "\t  sudo -u ${CLIENT_RUNAS} ./${CLIENT_FNAME} -quiet ${CLIENT_OPTS}\n"
    "\t  cd -\n"
    "\telse\n"
    "\t  ConsoleMessage \"Cannot locate distributed.net client\"\n"
    "\tfi\n"
    "}\n"
  );
  fprintf( file, "\nStopService ()\n"
    "{\n"
    "\tif [ -x \"${CLIENT_DIR}/${CLIENT_FNAME}\" ]; then\n"
    "\t  ConsoleMessage \"Stopping distributed.net client\"\n"
    "\t  \"${CLIENT_DIR}/${CLIENT_FNAME}\" -quiet -shutdown\n"
    "\telse\n"
    "\t  ConsoleMessage \"Cannot locate distributed.net client\"\n"
    "\tfi\n"
    "}\n"
  );
  fprintf( file, "\nRestartService ()\n"
    "{\n"
    "\tif [ -x \"${CLIENT_DIR}/${CLIENT_FNAME}\" ]; then\n"
    "\t  ConsoleMessage \"Restarting distributed.net client\"\n"
    "\t  \"${CLIENT_DIR}/${CLIENT_FNAME}\" -quiet -restart\n"
    "\telse\n"
    "\t  ConsoleMessage \"Cannot locate distributed.net client\"\n"
    "\tfi\n"
    "}\n"
  );
  fprintf( file, "\nRunService \"$1\"\n");

  fchmod( fileno(file), S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH); /* -rwxr-xr-x */
  fclose(file);
  return 0;
}


static int create_startup_params_file(const char *stparams_filename,
                                      const char *basename, int quietly)
{
  FILE *file = fopen(stparams_filename, "w");
  if (!file) {
    if (!quietly) {
      fprintf(stderr, "%s:\tUnable to create '%s'\n\t%s\n",
                       basename, stparams_filename, strerror(errno) );
    }
    return -1;
  }

  fprintf(file,
    "{\n"
    "  Description     = \"distributed.net client\";\n"
    "  Provides        = (\"dnetc\");\n"
    "  Requires        = (\"Disks\", \"NetInfo\", \"Resolver\");\n"
    "  Uses            = (\"Network\", \"Network Time\");\n"
    "  OrderPreference = \"None\";\n"
    "}\n" );

  fchmod(fileno(file), S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH); /* -rw-r--r-- */
  fclose(file);
  return 0;
}


/* ----------------------------------------------------------------------- */

static const char *get_basename(const char *argv0)
{
  const char *p = strrchr(argv0, '/');
  return (p) ? (p + 1) : argv0;
}

static int createDir(const char *path)
{
  int rc = mkdir(path, 0755);
  if (rc == 0) {
    chmod(path, 0755);      // BUG: Mac OS 10.1.3 always creates directories
                            // with flags drwxr-xr-x
    return 1;               // Directory created.
  }
  else if (errno == EEXIST)
    return 0;               // The directory already exists

  return -1;                // Error.
}


static int get_ftype(const char *fname)
{
  struct stat statblk;
  if (lstat(fname, &statblk) != 0) {
    if (stat(fname, &statblk) != 0)
      return -1;
  }
  if ((statblk.st_mode & S_IFMT) == S_IFLNK)
    return 'l';
  if ((statblk.st_mode & S_IFMT) == S_IFDIR)
    return 'd';
  return 'f';
}

static int recursive_delete(const char *basename,
                            const char *basepath, int quietly)
{
  int rc = 0;
  struct dirent *dp;
  DIR *dir;

  DNETC_UNUSED_PARAM(basepath);

  dir = opendir(".");
  if (!dir) {
    if (!quietly) {
      fprintf(stderr, "%s:\tUnable to list directory entries\n\t%s\n",
                      basename, strerror(errno) );
    }
    rc = -1;
  }
  else {
    while ((dp = readdir(dir)) != ((struct dirent *) 0)) {
      if (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0) {
        int typ = get_ftype(dp->d_name);
        rc = -1;
        if (typ == -1) {
          if (!quietly) {
            fprintf(stderr, "%s:\tUnable to stat %s\n\t%s\n",
                            basename, dp->d_name, strerror(errno) );
          }
        }
        else if (typ == 'f' || typ == 'l') {
          rc = unlink(dp->d_name);
          if (rc != 0 && !quietly) {
            fprintf(stderr, "%s:\tUnable to unlink %s\n\t%s\n",
                            basename, dp->d_name, strerror(errno) );
          }
        }
        else if (typ == 'd') {
          rc = chdir(dp->d_name);
          if (rc != 0 && !quietly) {
             fprintf(stderr, "%s:\tUnable to enumerate %s\n\t%s\n",
                             basename, dp->d_name, strerror(errno) );
          }
          if (rc == 0) {
            rc = recursive_delete(basename, dp->d_name,quietly);
          }
          if (rc == 0) {
            rc = chdir("..");
            if (rc != 0 && !quietly) {
              fprintf(stderr, "%s:\tUnable to chdir ..\n\t%s\n",
                              basename, strerror(errno) );
            }
          }
          if (rc == 0) {
            rc = rmdir(dp->d_name);
            if (rc != 0 && !quietly) {
              fprintf(stderr, "%s:\tUnable to rmdir %s\n\t%s\n",
                              basename, dp->d_name, strerror(errno) );
            }
          }
        }
        if (rc != 0)
          break;
      } /* if (strcmp(dp->d_name,".") != 0 && strcmp(dp->d_name,"..") != 0) */
    } /* while (readdir) */
    closedir(dir);
  }

  return rc;
}


/* ----------------------------------------------------------------------- */

int macosx_install(const char *argv0, int argc, const char *argv[],
                  int quietly) /* argv[1..(argc-1)] are boot options */
{
  int rc = -1;
  char fullpath[MAXPATHLEN];
  const char *basename = get_basename(argv0);
  const char *appname = realpath(argv0, fullpath);

  if (!appname) {
    if (!quietly) {
      fprintf(stderr, "%s:\tUnable to determine full path to '%s'\n",
                     basename, argv0 );
    }
  }
  else {
    const char *basepath = STARTUPITEMS_DIR;
    const char *hostpath = STARTUPITEMS_HOST;
    int dir_created = 0;

    rc = createDir(hostpath);
    if (rc < 0 && !quietly) {
      fprintf(stderr, "%s:\tUnable to create directory '%s'\n\t%s\n",
                     basename, hostpath, strerror(errno) );
    }

    if (rc >= 0) {
      rc = createDir(basepath);
      if (rc > 0)
        dir_created = 1;
      else if (rc < 0 && !quietly)
        fprintf(stderr, "%s:\tUnable to create directory '%s'\n\t%s\n",
                        basename, basepath, strerror(errno) );
    }

    if (rc >= 0) {
      rc = -1;
      if (create_start_script(START_SCRIPT_PATH,
                              basename, appname, argc, argv, quietly) == 0)
      {
        int reinstalled = (access(START_PARAM_PATH, 0) == 0);
        rc = 0;
        if (!reinstalled)
          rc = create_startup_params_file(START_PARAM_PATH, basename, quietly);
        if (rc != 0)
          unlink(START_SCRIPT_PATH);

        if (rc == 0 && !quietly) {
          fprintf(stderr,
            "%s:\tThe client has been sucessfully %sinstalled\n"
            "\tand will be automatically started on system boot.\n"
            "\t*** Please ensure that the client is configured ***\n",
             basename, ((!reinstalled) ? ("") : ("re-")) );
        }
      }
      if (rc != 0 && dir_created)
        rmdir(basepath);
    }
  }
  return rc;
}


int macosx_uninstall(const char *argv0, int quietly)
{
  int rc = -1;
  const char *basename = get_basename(argv0);
  const char *basepath = STARTUPITEMS_DIR;
  char curdir[PATH_MAX];

  if (getcwd(curdir, sizeof(curdir)) != 0)
    strcpy(curdir, "..");

  if (chdir(basepath) != 0) {
    if (errno == ENOENT) {
      if (!quietly) {
        fprintf(stderr, 
          "%s:\tThe client was not previously installed and therefore\n"
          "\tcannot be uninstalled.\n", basename );
      }
      rc = 0; /* treat as success */
    }
    else if (!quietly) {
      fprintf(stderr, "%s:\tUnable to change directory to '%s'\n\t%s\n",
                      basename, basepath, strerror(errno));
      }
  }
  else {
    rc = recursive_delete(basename, basepath, quietly);
    chdir(curdir);
    if (rc == 0) {
      if (rmdir(basepath) != 0) {
        rc = -1;
        if (!quietly) {
          fprintf(stderr, "%s:\tUnable to remove directory '%s'\n\t%s\n",
                          basename, basepath, strerror(errno));
        }
      }
      else if (!quietly) {
        fprintf(stderr,
           "%s:\tThe client has been sucessfully uninstalled and \n"
           "\twill no longer be automatically started on system boot.\n",
            basename );
      }
    }
  }
  return rc;
}


/* ----------------------------------------------------------------------- */

#if 0
int main(int argc,char *argv[])
{
  if (argc > 1 && strcmp(argv[1],"-uninstall")==0)
    return macosx_uninstall(argv[0],0);
  else if (argc == 1)
    return macosx_install(argv[0], argc, (const char **)argv, 0);
  return 0;
}
#endif
