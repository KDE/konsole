
add_subdirectory( color-schemes )
add_subdirectory( keyboard-layouts )

if (${ECM_VERSION} STRGREATER "5.58.0")
  install( FILES konsole.knsrc DESTINATION ${KDE_INSTALL_KNSRCDIR})
else()
  install( FILES konsole.knsrc DESTINATION ${CONFIG_INSTALL_DIR})
endif()
