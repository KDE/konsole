
ki18n_wrap_ui(EXTRA_SSHPLUGIN_SRCS sshwidget.ui)

kcoreaddons_add_plugin(konsole_sshmanagerplugin
SOURCES
    sshmanagerplugin.cpp
    sshmanagerpluginwidget.cpp
    sshmanagermodel.cpp
    sshmanagerfiltermodel.cpp
    sshtreeview.cpp
    ${EXTRA_SSHPLUGIN_SRCS}
INSTALL_NAMESPACE
    "konsoleplugins"
)

target_link_libraries(konsole_sshmanagerplugin
    konsoleprivate
    konsoleapp
)
