# This file is part of Konsole - an X terminal for KDE

#VERSION=1
#SUBVERS=9
#RELEASE=1

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
lib_LTLIBRARIES = libkonsole.la kwrited.la

#noinst_LTLIBRARIES = libkonsole.la

libkonsole_la_SOURCES =konsole.C \
	schema.C \
	session.C \
	TEWidget.C \
	TEmuVt102.C \
	TEScreen.C \
	TEmulation.C \
	TEPty.C \
	TEHistory.C \
	keytrans.C \
        konsole_part.C

libkonsole_la_LDFLAGS = -no-undefined $(all_libraries) -version-info 1:0:0 -module 
# -version-info `expr $(VERSION) + $(SUBVERS)`:$(RELEASE):$(SUBVERS)

libkonsole_la_LIBADD = $(LIB_KDEUI) $(LIBUTEMPTER) -lkparts

# Which sources should be compiled for konsole.

kwrited_la_SOURCES = kwrited.C
kwrited_la_LIBADD  = libkonsole.la $(LIB_KIMGIO) 
kwrited_la_LDFLAGS = $(all_libraries) -module -avoid-version

kwrited_SOURCES	= dummy.C
kwrited_LDADD   = kwrited.la 
kwrited_LDFLAGS = $(all_libraries) $(KDE_RPATH)

konsole_SOURCES	= main.C 
konsole_LDADD   = libkonsole.la $(LIB_KIMGIO)
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
METASOURCES = TEWidget.moc TEmuVt102.moc session.moc konsole.moc \
	TEmulation.moc TEPty.moc kwrited.moc konsole_part.moc

META_INCLUDES = $(srcdir)/../include

messages:
	rm -f schemas.C
	for i in ../other/*.schema; do \
	  grep "^title" $$i | cut -d':' -f2- | sed -e 's#^title \(.*\)$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	for i in ../other/*.Keytab ../other/*.keytab; do \
	  grep "^keyboard" $$i | sed -e 's#^keyboard \"\(.*\)\"$$#i18n(\"\1\")#' >> schemas.C ;\
	done
	$(XGETTEXT)  $(konsole_SOURCES) schemas.C -o $(podir)/konsole.pot
	rm -f schemas.C

dummy.C:
	echo > dummy.C


rcdir = $(kde_datadir)/konsole
rc_DATA = konsole_part.rc

# setting up x-application/konsole as a special mimetype
mimedir = $(kde_mimedir)/application
mime_DATA = x-konsole.desktop

# specifying gofai as a handler for the above mimetype
# also specifies the icon, and position in the kicker menus
kdelnkdir = $(kde_appsdir)/Utilities
kdelnk_DATA = konsole.desktop

# ??? no need to specify kdelnk_DATA here?
EXTRA_DIST = $(mime_DATA)                                             