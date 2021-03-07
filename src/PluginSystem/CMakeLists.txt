
find_package(KF5Service)

add_library(konsolepluginsystem SHARED
    IKonsolePlugin.cpp
    PluginManager.cpp
)

target_link_libraries(konsolepluginsystem
    KF5::Service
    Qt5::Widgets
    konsoleprivate
)

install(TARGETS konsolepluginsystem ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
