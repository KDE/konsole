
datadir = $(kde_datadir)/konsole
data_DATA = BlackOnLightYellow.schema BlackOnWhite.schema \
	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema \
	default.Schema dselect.kdelnk linux.kdelnk mc.kdelnk shell.kdelnk \
  syscolor.schema Linux.schema XTerm.schema

icondir = $(kde_icondir)
icon_DATA = konsole.xpm

install-data-local: 
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) konsole-mini.xpm $(kde_minidir)/konsole.xpm
	$(INSTALL_DATA) colourness.xpm $(kde_icondir)
	$(INSTALL_DATA) brightness.xpm $(kde_icondir)
	$(INSTALL_DATA) contrast.xpm $(kde_icondir)

uninstall-local:
	rm -f $(kde_minidir)/konsole.xpm

