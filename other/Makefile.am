
datadir = $(kde_datadir)/konsole
data_DATA = BlackOnLightYellow.schema BlackOnWhite.schema \
	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema \
	default.Schema dselect.kdelnk linux.kdelnk mc.kdelnk shell.kdelnk \
  syscolor.schema

appsdir = $(kde_appsdir)/Utilities
apps_DATA = konsole.kdelnk

icondir = $(kde_icondir)
icon_DATA = konsole.xpm

install-data-local: 
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) konsole-mini.xpm $(kde_minidir)/konsole.xpm

uninstall-local:
	rm -f $(kde_minidir)/konsole.xpm

