SUBDIRS = konsole other doc fonts

data_DATA = konsole.desktop 
datadir = $(kde_appsdir)/System

sudata_DATA = konsolesu.desktop
sudatadir = $(kde_appsdir)/System/More

install-data-local: uninstall.desktop
	$(mkinstalldirs) $(DESTDIR)$(kde_appsdir)/System
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(DESTDIR)$(kde_appsdir)/System/konsolesu.desktop 

kde_services_DATA = konsolepart.desktop kwrited.desktop

update_DATA = konsole.upd schemaStrip.pl
updatedir = $(kde_datadir)/kconf_update

autostart_DATA = kwrited.desktop
autostartdir = $(prefix)/share/autostart

konsolehere_DATA = konsolehere.desktop
konsoleheredir = $(kde_datadir)/konqueror/servicemenus

rc_DATA = kwritedrc
rcdir = $(kde_confdir)

tip_DATA = tips
tipdir = $(kde_datadir)/konsole

EXTRA_DIST = CONTRIBUTORS LICENSE.readme README.bugs \
	README.linux.console tests $(data_DATA)

kwritedrc: $(top_builddir)/config.h
	echo "[General]" >kwritedrc
	if test -z "@LIBUTEMPTER@"; then \
	    echo "# No utempter support, kwrited disabled by default" >>kwritedrc ; \
	    echo Autostart=false >>kwritedrc ; \
	else \
	    echo Autostart=true >>kwritedrc ; \
	fi
