# This file is part of Konsole - an X terminal for KDE

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

VERSION=0
SUBVERS=9
RELEASE=1

# just set the variable
APPSDIR = $(kde_appsdir)/Utilities
DATADIR = $(kde_datadir)/konsole
PICSDIR = $(DATADIR)/pics
TOOLBARDIR = $(DATADIR)/toolbar
MIMEDIR = $(kde_mimedir)/text

TOPDIR = $(top_srcdir)/konsole
INCDIR = $(TOPDIR)/include
OTHDIR = $(TOPDIR)/other

# set the include path for X, qt and KDE
INCLUDES =  -I$(INCDIR) $(all_includes)
# claim, which subdirectories you want to install
#SUBDIRS = 

# you can add here more. This one gets installed 
bin_PROGRAMS = 	konsole

lib_LTLIBRARIES = libkonsole.la

libkonsole_la_SOURCES = session.C TEWidget.C TEmuVt102.C TEScreen.C TEmulation.C TEShell.C
libkonsole_la_LDFLAGS =  -version-info `expr $(VERSION) + $(SUBVERS)`:$(RELEASE):$(SUBVERS)
                       
# Which sources should be compiled for konsole.

konsole_SOURCES	= main.C
konsole_LDADD   = libkonsole.la -lkdeui -lkdecore -lqt -lXext -lX11 $(EXTRALIBS) $(LIBSOCKET)
konsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

#LIBOBJS = TEWidget.lo TEmuVt102.lo TEScreen.lo TEmulation.lo TEShell.lo

# the library search path. 

# i have had troubles with the @LIB_KIMGIO@ macro.
# if you like to put it in here, please make sure it works before commiting
EXTRALIBS = $(KIMGIO_LIBS) -lkimgio

# just to make sure, automake makes them 
BUILTSOURCES =TEWidget.moc TEmuVt102.moc session.moc main.moc TEmulation.moc TEShell.moc

# use DISTCLEANFILES to remove these only when doing a make distclean.
CLEANFILES = $(BUILTSOURCES)

messages:
	$(XGETTEXT) -C -ktranslate -ki18n -x $(includedir)/kde.pot $(konsole_SOURCES) && mv messages.po ../../po/konsole.pot

# just install datas here. Use install-exec-local for scripts and etc.
# the binary itself is already installed from automake
# use mkinstalldirs, not "install -d"
# don't install a list of file. Just one file per install.
# if you have more of them, create a subdirectory with an extra Makefile 

install-data-local: 

# remove ALL you have installed in install-data-local or install-exec-local
uninstall-local:


# add a dependency for every moc file to be full portable

TEWidget.C: TEWidget.moc

TEWidget.moc: $(INCDIR)/TEWidget.h
	$(MOC) $(INCDIR)/TEWidget.h -o TEWidget.moc

TEShell.C: TEShell.moc

TEShell.moc: $(INCDIR)/TEShell.h
	$(MOC) $(INCDIR)/TEShell.h -o TEShell.moc

TEmulation.C: TEmulation.moc

TEmulation.moc: $(INCDIR)/TEmulation.h
	$(MOC) $(INCDIR)/TEmulation.h -o TEmulation.moc

TEmuVt102.C: TEmuVt102.moc

TEmuVt102.moc: $(INCDIR)/TEmuVt102.h
	$(MOC) $(INCDIR)/TEmuVt102.h -o TEmuVt102.moc

main.C: main.moc

main.moc: $(INCDIR)/main.h
	$(MOC) $(INCDIR)/main.h -o main.moc

session.C: session.moc

session.moc: $(INCDIR)/session.h
	$(MOC) $(INCDIR)/session.h -o session.moc
