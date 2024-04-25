set(demo_konsolepart_SRCS
    main.cpp
    demo_konsolepart.cpp
)

add_executable(demo_konsolepart ${demo_konsolepart_SRCS})

target_link_libraries(demo_konsolepart
    KF6::CoreAddons
    KF6::I18n
    KF6::Parts
    KF6::Service
    Qt::Widgets
    KF6::XmlGui
    KF6::WindowSystem
)
