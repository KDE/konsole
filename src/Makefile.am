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

TOPDIR = $(srcdir)/..
INCDIR = $(TOPDIR)/include
OTHDIR = $(TOPDIR)/other

# set the include path for X, qt and KDE
INCLUDES =  -I$(INCDIR) $(all_includes)

# you can add here more. This one gets installed 
bin_PROGRAMS = 	konsole kcmkonsole konsole_grantpty

noinst_LIBRARIES = libkonsole.a

libkonsole_a_SOURCES = TEWidget.C TEmuVt102.C TEScreen.C TEmulation.C TEShell.C
# libkonsole_a_LDFLAGS =  -version-info `expr $(VERSION) + $(SUBVERS)`:$(RELEASE):$(SUBVERS)
                       
# Which sources should be compiled for konsole.

konsole_SOURCES	= main.C schema.C session.C
konsole_LDADD   = libkonsole.a $(LIB_KIMGIO) $(LIB_KDEUI) 
konsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

kcmkonsole_SOURCES = kcmkonsole.C schema.C
kcmkonsole_LDADD   = $(LIB_KIMGIO) $(LIB_KDEUI) 
kcmkonsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

konsole_grantpty_SOURCES = konsole_grantpty.c

install-exec-local:
	@echo
	@echo Make sure to set konsole_grantpty to be root suid.
	@echo

# the library search path. 

# just to make sure, automake makes them 
METASOURCES =TEWidget.moc TEmuVt102.moc session.moc main.moc \
	TEmulation.moc TEShell.moc kcmkonsole.moc

META_INCLUDES = $(srcdir)/../include

messages:
	$(XGETTEXT) -C -ktranslate -ki18n -x $(includedir)/kde.pot $(konsole_SOURCES) && mv messages.po ../../po/konsole.pot

