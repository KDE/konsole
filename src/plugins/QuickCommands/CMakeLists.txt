
ki18n_wrap_ui(EXTRA_QUICKCOMMANDSPLUGIN_SRCS qcwidget.ui)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED TextEditor)

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

target_link_libraries(konsole_quickcommandsplugin
    konsoleprivate
    konsoleapp
    KF5::TextEditor
)
