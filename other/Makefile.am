
datadir = $(kde_datadir)/konsole
data_DATA = BlackOnLightYellow.schema BlackOnWhite.schema \
	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema \
	default.Schema linux.kdelnk mc.kdelnk shell.kdelnk \
  syscolor.schema Linux.schema XTerm.schema vim.schema \
  DarkPicture.schema LightPicture.schema

kde_icon_DATA = konsole.xpm colourness.xpm brightness.xpm contrast.xpm


install-data-local: 
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) $(srcdir)/konsole-mini.xpm $(kde_minidir)/konsole.xpm

uninstall-local:
	rm -f $(kde_minidir)/konsole.xpm

