add_executable(konsole_globalaccel
    konsole_globalaccel.cpp
)

target_link_libraries(konsole_globalaccel PRIVATE
    KF6::ConfigCore
    KF6::GlobalAccel
    KF6::Service
)

add_executable(konsole_show_menubar konsole_show_menubar.cpp ../data/data.qrc ../desktop/konsole.qrc)

target_link_libraries(konsole_show_menubar
    KF6::XmlGui
    KF6::CoreAddons
)

install(FILES konsole.upd DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(PROGRAMS konsole_add_hamburgermenu_to_toolbar.sh DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
install(TARGETS konsole_globalaccel konsole_show_menubar DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin)
