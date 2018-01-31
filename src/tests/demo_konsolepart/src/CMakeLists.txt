set(demo_konsolepart_SRCS
    main.cpp
    demo_konsolepart.cpp
)

add_executable(demo_konsolepart ${demo_konsolepart_SRCS})

target_link_libraries(demo_konsolepart
    KF5::CoreAddons
    KF5::I18n
    KF5::Parts
    KF5::Pty
    Qt5::Widgets
    KF5::XmlGui
    KF5::WindowSystem
)
