
add_subdirectory( color-schemes )
add_subdirectory( keyboard-layouts )

if(DEFINED ${KDE_INSTALL_KNSRCDIR})
  install( FILES konsole.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
else()
  install( FILES konsole.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
endif()
