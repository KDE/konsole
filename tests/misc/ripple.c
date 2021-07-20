/*
  Ripple test.
  Usage: ripple [ w [ l ] ]
   w = screen line width, default 80, must be > 0, max 132.
   l = how many lines to display, default 1000, must be > 0.
  Author: Frank da Cruz, Columbia University, 1995.
  This is in the public domain as far as can be determined.
*/
char *crlf = "\015\012";
char *p = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]\
^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGH\
IJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~ !\"#$%&'()*+,-./012\
3456789:;<=>?@ABCD";

main(argc,argv) int argc; char *argv[]; {
    int i, j, w = 80, l = 1000;

    if (argc > 1)                      /* User-specified width */
      w = atoi(argv[1]);
    if (argc > 2)                      /* User-specified number of lines */
      l = atoi(argv[2]);
    if (w < 1 || l < 1 || w > 132)   /* Quit upon conversion error */
      exit(1);

    for (j = i = 0; i < l; i++) {      /* Ripple loop */
        write(1, p+j, w);
        write(1, crlf, 2);
        if (++j > 94) j = 0;
    }
}
