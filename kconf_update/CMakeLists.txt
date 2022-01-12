add_executable(konsole_globalaccel
    konsole_globalaccel.cpp
)

target_link_libraries(konsole_globalaccel PRIVATE
    KF5::ConfigCore
    KF5::GlobalAccel
    KF5::Service
)

install(TARGETS konsole_globalaccel DESTINATION ${KDE_INSTALL_LIBDIR}/kconf_update_bin)
install(FILES konsole_globalaccel.upd DESTINATION ${KDE_INSTALL_KCONFUPDATEDIR})
