# cmake-options : -DCMAKE_DISABLE_FIND_PACKAGE_LibKonq=TRUE or FALSE; default is FALSE

add_definitions(-DTRANSLATION_DOMAIN=\"konsole\")

### Too many crashes/issues with detaching on MacOSX
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(ENABLE_DETACHING 0)
else()
  set(ENABLE_DETACHING 1)
endif()

### Handle DragonFlyBSD here instead of using __DragonFly__
IF(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
  set(HAVE_OS_DRAGONFLYBSD 1)
else()
  set(HAVE_OS_DRAGONFLYBSD 0)
endif()

include(CheckIncludeFiles)
include(ECMAddAppIcon)

configure_file(config-konsole.h.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h)

### Tests
if(BUILD_TESTING)
  find_package(Qt5Test ${QT_MIN_VERSION} CONFIG REQUIRED)
  add_subdirectory(autotests)
  add_subdirectory(tests)
endif()

### Security concerns about sendText and runCommand dbus methods being public
option(REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS "Konsole: remove sendText and runCommand dbus methods" OFF)

### Font Embedder and LineFont.h
option(KONSOLE_BUILD_FONTEMBEDDER "Konsole: build fontembedder executable" OFF)
option(KONSOLE_GENERATE_LINEFONT "Konsole: regenerate LineFont file" OFF)

### Konsole source files shared between embedded terminal and main application
# qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
# qdbuscpp2xml -M -s ViewManager.h -o org.kde.konsole.Konsole.xml

# Generate dbus .xml files; do not store .xml in source folder
qt5_generate_dbus_interface(Session.h org.kde.konsole.Session.xml OPTIONS -m)
qt5_generate_dbus_interface(ViewManager.h org.kde.konsole.Window.xml OPTIONS -m)

qt5_add_dbus_adaptor(sessionadaptors_SRCS
                    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
                    Session.h
                    Konsole::Session)
qt5_add_dbus_adaptor(windowadaptors_SRCS
                    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
                    ViewManager.h
                    Konsole::ViewManager)

set(konsoleprivate_SRCS ${sessionadaptors_SRCS}
                        ${windowadaptors_SRCS}
                        BookmarkHandler.cpp
                        ColorScheme.cpp
                        ColorSchemeManager.cpp
                        ColorSchemeEditor.cpp
                        CopyInputDialog.cpp
                        EditProfileDialog.cpp
                        Emulation.cpp
                        Filter.cpp
                        History.cpp
                        HistorySizeDialog.cpp
                        HistorySizeWidget.cpp
                        IncrementalSearchBar.cpp
                        KeyBindingEditor.cpp
                        KeyboardTranslator.cpp
                        KeyboardTranslatorManager.cpp
                        ProcessInfo.cpp
                        Profile.cpp
                        ProfileList.cpp
                        ProfileReader.cpp
                        ProfileWriter.cpp
                        ProfileManager.cpp
                        Pty.cpp
                        RenameTabDialog.cpp
                        RenameTabWidget.cpp
                        Screen.cpp
                        ScreenWindow.cpp
                        ScrollState.cpp
                        Session.cpp
                        SessionController.cpp
                        SessionManager.cpp
                        SessionListModel.cpp
                        ShellCommand.cpp
                        TabTitleFormatButton.cpp
                        TerminalCharacterDecoder.cpp
                        ExtendedCharTable.cpp
                        TerminalDisplay.cpp
                        TerminalDisplayAccessible.cpp
                        ViewContainer.cpp
                        ViewContainerTabBar.cpp
                        ViewManager.cpp
                        ViewProperties.cpp
                        ViewSplitter.cpp
                        Vt102Emulation.cpp
                        ZModemDialog.cpp
                        PrintOptions.cpp
                        konsole_wcwidth.cpp
                        WindowSystemInfo.cpp
                        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
                        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml)

ecm_qt_declare_logging_category(konsoleprivate_SRCS HEADER konsoledebug.h IDENTIFIER KonsoleDebug CATEGORY_NAME org.kde.konsole)

kconfig_add_kcfg_files(konsoleprivate_SRCS settings/KonsoleSettings.kcfgc)

set(konsole_LIBS
                 KF5::XmlGui
                 Qt5::PrintSupport
                 Qt5::Xml
                 KF5::Notifications
                 KF5::WindowSystem
                 KF5::TextWidgets
                 KF5::GuiAddons
                 KF5::IconThemes
                 KF5::Bookmarks
                 KF5::I18n
                 KF5::Pty
                 KF5::KIOWidgets
                 KF5::DBusAddons
                 KF5::GlobalAccel
                 KF5::NewStuff
)

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    #kinfo_getfile() is in libutil
    list(APPEND konsole_LIBS util)
endif()

### Konsole Application

ki18n_wrap_ui(konsoleprivate_SRCS ColorSchemeEditor.ui
                                CopyInputDialog.ui
                                EditProfileDialog.ui
                                KeyBindingEditor.ui
                                RenameTabDialog.ui
                                RenameTabWidget.ui
                                HistorySizeDialog.ui
                                HistorySizeWidget.ui
                                PrintOptions.ui
                                settings/FileLocationSettings.ui
                                settings/GeneralSettings.ui
                                settings/PartInfo.ui
                                settings/ProfileSettings.ui
                                settings/TabBarSettings.ui)

# add the resource files for the ui files
qt5_add_resources( konsoleprivate_SRCS ../desktop/konsole.qrc)

add_library(konsoleprivate ${konsoleprivate_SRCS})
generate_export_header(konsoleprivate BASE_NAME konsoleprivate)
target_link_libraries(konsoleprivate PUBLIC ${konsole_LIBS})

set_target_properties(konsoleprivate PROPERTIES
    VERSION ${KONSOLEPRIVATE_VERSION_STRING}
    SOVERSION ${KONSOLEPRIVATE_SOVERSION}
)

install(TARGETS konsoleprivate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

set(konsole_KDEINIT_SRCS
   Application.cpp
   MainWindow.cpp
   main.cpp
   settings/FileLocationSettings.cpp
   settings/GeneralSettings.cpp
   settings/ProfileSettings.cpp
   settings/TabBarSettings.cpp)

kconfig_add_kcfg_files(konsole_KDEINIT_SRCS settings/KonsoleSettings.kcfgc)

# Sets the icon on Windows and OSX
file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../data/icons/*.png")
ecm_add_app_icon(kdeinit_konsole ICONS ${ICONS_SRCS})

kf5_add_kdeinit_executable(konsole ${konsole_KDEINIT_SRCS})
target_link_libraries(kdeinit_konsole konsoleprivate KF5::XmlGui KF5::WindowSystem KF5::Bookmarks
  KF5::I18n
  KF5::KIOWidgets
  KF5::NotifyConfig
  KF5::Crash
)

if(APPLE)
    set_target_properties(konsole PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.konsole"
        MACOSX_BUNDLE_BUNDLE_NAME "Konsole"
        MACOSX_BUNDLE_DISPLAY_NAME "Konsole"
        MACOSX_BUNDLE_INFO_STRING "Konsole, the KDE terminal emulator"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Konsole ${KDE_APPLICATIONS_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${KDE_APPLICATIONS_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "1997-2016 The Konsole Developers")
endif()

install(TARGETS kdeinit_konsole konsole
       ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

### Embedded Konsole KPart

set(konsolepart_PART_SRCS Part.cpp
    settings/PartInfo.cpp
    settings/ProfileSettings.cpp)
kconfig_add_kcfg_files(konsolepart_PART_SRCS settings/KonsoleSettings.kcfgc)
add_library(konsolepart MODULE ${konsolepart_PART_SRCS})
generate_export_header(konsolepart BASE_NAME konsole)
kcoreaddons_desktop_to_json(konsolepart ../desktop/konsolepart.desktop)
set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)
target_link_libraries(konsolepart KF5::Parts KF5::XmlGui konsoleprivate)
install(TARGETS konsolepart  DESTINATION ${KDE_INSTALL_PLUGINDIR})

### konsoleprofile command-line tool
install(PROGRAMS konsoleprofile DESTINATION ${KDE_INSTALL_BINDIR})

