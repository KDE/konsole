

########### install files ###############

install( PROGRAMS org.kde.konsole.desktop  DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.konsole.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install(FILES org.kde.konsole.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kglobalaccel)

install(FILES konsole.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
install( PROGRAMS konsolerun.desktop  DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus )
