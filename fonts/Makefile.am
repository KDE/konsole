kde_fontdir = $(prefix)/share/fonts
fonts =console8x16.pcf.gz console8x8.pcf.gz 9x15.pcf.gz

EXTRA_DIST = $(fonts)

install-data-local: 
	$(mkinstalldirs) $(kde_fontdir)
	for i in $(fonts); do $(INSTALL_DATA) $(srcdir)/$$i $(kde_fontdir)/$$i; done

uninstall-local:
	for i in $(fonts); do rm -f $(kde_fontdir)/$$i; done

