fontdir = $(datadir)/fonts
font_DATA =console8x16.pcf.gz console8x8.pcf.gz 9x15.pcf.gz

EXTRA_DIST = $(font_DATA)

# This uses -hook instead of -local so that it is run after fonts are installed
install-data-hook:
	-mkfontdir $(DESTDIR)$(fontdir)

# edit with xmbdfed console8x16.bdf
fonts: $(srcdir)/console8x16.bdf
	bdftopcf -o console8x16.pcf $(srcdir)/console8x16.bdf
	gzip -c --best console8x16.pcf > $(srcdir)/console8x16.pcf.gz
