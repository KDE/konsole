set(demo_konsolepart_SRCS
    main.cpp
    demo_konsolepart.cpp
)

add_executable(demo_konsolepart ${demo_konsolepart_SRCS})

target_link_libraries(demo_konsolepart
    KF${QT_MAJOR_VERSION}::CoreAddons
    KF${QT_MAJOR_VERSION}::I18n
    KF${QT_MAJOR_VERSION}::Parts
    KF${QT_MAJOR_VERSION}::Pty
    Qt::Widgets
    KF${QT_MAJOR_VERSION}::XmlGui
    KF${QT_MAJOR_VERSION}::WindowSystem
)
