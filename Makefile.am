SUBDIRS = konsole other doc fonts

data_DATA = konsole.desktop konsolesu.desktop
datadir = $(kde_appsdir)/System

kde_services_DATA = konsolepart.desktop

update_DATA = konsole.upd
updatedir = $(kde_datadir)/kconf_update

autostart_DATA = kwrited.desktop
autostartdir = $(prefix)/share/autostart

rc_DATA = kwritedrc
rcdir = $(kde_confdir)

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
