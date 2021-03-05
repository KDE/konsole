
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
