
### Compile-time features
macro_optional_find_package(XKB)
macro_bool_to_01(X11_Xrender_FOUND HAVE_XRENDER)
macro_log_feature(XKB_FOUND "XKB" "X keyboard extension" "http://www.x.org" FALSE "" "Gives Konsole better keyboard support.")

configure_file (config-konsole.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h )

### Tests
    add_subdirectory(tests)

### Font Embedder

    set(fontembedder_SRCS fontembedder.cpp )
    kde4_add_executable(fontembedder ${fontembedder_SRCS})
    target_link_libraries(fontembedder  ${KDE4_KIO_LIBS} )

### Line graphics font

    option(KONSOLE_GENERATE_LINEFONT "Konsole: regenerate LineFont file" OFF)

    if(KONSOLE_GENERATE_LINEFONT)
	    add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/LineFont.h
                           COMMAND ${CMAKE_CURRENT_BINARY_DIR}/fontembedder
                           ARGS ${CMAKE_SOURCE_DIR}/LineFont.src
                           DEPENDS ${CMAKE_SOURCE_DIR}/LineFont.src )
    endif(KONSOLE_GENERATE_LINEFONT)

### Konsole source files shared between embedded terminal and main application
    # qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
    # qdbuscpp2xml -M -s ViewManager.h -o org.kde.konsole.Konsole.xml

    qt4_add_dbus_adaptor( sessionadaptors_SRCS
                          org.kde.konsole.Session.xml
                          Session.h
                          Konsole::Session )
    qt4_add_dbus_adaptor( konsoleadaptors_SRCS 
                          org.kde.konsole.Konsole.xml 
                          ViewManager.h 
                          Konsole::ViewManager )


    set(konsoleprivate_SRCS
        ${sessionadaptors_SRCS}
        ${konsoleadaptors_SRCS}
        BlockArray.cpp
        BookmarkHandler.cpp
        ColorScheme.cpp
        ColorSchemeEditor.cpp
        CopyInputDialog.cpp
        EditProfileDialog.cpp
        Emulation.cpp
        Filter.cpp
        History.cpp
        HistorySizeDialog.cpp
        IncrementalSearchBar.cpp
        KeyBindingEditor.cpp
        KeyboardTranslator.cpp
        ManageProfilesDialog.cpp
        ProcessInfo.cpp
        Profile.cpp
        ProfileList.cpp
        ProfileListWidget.cpp
        Pty.cpp
        RemoteConnectionDialog.cpp
        Screen.cpp
        ScreenWindow.cpp
        Session.cpp
        SessionController.cpp
        SessionManager.cpp
        ShellCommand.cpp
        TabTitleFormatAction.cpp
        TerminalCharacterDecoder.cpp
        TerminalDisplay.cpp
        ViewContainer.cpp
        ViewManager.cpp
        ViewProperties.cpp
        ViewSplitter.cpp
        Vt102Emulation.cpp
        WarningBox.cpp
        XKB.cpp
        ZModemDialog.cpp
        konsole_wcwidth.cpp
    )

    set(konsole_LIBS
        ${KDE4_KPTY_LIBRARY}
        ${KDE4_KIO_LIBS}
        ${QT_QTXML_LIBRARY}
        ${KDE4_KNOTIFYCONFIG_LIBRARY}
        ${X11_LIBRARIES}
    )

    # XRender library for true transparency support
    if(X11_Xrender_FOUND)
        list(APPEND konsole_LIBS ${X11_Xrender_LIB})
    endif(X11_Xrender_FOUND)

# doesn't seem to be needed
#    if(X11_XTest_FOUND)
#        list(APPEND konsole_LIBS ${X11_XTest_LIB})
#    endif(X11_XTest_FOUND)

### Konsole Application

    kde4_add_ui_files(konsoleprivate_SRCS
                  ColorSchemeEditor.ui
                  CopyInputDialog.ui
                  EditProfileDialog.ui
                  KeyBindingEditor.ui
                  ManageProfilesDialog.ui
                  RemoteConnectionDialog.ui
                 )

    kde4_add_library(konsoleprivate SHARED ${konsoleprivate_SRCS})
    target_link_libraries(konsoleprivate ${konsole_LIBS})
    target_link_libraries(konsoleprivate LINK_INTERFACE_LIBRARIES ${konsole_LIBS})

    set(konsole_KDEINIT_SRCS
        Application.cpp
        MainWindow.cpp
        main.cpp
    )

    kde4_add_app_icon(konsole_KDEINIT_SRCS "${KDE4_ICON_INSTALL_DIR}/oxygen/*/apps/utilities-terminal.png")

    kde4_add_kdeinit_executable( konsole ${konsole_KDEINIT_SRCS})
    target_link_libraries(kdeinit_konsole konsoleprivate)

    install(TARGETS kdeinit_konsole konsole konsoleprivate ${INSTALL_TARGETS_DEFAULT_ARGS})

### Embedded Konsole KPart

    set(konsolepart_PART_SRCS
      ${konsole_COMMON_SRCS}
      Part.cpp
   )

    kde4_add_plugin(konsolepart WITH_PREFIX ${konsolepart_PART_SRCS})
    set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)
    target_link_libraries(konsolepart ${KDE4_KPARTS_LIBS} konsoleprivate)
    install(TARGETS konsolepart  DESTINATION ${PLUGIN_INSTALL_DIR} )

### konsoleprofile command-line tool

    install(FILES konsoleprofile PERMISSIONS OWNER_READ
                                         OWNER_WRITE
                                         OWNER_EXECUTE
                                         GROUP_EXECUTE
                                         GROUP_READ
                                         WORLD_READ
                                         WORLD_EXECUTE DESTINATION ${BIN_INSTALL_DIR})
