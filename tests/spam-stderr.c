// from https://bugs.kde.org/show_bug.cgi?id=230184
// author Stefan Westerfeld
// This code is in the public domain.

#include <stdio.h>

int
main()
{
  for (int i = 0; i < 100000000; i++)
    {
      fprintf (stderr, "foo %d\n", i);
    }
}


