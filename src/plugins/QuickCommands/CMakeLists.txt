
ki18n_wrap_ui(EXTRA_QUICKCOMMANDSPLUGIN_SRCS qcwidget.ui)

kcoreaddons_add_plugin(konsole_quickcommandsplugin
SOURCES
    quickcommandsplugin.cpp
    quickcommandswidget.cpp
    quickcommandsmodel.cpp
    filtermodel.cpp
    ${EXTRA_QUICKCOMMANDSPLUGIN_SRCS}
INSTALL_NAMESPACE
    "konsoleplugins"
)

configure_file(konsole_quickcommands.in.json konsole_quickcommands.json)
target_link_libraries(konsole_quickcommandsplugin
    konsoleprivate
    konsoleapp
)
