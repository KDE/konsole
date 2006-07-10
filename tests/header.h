/*
                               VTTEST.C

         Written Novemeber 1983 - July 1984 by Per Lindberg,
         Stockholm University Computer Center (QZ), Sweden.

                  THE MAD PROGRAMMER STRIKES AGAIN!

                   This software is (c) 1984 by QZ
               Non-commercial use and copying allowed.

If you are developing a commercial product, and use this program to do
it, and that product is successful, please send a sum of money of your
choice to the address below.

*/
#define VERSION "1.7b 1985-04-19"

/* Choose one of these */

/* #define XENIX        /* XENIX implies UNIX                           */
/* #define SIII         /* SIII  implies UNIX, (NDELAY a la System III) */
#define UNIX            /* UNIX                                         */
#define TERMIO
/* #define VMS          /* VMS not done yet -- send me your version!!!! */
/* #define SARG20       /* Sargasso C for TOPS-20                       */
/* #define SARG10       /* Sargasso C for TOPS-10                       */

/* These #ifdef:s are implementation dependent stuff for the Sargasso C */
/* Unix C barfs on directives like "#strings", so we keep them	        */
/* indented. Then unix c can't find them, but Sargasso C *can*.		*/
/* Admittedly kludgey, but it works...)					*/
#ifdef SARG10
  #define _UNIXCON  /* Make UNIX-flavored I/O on TOPS */
  #strings low      /* put strings in lowseg mem so we can modify them. */
#endif
#ifdef SARG20
  #define _UNIXCON  /* Make UNIX-flavored I/O on TOPS */
  #strings low      /* put strings in lowseg mem so we can modify them. */
  #include <TOPS20.HDR>
#endif

#include <stdio.h>


#ifdef UNIX
#include <ctype.h>
#ifdef TERMIO
# include <termio.h>
#else
# include <sgtty.h>
#endif
#include <signal.h>
#include <setjmp.h>
jmp_buf intrenv;

#ifdef TERMIO
struct termio termioOrg, termioNew;
#else
struct sgttyb sgttyOrg, sgttyNew;
#endif

char stdioBuf[BUFSIZ];
int brkrd, reading;
extern onterm(), onbrk();
#ifdef SIII
#include <fcntl.h>
#endif
#endif
int ttymode;
