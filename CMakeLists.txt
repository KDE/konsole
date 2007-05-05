
project(konsole)

add_subdirectory( src ) 
add_subdirectory( data ) 
add_subdirectory( desktop )


########### install files ###############

install( FILES konsole.upd   DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )
install(PROGRAMS schemaStrip.pl  DESTINATION  ${KCONF_UPDATE_INSTALL_DIR} )


