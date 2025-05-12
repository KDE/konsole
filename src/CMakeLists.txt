# cmake-options : -DCMAKE_DISABLE_FIND_PACKAGE_LibKonq=TRUE or FALSE; default is FALSE

add_definitions(-DTRANSLATION_DOMAIN=\"konsole\")

### Handle DragonFlyBSD here instead of using __DragonFly__
if(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
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
        find_package(Qt6Test ${QT_MIN_VERSION} CONFIG REQUIRED)
        add_subdirectory(autotests)
        add_subdirectory(tests)
endif()


### Security concerns about sendText and runCommand dbus methods being public
option(REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS "Konsole: remove sendText and runCommand dbus methods" OFF)

### Security concerns about reading arbitrary screen positions
option(ENABLE_DECRQCRA "Konsole: enable DEC request checksum rectangular area" OFF)

### Development tools
option(KONSOLE_BUILD_UNI2CHARACTERWIDTH "Konsole: build uni2characterwidth executable" OFF)

set(dbus_xml_srcs)
if(HAVE_DBUS)
    ### Konsole source files shared between embedded terminal and main application
    # qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
    # qdbuscpp2xml -M -s ViewManager.h -o org.kde.konsole.Konsole.xml

    # Generate dbus .xml files; do not store .xml in source folder
    qt_generate_dbus_interface(ViewManager.h org.kde.konsole.Window.xml OPTIONS -m)

    qt_add_dbus_adaptor(windowadaptors_SRCS
                        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
                        ViewManager.h
                        Konsole::ViewManager)

    # qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
    # Generate dbus .xml files; do not store .xml in source folder
    qt_generate_dbus_interface(session/Session.h org.kde.konsole.Session.xml OPTIONS -m)

    qt_add_dbus_adaptor(
        sessionadaptors_SRCS
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
        session/Session.h
        Konsole::Session
    )

    set(dbus_xml_srcs
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
        ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Window.xml
    )
endif()

set(konsole_LIBS
    KF6::XmlGui
    Qt::Multimedia
    Qt::PrintSupport
    Qt::Xml
    KF6::Notifications
    KF6::WindowSystem
    KF6::TextWidgets
    KF6::GuiAddons
    KF6::IconThemes
    KF6::Bookmarks
    KF6::I18n
    KF6::KIOWidgets
    KF6::NewStuffCore
)

if(NOT WIN32)
    list(APPEND konsole_LIBS
        KF6::Pty
    )
endif()

if(HAVE_DBUS)
    list(APPEND konsole_LIBS
        KF6::DBusAddons
        KF6::GlobalAccel
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    #kinfo_getfile() is in libutil
    list(APPEND konsole_LIBS util)
endif()

if(HAIKU)
    list(APPEND konsole_LIBS network) # for undefined reference to `inet_aton'
endif()

# TODO: Move this to an internal folder if it grows too much.
# This konsoleprivate_core is the "core" library that depends in nothing from konsole itself
# so we can start to detangle the codebase.
set(konsoleprivate_core_SRCS
    ShellCommand.cpp
)

ecm_qt_declare_logging_category(
    konsoleprivate_core_SRCS
    HEADER konsoledebug.h
    IDENTIFIER KonsoleDebug
    CATEGORY_NAME org.kde.konsole
    DESCRIPTION "Konsole"
    EXPORT KONSOLE
)

add_library(konsoleprivate_core STATIC ${konsoleprivate_core_SRCS})
# Needed to link this static lib to shared libs
set_target_properties(konsoleprivate_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(konsoleprivate_core KF6::CoreAddons)

set(konsolehelpers_SRCS
    LabelsAligner.cpp
)
add_library(konsolehelpers
    STATIC
    ${konsolehelpers_SRCS}
)
set_target_properties(konsolehelpers PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(konsolehelpers
    Qt::Core
    Qt::Widgets
)

add_subdirectory(colorscheme)
add_subdirectory(keyboardtranslator)
add_subdirectory(characters)
add_subdirectory(decoders)
add_subdirectory(plugins)
if(WIN32)
    set(ptyqt_srcs ptyqt/iptyprocess.cpp ptyqt/conptyprocess.cpp)
endif()

set(konsoleprivate_SRCS ${windowadaptors_SRCS}
                        BookmarkHandler.cpp
                        BookmarkMenu.cpp
                        CheckableSessionModel.cpp
                        CopyInputDialog.cpp
                        Emulation.cpp
                        EscapeSequenceUrlExtractor.cpp
                        FontDialog.cpp
                        HistorySizeDialog.cpp
                        KeyBindingEditor.cpp
                        LabelsAligner.cpp
                        NullProcessInfo.cpp
                        PrintOptions.cpp
                        ProcessInfo.cpp
                        Pty.cpp
                        RenameTabDialog.cpp
                        SSHProcessInfo.cpp
                        SaveHistoryTask.cpp
                        SaveHistoryAutoTask.cpp
                        Screen.cpp
                        ScreenWindow.cpp
                        ScrollState.cpp
                        SearchHistoryTask.cpp
                        ShouldApplyProperty.cpp
                        UnixProcessInfo.cpp
                        ViewManager.cpp
                        ViewProperties.cpp
                        Vt102Emulation.cpp
                        WindowSystemInfo.cpp
                        ZModemDialog.cpp
                        filterHotSpots/EscapeSequenceUrlFilter.cpp
                        filterHotSpots/EscapeSequenceUrlFilterHotSpot.cpp
                        filterHotSpots/FileFilter.cpp
                        filterHotSpots/FileFilterHotspot.cpp
                        filterHotSpots/Filter.cpp
                        filterHotSpots/FilterChain.cpp
                        filterHotSpots/HotSpot.cpp
                        filterHotSpots/RegExpFilter.cpp
                        filterHotSpots/RegExpFilterHotspot.cpp
                        filterHotSpots/TerminalImageFilterChain.cpp
                        filterHotSpots/UrlFilter.cpp
                        filterHotSpots/UrlFilterHotspot.cpp
                        filterHotSpots/ColorFilter.cpp
                        filterHotSpots/ColorFilterHotSpot.cpp
                        history/HistoryFile.cpp
                        history/HistoryScroll.cpp
                        history/HistoryScrollFile.cpp
                        history/HistoryScrollNone.cpp
                        history/HistoryType.cpp
                        history/HistoryTypeFile.cpp
                        history/HistoryTypeNone.cpp
                        history/compact/CompactHistoryScroll.cpp
                        history/compact/CompactHistoryType.cpp
                        widgets/DetachableTabBar.cpp
                        widgets/EditProfileDialog.cpp
                        widgets/HistorySizeWidget.cpp
                        widgets/IncrementalSearchBar.cpp
                        widgets/IncrementalSearchBarSizeGrip.cpp
                        widgets/RenameTabWidget.cpp
                        widgets/TabTitleFormatButton.cpp

                        searchtabs/SearchTabs.cpp
                        searchtabs/SearchTabsModel.cpp

                        terminalDisplay/extras/CompositeWidgetFocusWatcher.cpp
                        terminalDisplay/extras/AutoScrollHandler.cpp
                        terminalDisplay/extras/HighlightScrolledLines.cpp

                        terminalDisplay/TerminalDisplay.cpp
                        terminalDisplay/TerminalPainter.cpp
                        terminalDisplay/TerminalScrollBar.cpp
                        terminalDisplay/TerminalColor.cpp
                        terminalDisplay/TerminalFonts.cpp
                        terminalDisplay/TerminalBell.cpp

                        profile/Profile.cpp
                        profile/ProfileCommandParser.cpp
                        profile/ProfileGroup.cpp
                        profile/ProfileList.cpp
                        profile/ProfileReader.cpp
                        profile/ProfileWriter.cpp
                        profile/ProfileManager.cpp
                        profile/ProfileModel.cpp

                        ${ptyqt_srcs} # windows conpty backend

                        ${sessionadaptors_SRCS}
                        session/Session.cpp
                        session/SessionController.cpp
                        session/SessionDisplayConnection.cpp
                        session/SessionGroup.cpp
                        session/SessionListModel.cpp
                        session/SessionManager.cpp
                        session/SessionTask.cpp

                        widgets/TerminalDisplayAccessible.cpp
                        widgets/TerminalHeaderBar.cpp
                        widgets/ViewContainer.cpp
                        widgets/ViewSplitter.cpp
                        widgets/KonsolePrintManager.cpp

                        ${dbus_xml_srcs}

                        ../data/data.qrc
                        ../desktop/konsole.qrc)

kconfig_add_kcfg_files(konsoleprivate_SRCS settings/KonsoleSettings.kcfgc)

### Konsole Application

ki18n_wrap_ui(konsoleprivate_SRCS
    CopyInputDialog.ui
    HistorySizeDialog.ui
    KeyBindingEditor.ui
    PrintOptions.ui       # Temporary to build session static library
    RenameTabDialog.ui
    settings/GeneralSettings.ui
    settings/MemorySettings.ui
    settings/PartInfo.ui
    settings/ProfileSettings.ui
    settings/TabBarSettings.ui
    settings/TemporaryFilesSettings.ui
    settings/ThumbnailsSettings.ui
    widgets/EditProfileAdvancedPage.ui
    widgets/EditProfileAppearancePage.ui
    widgets/EditProfileGeneralPage.ui
    widgets/EditProfileKeyboardPage.ui
    widgets/EditProfileMousePage.ui
    widgets/EditProfileScrollingPage.ui
    widgets/EditProfileTabsPage.ui
    widgets/HistorySizeWidget.ui
    widgets/RenameTabWidget.ui
)

add_library(konsoleprivate ${konsoleprivate_SRCS})
generate_export_header(konsoleprivate BASE_NAME konsoleprivate)

find_package(ZLIB)

target_link_libraries(konsoleprivate
    PUBLIC
    konsoleprivate_core
    konsolecolorscheme
    keyboardtranslator
    konsolehelpers
    konsolecharacters
    konsoledecoders
    KF6::NewStuffCore
    KF6::NewStuffWidgets
    ${konsole_LIBS}
    ZLIB::ZLIB
    ICU::uc
    ICU::i18n
)

target_link_libraries(konsoleprivate
    PRIVATE
    KF6::IconWidgets
    KF6::BookmarksWidgets
)

set_target_properties(konsoleprivate PROPERTIES
    VERSION ${RELEASE_SERVICE_VERSION}
)

install(TARGETS konsoleprivate ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

add_library(konsoleapp SHARED Application.cpp
    MainWindow.cpp
    settings/ConfigurationDialog.cpp
    settings/ConfigDialogButtonGroupManager.cpp
    settings/TemporaryFilesSettings.cpp
    settings/GeneralSettings.cpp
    settings/MemorySettings.cpp
    settings/ProfileSettings.cpp
    settings/TabBarSettings.cpp
    settings/ThumbnailsSettings.cpp
    pluginsystem/IKonsolePlugin.cpp
    pluginsystem/PluginManager.cpp
    delegates/ProfileShortcutDelegate.cpp
)

generate_export_header(konsoleapp BASE_NAME konsoleapp)

target_compile_definitions(konsoleapp PRIVATE -DRELEASE_SERVICE_VERSION="${RELEASE_SERVICE_VERSION}")

target_link_libraries(konsoleapp
  konsoleprivate
  KF6::XmlGui
  KF6::WindowSystem
  KF6::Bookmarks
  KF6::I18n
  KF6::KIOWidgets
  KF6::NotifyConfig
  KF6::Crash
)

set_target_properties(konsoleapp PROPERTIES
    VERSION ${RELEASE_SERVICE_VERSION}
)

install(TARGETS konsoleapp ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

set(konsole_SRCS
    main.cpp
)

# Sets the icon on Windows and OSX
ecm_add_app_icon(ICONS_SOURCES ICONS ${ICONS_SRCS})

add_executable(konsole ${konsole_SRCS} ${ICONS_SOURCES})
target_link_libraries(konsole
  konsoleprivate
  konsoleapp
  KF6::XmlGui
  KF6::WindowSystem
  KF6::Bookmarks
  KF6::I18n
  KF6::KIOWidgets
  KF6::NotifyConfig
  KF6::Crash
)

if(APPLE)
    set_target_properties(konsole PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.konsole"
        MACOSX_BUNDLE_BUNDLE_NAME "Konsole"
        MACOSX_BUNDLE_DISPLAY_NAME "Konsole"
        MACOSX_BUNDLE_INFO_STRING "Konsole, the KDE terminal emulator"
        MACOSX_BUNDLE_LONG_VERSION_STRING "Konsole ${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${RELEASE_SERVICE_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "1997-2022 The Konsole Developers")
endif()

install(TARGETS konsole ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

### Embedded Konsole KPart

set(konsolepart_PART_SRCS Part.cpp
    settings/PartInfo.cpp
    settings/ProfileSettings.cpp
    delegates/ProfileShortcutDelegate.cpp
)

add_library(konsolepart MODULE ${konsolepart_PART_SRCS})
generate_export_header(konsolepart BASE_NAME konsole)
set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)
target_link_libraries(konsolepart
    KF6::Parts
    KF6::XmlGui
    konsoleprivate
)

install(TARGETS konsolepart DESTINATION ${KDE_INSTALL_PLUGINDIR}/kf6/parts)

# Add cmake-options -DBUILD_COVERAGE=ON -DCOVERAGE_GENERATE_HTML=ON
if(COVERAGE_GENERATE_HTML)
  # find required tools - no error checking
  find_program(LCOV lcov REQUIRED)
  find_program(GENHTML genhtml REQUIRED)

  add_custom_target(coverage
    # runs tests to get .gcda files
    COMMAND ctest --progress
    # generate data
    COMMAND ${LCOV} --directory . --capture --output-file coverage.info
    # generate report
    COMMAND ${GENHTML} --ignore-errors source --demangle-cpp -o coverage coverage.info
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

