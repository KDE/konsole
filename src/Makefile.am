# This file is part of Konsole - an X terminal for KDE

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
lib_LTLIBRARIES = libkonsolepart.la kwrited.la konsole.la
# libkonsolepart is a part, but konsole.la links directly to it, so we can't
# install it under kde_module

libkonsolepart_la_SOURCES = TEPty.C BlockArray.C \
	schema.C \
	session.C \
	TEWidget.C \
	TEmuVt102.C \
	TEScreen.C \
	TEmulation.C \
	TEHistory.C \
	keytrans.C \
        konsole_part.C

libkonsolepart_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkonsolepart_la_LIBADD = $(LIB_KPARTS) $(LIBUTIL)

# kwrited kdeinit module
kwrited_la_SOURCES = kwrited.C
kwrited_la_LIBADD  = libkonsolepart.la
kwrited_la_LDFLAGS = $(all_libraries) -module -avoid-version

# kwrited executable
kwrited_SOURCES	= dummy.C
kwrited_LDADD   = kwrited.la 
kwrited_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# konsole kdeinit module
konsole_la_SOURCES = main.C konsole.C
konsole_la_LIBADD  = libkonsolepart.la
konsole_la_LDFLAGS = $(all_libraries) -module -avoid-version

# konsole executable - has all the sources, doesn't link kparts, saves startup time
konsole_SOURCES	= TEPty.C BlockArray.C main.C konsole.C schema.C session.C TEWidget.C TEmuVt102.C \
                  TEScreen.C TEmulation.C TEHistory.C keytrans.C 
konsole_LDADD = $(LIB_KDEUI) $(LIB_KSYCOCA) $(LIBUTIL)
konsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# kcmkonsole_SOURCES = kcmkonsole.C schema.C
# kcmkonsole_LDADD   = $(LIB_KDEUI)
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

# Not AUTO here, because there are some unused files
METASOURCES = TEWidget.moc TEmuVt102.moc session.moc konsole.moc \
       TEmulation.moc TEPty.moc kwrited.moc konsole_part.moc 

META_INCLUDES = $(srcdir)/../include

messages: rc.cpp
	rm -f schemas.C
	for i in ../other/*.schema; do \
	  grep "^title" $$i | cut -d':' -f2- | sed -e 's#^title \(.*\)$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	for i in ../other/*.Keytab ../other/*.keytab; do \
	  grep "^keyboard" $$i | sed -e 's#^keyboard \"\(.*\)\"$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	$(XGETTEXT) rc.cpp *.C -o $(podir)/konsole.pot
	rm -f schemas.C

dummy.C:
	echo > dummy.C


rcdir = $(kde_datadir)/konsole
rc_DATA = konsole_part.rc

# setting up x-application/konsole as a special mimetype
mimedir = $(kde_mimedir)/application
mime_DATA = x-konsole.desktop

# default toolbar configuration
datadir =$(kde_confdir)
data_DATA =konsolerc

EXTRA_DIST = $(mime_DATA) $(data_DATA)                                             
