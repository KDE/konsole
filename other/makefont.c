/*
  makes vga font for X11

  use with font specification from /usr/src/linux/drivers/video/font_*.c

  usage: makefont > linux8x16.bdf
         bdftopcf -o linux8x16.pcf linux8x16.bdf
         gzip linux8x16.pcf

  use X  Y A with
      8  8 1 font_8x8
      6 11 3 font_6x11.c
      8 16 4 font_8x16.c
*/

/* insert font file here */

#define X 6
#define Y 11
#define A 3

main()
{ int i,j;
  printf("STARTFONT 2.1\n");
  printf("COMMENT Linux console font %dx%d\n",X,Y);
  printf("FONT linux%dx%d\n",X,Y);
  printf("SIZE 8 75 75\n");
  printf("FONTBOUNDINGBOX %d %d 0 %d\n",X,Y,-A);
  printf("STARTPROPERTIES 2\n");
  printf("FONT_DESCENT %d\n",A);
  printf("FONT_ASCENT %d\n",X-A);
  printf("ENDPROPERTIES\n");
  printf("CHARS 256\n");
  for (i = 0; i < 256; i++)
  {
    printf("STARTCHAR x%02x\n",i);
    printf("ENCODING %d\n",i);
    printf("SWIDTH %d %d\n",0,0);
    printf("DWIDTH %d %d\n",X,0);
    printf("BBX %d %d %d %d\n",X,Y,0,-A);
    printf("BITMAP\n");
    for (j = 0; j < Y; j++)
    {
      printf("%02x\n",(unsigned char)fontdata_6x11[Y*i+j]);
    }
    printf("ENDCHAR x%02x\n",i);
  }
  printf("ENDFONT\n");
}
