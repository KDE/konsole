fontdir = $(kde_datadir)/konsole/fonts
font_DATA =console8x16.pcf.gz 9x15.pcf.gz

EXTRA_DIST = $(font_DATA)

# This uses -hook instead of -local so that it is run after fonts are installed
install-data-hook:
	-mkfontdir $(DESTDIR)$(fontdir)

console8x16.pcf.gz: $(srcdir)/console8x16.bdf
	bdftopcf -o console8x16.pcf $(srcdir)/console8x16.bdf
	gzip --best console8x16.pcf

9x15.pcf.gz: $(srcdir)/9x15.bdf
	bdftopcf -o 9x15.pcf $(srcdir)/9x15.bdf
	gzip --best 9x15.pcf
