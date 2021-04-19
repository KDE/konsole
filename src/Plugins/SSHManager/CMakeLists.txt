
qt_wrap_ui(EXTRA_SSHPLUGIN_SRCS sshwidget.ui)


kcoreaddons_add_plugin(konsole_sshmanagerplugin
SOURCES
    sshmanagerplugin.cpp
    sshmanagerpluginwidget.cpp
    sshmanagermodel.cpp
    sshconfigurationdata.cpp
    ${EXTRA_SSHPLUGIN_SRCS}
INSTALL_NAMESPACE
    "konsoleplugins"
)

kcoreaddons_desktop_to_json(konsole_sshmanagerplugin konsole_sshmanager.desktop)

target_link_libraries(konsole_sshmanagerplugin
    konsoleprivate
    konsolepluginsystem
)
