SUBDIRS = konsole other doc fonts

xdg_apps_DATA = konsole.desktop konsolesu.desktop

kde_services_DATA = konsolepart.desktop kwrited.desktop konsole-script.desktop
kde_servicetypes_DATA = terminalemulator.desktop

update_DATA = konsole.upd
update_SCRIPTS = schemaStrip.pl
updatedir = $(kde_datadir)/kconf_update

servicesdir = $(kde_servicesdir)/kded
services_DATA = kwrited.desktop

install-data-local: uninstall.desktop
	$(mkinstalldirs) $(DESTDIR)$(datadir)/autostart
	$(INSTALL_DATA) $(srcdir)/uninstall.desktop $(DESTDIR)$(datadir)/autostart/kwrited.desktop

konsolehere_DATA = konsolehere.desktop
konsoleheredir = $(kde_datadir)/konqueror/servicemenus

rc_DATA = kwritedrc
rcdir = $(kde_confdir)

misc_DATA = eventsrc tips
miscdir = $(kde_datadir)/konsole

EXTRA_DIST = CONTRIBUTORS LICENSE.readme README.bugs \
	README.linux.console tests $(xdg_apps_DATA)
