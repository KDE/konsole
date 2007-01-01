
add_subdirectory( icons ) 
add_subdirectory( color-schemes )
add_subdirectory( keyboard-layouts )
add_subdirectory( sessions )


########### install files ###############

install( FILES 2shells 3shells 4shells 5shells  DESTINATION  ${DATA_INSTALL_DIR}/konsole/profiles )

kde4_install_icons( ${ICON_INSTALL_DIR}  )

