

########### install files ###############
if (HAVE_DBUS)
    set(DBusActivatable "true")
else()
    set(DBusActivatable "false")
endif()
configure_file(org.kde.konsole.desktop.in org.kde.konsole.desktop @ONLY)
install( PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.desktop  DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.konsole.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kglobalaccel)

install(FILES konsole.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install( PROGRAMS konsolerun.desktop  DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus )
