/* konsole_grantpty - helper program for for konsole's grantpty. */

/* This program is based on the glibc2.1 pt_chmod.
 * It was pulled out from there since both Linux
 * distributors and other OSes are not able to make
 * use of the glibc for different reasons.
 *
 * THIS IS A ROOT SUID PROGRAM
 *
 * Things work as following:
 * 
 * In konsole we open a master pty. This can be opened
 * done by at most one process. Prior to opening the
 * master pty, the slave pty cannot be opened. Then, in
 * grantpty, we fork to this program. The trick is, that
 * the parameter is passes as a file handle, which cannot
 * be faked, so that we get a secure setuid root chmod/chown
 * with this program.
 *
 * We have to chown/chmod the slave pty to prevent eavesdroping.
 * 
 * Contributed by Zack Weinberg <zack@rabi.phys.columbia.edu>, 1998.
 * Copyright (c) 1999 by Lars Doelle <lars.doelle@on-line.de>.
 * GPL applies.
 */

#include <sys/types.h>
#include <errno.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/param.h>
#ifdef BSD
#  define BSD_PTY_HACK
#  include <paths.h>
#  include <dirent.h>
#endif

#define PTY_FILENO 3    /* keep in sync with grantpty */
#define TTY_GROUP "tty"

int main (int argc, char *argv[])
{
  char*         pty;
  struct stat   st;
  struct group* p;
  gid_t         gid;
  uid_t         uid;
  mode_t        mod;
  char*         tty;

  /* check preconditions **************************************************/
  if (argc != 2 || (strcmp(argv[1],"--grant") && strcmp(argv[1],"--revoke")))
  {
    printf("usage: %s (--grant|--revoke)\n",argv[0]);
    printf("%s is a helper for\n",argv[0]);
    printf("konsole and not intented to\n");
    printf("be called from the command\n");
    printf("line. In needs to be installed\n");
    printf("root setuid to function.\n");
    return 1; /* FAIL */
  }

  if (geteuid () != 0)
  {
    fprintf(stderr,"%s not installed root-suid\n",argv[0]);
    return 1; /* FAIL */
  }

  /* setup parameters for the operation ***********************************/

  if (!strcmp(argv[1],"--grant"))
  { 
    uid = getuid(); /* current user id */
    mod = S_IRUSR|S_IWUSR|S_IWGRP;
  }
  else
  {
    uid = 0;        /* root */
    mod = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
  }
  /* Get the group ID of the special `tty' group.  */
  p = getgrnam(TTY_GROUP);            /* posix */
  gid = p ? p->gr_gid : getgid ();    /* posix */

  /* get slave pty name from master pty file handle in PTY_FILENO *********/

#if defined(BSD_PTY_HACK)
  /*
    Hack to make konsole_grantpty work on *BSD.  ttyname(3) does not work
    on BSD based systems with a file descriptor opened on a /dev/pty?? device.

    Instead, this code looks through all the devices in /dev for a device
    which has the same inode as our PTY_FILENO descriptor... if found, we
    have the name for our pty.
  */

  if (uid == 0) {
    p = getgrnam("wheel");
    gid = p ? p->gr_gid : getgid();
  }

  pty = NULL;
  {
    struct dirent *dirp;
    DIR *dp;
    struct stat dsb;

    if (fstat(PTY_FILENO, &dsb) != -1) {
      if ((dp = opendir(_PATH_DEV)) != NULL) {
        while ((dirp = readdir(dp))) {
          if (dirp->d_fileno != dsb.st_ino)
            continue;

          {
            int pdlen = strlen(_PATH_DEV), namelen = strlen(dirp->d_name);
            pty = malloc(pdlen + namelen + 1);
            if (pty) {
              *pty = 0;
              strcat(pty, _PATH_DEV);
              strcat(pty, dirp->d_name);
            }
          }
        }

        (void) closedir(dp);
      }
    }
  }
#else
  /* Check that PTY_FILENO is a valid master pseudo terminal.  */
  pty = ttyname(PTY_FILENO);          /* posix */
#endif

  if (pty == NULL)
  {
    fprintf(stderr,"%s: cannot determine the name of device.\n",argv[0]);
    return 1; /* FAIL */
  }
  close(PTY_FILENO);

  /* matches /dev/pty?? */
  if (strlen(pty) < 8 || strncmp(pty,"/dev/pty",8))
  {
    fprintf(stderr,"%s: determined a strange device name `%s'.\n",argv[0],pty);
    return 1; /* FAIL */
  }

  tty = malloc(strlen(pty) + 1);
  strcpy(tty,"/dev/tty");
  strcat(tty,pty+8);

  /* Check that the returned slave pseudo terminal is a character device.  */
  if (stat(tty, &st) < 0 || !S_ISCHR(st.st_mode))
  {
    fprintf(stderr,"%s: found `%s' not to be a character device.\n",argv[0],tty);
    return 1; /* FAIL */
  }

  /* Perform the actual chown/chmod ************************************/

  if (chown(tty, uid, gid) < 0)
  {
    fprintf(stderr,"%s: cannot chown %s.\n",argv[0],tty); perror("Reason");
    return 1; /* FAIL */
  }

  if (chmod(tty, mod) < 0)
  {
    fprintf(stderr,"%s: cannot chmod %s.\n",argv[0],tty); perror("Reason");
    return 1; /* FAIL */
  }

  return 0; /* OK */
}
