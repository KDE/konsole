/*
	"VGA8x16", 8, 16, fontdata_8x16,

  makes vga font for X11

  use with font specification from /usr/src/linux/drivers/video/font_8x16.c

  usage: makefont > linux8x16.bdf
         bdftopcf -o linux8x16.pcf linux8x16.bdf
         gzip linux8x16.pcf
*/

main()
{ int i,j;
  printf("STARTFONT 2.1\n");
  printf("COMMENT Linux console font 8x16\n");
  printf("FONT linux8x16\n");
  printf("SIZE 8 75 75\n");
  printf("FONTBOUNDINGBOX 8 16 0 -4\n");
  printf("STARTPROPERTIES 2\n");
  printf("FONT_DESCENT 4\n");
  printf("FONT_ASCENT 12\n");
  printf("ENDPROPERTIES\n");
  printf("CHARS 256\n");
  for (i = 0; i < 256; i++)
  {
    printf("STARTCHAR x%02x\n",i);
    printf("ENCODING %d\n",i);
    printf("SWIDTH %d %d\n",0,0);
    printf("DWIDTH %d %d\n",8,0);
    printf("BBX %d %d %d %d\n",8,16,0,-4);
    printf("BITMAP\n");
    for (j = 0; j < 16; j++)
    {
      printf("%02x\n",(unsigned char)fontdata_8x16[16*i+j]);
    }
    printf("ENDCHAR x%02x\n",i);
  }
  printf("ENDFONT\n");
}
