fontdir = $(prefix)/share/fonts
font_DATA =console8x16.pcf.gz console8x8.pcf.gz 9x15.pcf.gz

EXTRA_DIST = $(font_DATA)

# This uses -hook instead of -local so that it is run after fonts are installed
install-data-hook:
	-mkfontdir $(DESTDIR)$(fontdir)
