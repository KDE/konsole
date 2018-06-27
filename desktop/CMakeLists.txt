

########### install files ###############

install( PROGRAMS org.kde.konsole.desktop  DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.konsole.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install( FILES terminalemulator.desktop    DESTINATION ${KDE_INSTALL_KSERVICETYPES5DIR} )
install( FILES konsolepart.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR} )
install( FILES konsolehere.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/ServiceMenus )
install( FILES konsolerun.desktop  DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/ServiceMenus )
install( FILES konsole.khotkeys DESTINATION ${DATA_INSTALL_DIR}/khotkeys )
install( FILES konsole.notifyrc DESTINATION  ${KDE_INSTALL_KNOTIFY5RCDIR} )
