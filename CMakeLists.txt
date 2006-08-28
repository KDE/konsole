
project(konsole)

add_subdirectory( konsole ) 
add_subdirectory( other ) 
add_subdirectory( settings ) 
add_subdirectory( desktop )


########### install files ###############

install( FILES konsole.upd   DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install(PROGRAMS schemaStrip.pl  DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )



#original Makefile.am contents follow:

#SUBDIRS = konsole other doc settings 
#
#xdg_apps_DATA = konsole.desktop konsolesu.desktop
#
#kde_services_DATA = konsolepart.desktop kwrited.desktop konsole-script.desktop
#kde_servicetypes_DATA = terminalemulator.desktop
#
#update_DATA = konsole.upd
#update_SCRIPTS = schemaStrip.pl
#updatedir = $(kde_datadir)/kconf_update
#
#servicesdir = $(kde_servicesdir)/kded
#services_DATA = kwrited.desktop
#
#install-data-local:
#	@-rm -f $(DESTDIR)$(datadir)/autostart/kwrited.desktop
#
#konsolehere_DATA = konsolehere.desktop
#konsoleheredir = $(kde_datadir)/konqueror/servicemenus
#
#misc_DATA = konsole.notifyrc tips
#miscdir = $(kde_datadir)/konsole
#
#EXTRA_DIST = CONTRIBUTORS LICENSE.readme README.bugs \
#	README.linux.console tests $(xdg_apps_DATA)
