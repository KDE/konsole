/*
 
   This is a tiny test program that can be used to track down
   strange effects of the emulation.

   Make:

   - gcc -o audit audit.c

   Usage:

   - In TEShell.C let syslog be stdout.
   - konsole > ttt
   - produce the effect in question.
   - run this program.
     pressing any key advances the audit
     ^C terminates.

   You need to make sure that the size of the screen matches
   the one being debugged.

   This code was written by Lars Doelle <lars.doelle@on-line.de>.
   This code is in the public domain.
 
*/


#include <stdio.h>
#include <termios.h>
#include <unistd.h>

struct termios save;
struct termios curr;

#define HERE fprintf(stderr,"%s(%d): here.\n",__FILE__,__LINE__)

main()
{ int cc;
  FILE* sysin = fopen("ttt","r");
  tcgetattr(0, &save);
  tcgetattr(0, &curr);
  cfmakeraw(&curr);
  tcsetattr(0, TCSANOW, &curr);
  cc = fgetc(sysin);
  while( cc > 0 )
  { int tmp;
    while (cc > 0)
    {
      fputc(cc,stdout); cc = fgetc(sysin);
      if (cc == 0x1b) break;
    }
    tmp = fgetc(stdin);
    if (tmp == 3) break;
  }
  tcsetattr(0, TCSANOW, &save);
}
