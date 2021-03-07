
kcoreaddons_add_plugin(konsole_sshmanagerplugin
SOURCES
    sshmanagerplugin.cpp
    sshmanagerpluginwidget.cpp
JSON
    konsole_sshmanager.desktop
INSTALL_NAMESPACE
    "konsoleplugins"
)

target_link_libraries(konsole_sshmanagerplugin
    konsoleprivate
    konsolepluginsystem
)
