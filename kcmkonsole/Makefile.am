kde_module_LTLIBRARIES = kcm_konsole.la

kcm_konsole_la_SOURCES =  schemaeditor.cpp sessioneditor.cpp kcmkonsole.cpp \
	schemadialog.ui sessiondialog.ui kcmkonsoledialog.ui

kcm_konsole_la_LDFLAGS  = $(all_libraries) -module -avoid-version -no-undefined
kcm_konsole_la_LIBADD = -lkdeui $(LIB_KIO)
AM_CPPFLAGS = $(all_includes) 

METASOURCES = AUTO 

noinst_HEADERS = kcmkonsole.h schemaeditor.h sessioneditor.h schemadialog.h sessiondialog.h kcmkonsoledialog.h

kcmkonsole_data_DATA = kcmkonsole.desktop
kcmkonsole_datadir = $(kde_appsdir)/.hidden

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kcmkonsole.pot
