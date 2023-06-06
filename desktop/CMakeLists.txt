

########### install files ###############

install( PROGRAMS org.kde.konsole.desktop  DESTINATION ${KDE_INSTALL_APPDIR} )
install( FILES org.kde.konsole.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR} )
install( FILES org.kde.konsole.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kglobalaccel)

if(ECM_VERSION VERSION_GREATER_EQUAL "5.89.0")
    if(QT_MAJOR_VERSION STREQUAL "5")
        install(FILES terminalemulator.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPESDIR})
        install(FILES konsolepart.desktop DESTINATION ${KDE_INSTALL_KSERVICESDIR})
    endif()
    install(FILES konsole.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})
else()
    install(FILES terminalemulator.desktop DESTINATION ${KDE_INSTALL_KSERVICETYPES5DIR})
    install(FILES konsolepart.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
    install(FILES konsole.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFY5RCDIR})
endif()

if(KF${QT_MAJOR_VERSION}KIO_VERSION VERSION_GREATER_EQUAL "5.85.0")
    install( PROGRAMS konsolerun.desktop  DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus )
else()
    install( FILES konsolerun.desktop  DESTINATION ${KDE_INSTALL_KSERVICES5DIR}/ServiceMenus )
endif()
