
kcoreaddons_add_plugin(konsole_quickcommandsplugin
SOURCES
    quickcommandsplugin.cpp
    quickcommandswidget.cpp
    quickcommandsmodel.cpp
    filtermodel.cpp
INSTALL_NAMESPACE
    "konsoleplugins"
)

ki18n_wrap_ui(konsole_quickcommandsplugin qcwidget.ui)

configure_file(konsole_quickcommands.in.json konsole_quickcommands.json)
target_link_libraries(konsole_quickcommandsplugin
    konsoleprivate
    konsoleapp
    konsolelogging
    KF6::CoreAddons
    KF6::I18n
)
