add_executable(demo_konsolepart
    main.cpp
    demo_konsolepart.cpp
)

target_link_libraries(demo_konsolepart
    KF6::CoreAddons
    KF6::I18n
    KF6::Parts
    KF6::XmlGui
    KF6::WindowSystem
    Qt::Widgets
)
