# This file is part of Konsole - an X terminal for KDE

# just set the variable
APPSDIR = $(kde_appsdir)/Utilities
DATADIR = $(kde_datadir)/konsole
PICSDIR = $(DATADIR)/pics
TOOLBARDIR = $(DATADIR)/toolbar
MIMEDIR = $(kde_mimedir)/text

# set the include path for X, qt and KDE
INCLUDES = -I. $(all_includes)

# you can add here more. This one gets installed
bin_PROGRAMS = 	konsole konsole_grantpty kwrited #kcmkonsole
lib_LTLIBRARIES = libkonsolepart.la kwrited.la konsole.la

# libkonsolepart is a part, but konsole.la links directly to it, so we can't
# install it under kde_module

libkonsolepart_la_SOURCES = TEPty.cpp BlockArray.cpp konsole_part.cpp \
	schema.cpp \
	session.cpp \
	TEWidget.cpp \
	TEmuVt102.cpp \
	TEScreen.cpp konsole_wcwidth.cpp \
	TEmulation.cpp \
	TEHistory.cpp \
	keytrans.cpp \
	konsoleiface.skel \
        sessioniface.skel

libkonsolepart_la_LDFLAGS = $(all_libraries) $(KDE_PLUGIN)
libkonsolepart_la_LIBADD = $(LIB_KPARTS) $(LIBUTIL) $(XTESTLIB)

# kwrited kdeinit module
kwrited_la_SOURCES = kwrited.cpp
kwrited_la_LIBADD  = libkonsolepart.la
kwrited_la_LDFLAGS = $(all_libraries) -module -avoid-version

# kwrited executable
kwrited_SOURCES	= dummy.cpp
kwrited_LDADD   = kwrited.la 
kwrited_LDFLAGS = $(all_libraries) $(KDE_RPATH)


# konsole kdeinit module
konsole_la_SOURCES = TEPty.cpp BlockArray.cpp main.cpp konsole.cpp schema.cpp session.cpp TEWidget.cpp TEmuVt102.cpp \
     TEScreen.cpp TEmulation.cpp TEHistory.cpp keytrans.cpp konsoleiface.skel sessioniface.skel \
     konsole_wcwidth.cpp konsolebookmarkhandler.cpp konsole_child.cpp
konsole_la_LDFLAGS = $(all_libraries) -module -avoid-version
konsole_la_LIBADD = $(LIB_KDEUI) $(LIB_KIO) $(LIBUTIL) $(XTESTLIB)

# konsole executable
konsole_SOURCES = dummy.cpp
konsole_LDADD = konsole.la $(LIB_KIO)
konsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

# kcmkonsole_SOURCES = kcmkonsole.cpp schema.cpp
# kcmkonsole_LDADD   = $(LIB_KDEUI)
# kcmkonsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)

noinst_HEADERS = TEWidget.h TEPty.h TEmulation.h TEmuVt102.h konsole_child.h \
	TECommon.h TEScreen.h konsole.h schema.h session.h konsole_wcwidth.h \
	kwrited.h TEHistory.h keytrans.h default.keytab.h BlockArray.h \
        konsolebookmarkhandler.h

konsole_grantpty_SOURCES = konsole_grantpty.c
konsole_grantpty_LDFLAGS = $(KDE_RPATH)

install-data-local:
	@echo ""
	@echo "konsole_grantpty is by default installed with SETUID root bit!"
	@echo "This is needed for konsole to ensure that it can't be eavesdroped."
	@echo ""

# konsole_grantpty needs to be installed setuid root
install-exec-hook: 
	@(chown root $(DESTDIR)$(bindir)/konsole_grantpty && chmod 4755 $(DESTDIR)$(bindir)/konsole_grantpty) || echo "Please make konsole_grandpty setuid root"

# Not AUTO here, because there are some unused files
METASOURCES = TEWidget.moc TEmuVt102.moc session.moc konsole.moc konsole_child.moc \
              TEmulation.moc TEPty.moc kwrited.moc konsole_part.moc konsolebookmarkhandler.moc

messages: rc.cpp
	rm -f schemas.cpp
	rm -f tips.cpp
	(cd .. && $(PREPARETIPS) > konsole/tips.cpp)
	for i in ../other/*.schema; do \
	  grep "^title" $$i | cut -d':' -f2- | sed -e 's#^title \(.*\)$$#i18n(\"\1\")#' >> schemas.cpp ;\
	done
	for i in ../other/*.Keytab ../other/*.keytab; do \
	  grep "^keyboard" $$i | sed -e 's#^keyboard \"\(.*\)\"$$#i18n(\"\1\")#' >> schemas.cpp ;\
	done
	$(XGETTEXT) rc.cpp *.cpp -o $(podir)/konsole.pot
	rm -f schemas.cpp
	rm -f tips.cpp

dummy.cpp:
	echo > dummy.cpp

# setting up x-application/konsole as a special mimetype
mimedir = $(kde_mimedir)/application
mime_DATA = x-konsole.desktop

# default toolbar configuration
datadir = $(kde_confdir)
data_DATA =konsolerc

EXTRA_DIST = $(mime_DATA) $(data_DATA)
