# This file is part of Konsole - an X terminal for KDE

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
bin_PROGRAMS = 	konsole konsole_grantpty kwrited #kcmkonsole

noinst_LIBRARIES = libkonsole.a

libkonsole_a_SOURCES = TEWidget.C TEmuVt102.C TEScreen.C TEmulation.C TEPty.C TEHistory.C keytrans.C
# libkonsole_a_LDFLAGS =  -version-info `expr $(VERSION) + $(SUBVERS)`:$(RELEASE):$(SUBVERS)

# Which sources should be compiled for konsole.

kwrited_SOURCES	= kwrited.C
kwrited_LDADD   = libkonsole.a $(LIB_KIMGIO) $(LIB_KDEUI) $(LIBUTEMPTER)
kwrited_LDFLAGS = $(all_libraries) $(KDE_RPATH)

konsole_SOURCES	= main.C schema.C session.C krootpixmap.cpp
konsole_LDADD   = libkonsole.a $(LIB_KIMGIO) $(LIB_KDEUI) $(LIBUTEMPTER)
konsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# kcmkonsole_SOURCES = kcmkonsole.C schema.C
# kcmkonsole_LDADD   = $(LIB_KIMGIO) $(LIB_KDEUI)
# kcmkonsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

konsole_grantpty_SOURCES = konsole_grantpty.c
konsole_grantpty_LDFLAGS = $(KDE_RPATH)

install-data-local:
	@echo ""
	@echo "konsole_grantpty is by default installed with SETUID root bit!"
	@echo "This is needed for konsole to ensure that it can't be eavesdroped."
	@echo ""

# konsole_grantpty needs to be installed setuid root
install-exec-local: install-binPROGRAMS
	@(chown root $(bindir)/konsole_grantpty && chmod 4755 $(bindir)/konsole_grantpty) || echo "Please make konsole_grandpty setuid root"

# just to make sure, automake makes them
METASOURCES = TEWidget.moc TEmuVt102.moc session.moc main.moc \
	TEmulation.moc TEPty.moc kwrited.moc \
	krootpixmap.moc

META_INCLUDES = $(srcdir)/../include

messages:
	rm -f schemas.C
	for i in ../other/*.schema; do \
	  grep "^title" $$i | cut -d':' -f2- | sed -e 's#^title \(.*\)$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	for i in ../other/*.Keytab ../other/*.keytab; do \
	  grep "^keyboard" $$i | sed -e 's#^keyboard \"\(.*\)\"$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	$(XGETTEXT) -C  -kI18N_NOOP -ki18n -x $(includedir)/kde.pot $(konsole_SOURCES) schemas.C -o $(podir)/konsole.pot
	rm -f schemas.C


