
project(konsole)

add_subdirectory( konsole ) 
add_subdirectory( other ) 
add_subdirectory( desktop )


########### install files ###############

install( FILES konsole.upd   DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install(PROGRAMS schemaStrip.pl  DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )


