# this 10 paths are KDE specific. Usage:

# kde_htmldir       Where your docs should go to. (contains lang subdirs)
# kde_appsdir       Where your application file (.kdelnk) should go to. 
# kde_icondir       Where your icon should go to.
# kde_minidir       Where your mini icon should go to.
# kde_datadir       Where you install application data. (Use a subdir)
# kde_locale        Where translation files should go to.(contains lang subdirs)
# kde_cgidir        Where cgi-bin executables should go to.
# kde_confdir       Where config files should go to.
# kde_mimedir       Where mimetypes should go to.
# kde_toolbardir    Where general toolbar icons should go to.
# kde_wallpaperdir  Where general wallpapers should go to.

# just set the variable
APPSDIR = $(kde_appsdir)/Utilities
DATADIR = $(kde_datadir)/konsole
PICSDIR = $(DATADIR)/pics
TOOLBARDIR = $(DATADIR)/toolbar
MIMEDIR = $(kde_mimedir)/text

# just install datas here. Use install-exec-local for scripts and etc.
# the binary itself is already installed from automake
# use mkinstalldirs, not "install -d"
# don't install a list of file. Just one file per install.
# if you have more of them, create a subdirectory with an extra Makefile 

install-data-local: 
	$(mkinstalldirs) $(DATADIR)
	$(INSTALL_DATA) *.[Ss]chema $(DATADIR)
	$(INSTALL_DATA) *.kdelnk $(DATADIR)
	$(mkinstalldirs) $(APPSDIR)
	$(INSTALL_DATA) konsole.kdelnk $(APPSDIR)
	$(mkinstalldirs) $(kde_icondir)
	$(INSTALL_DATA) konsole.xpm $(kde_icondir)
	$(mkinstalldirs) $(kde_minidir)
	$(INSTALL_DATA) konsole-mini.xpm $(kde_minidir)/konsole.xpm

# remove ALL you have installed in install-data-local or install-exec-local
uninstall-local:
	rm -f $(APPSDIR)/konsole.kdelnk
	rm -f $(kde_icondir)/konsole.xpm
	rm -f $(kde_minidir)/konsole.xpm

