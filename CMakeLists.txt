kde4_header()

add_subdirectory( konsole ) 
add_subdirectory( other ) 
add_subdirectory( doc ) 
add_subdirectory( settings ) 

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############

install_files( ${XDG_APPS_DIR} FILES konsole.desktop konsolesu.desktop )
install_files( ${DATA_INSTALL_DIR}/konsole FILES konsole.notifyrc tips )
install_files( ${SERVICETYPES_INSTALL_DIR} FILES terminalemulator.desktop )
install_files( ${SERVICES_INSTALL_DIR} FILES konsolepart.desktop kwrited.desktop konsole-script.desktop )
install_files( ${DATA_INSTALL_DIR}/konqueror/servicemenus FILES konsolehere.desktop )
install_files( ${SERVICES_INSTALL_DIR}/kded FILES kwrited.desktop )
install_files( ${DATA_INSTALL_DIR}/kconf_update FILES konsole.upd )

kde4_footer()



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
