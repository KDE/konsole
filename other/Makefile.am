datadir = $(kde_datadir)/konsole
data_DATA = BlackOnLightYellow.schema BlackOnWhite.schema \
	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema \
	default.Schema linux.kdelnk mc.kdelnk shell.kdelnk \
  syscolor.schema Linux.schema XTerm.schema vim.schema \
  DarkPicture.schema LightPicture.schema su.kdelnk

kde_icon_DATA = konsole.xpm colourness.xpm brightness.xpm contrast.xpm


install-data-local: 
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) $(srcdir)/konsole-mini.xpm $(kde_minidir)/konsole.xpm
	$(mkinstalldirs) $(kde_icondir)/large
	$(INSTALL_DATA) $(srcdir)/large-konsole.xpm $(kde_icondir)/large/konsole.xpm

uninstall-local:
	rm -f $(kde_icondir)/large/konsole.xpm
	rm -f $(kde_minidir)/konsole.xpm

EXTRA_DIST = $(data_DATA) $(kde_icon_DATA) konsole-mini.xpm

