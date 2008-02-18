macro_optional_find_package(XKB)
macro_bool_to_01(X11_Xrender_FOUND HAVE_XRENDER)
macro_log_feature(XKB_FOUND "XKB" "X keyboard extension" "http://www.x.org" FALSE "" "Gives Konsole better keyboard support.")

configure_file (config-konsole.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h )

### Font Embedder

set(fontembedder_SRCS fontembedder.cpp )
kde4_add_executable(fontembedder ${fontembedder_SRCS})
target_link_libraries(fontembedder  ${KDE4_KIO_LIBS} )

### Line graphics font

OPTION(KONSOLE_GENERATE_LINEFONT "Konsole: regenerate LineFont file" OFF)

if(KONSOLE_GENERATE_LINEFONT)
	add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/LineFont.h COMMAND ${CMAKE_CURRENT_BINARY_DIR}/fontembedder ARGS ${CMAKE_SOURCE_DIR}/LineFont.src DEPENDS ${CMAKE_SOURCE_DIR}/LineFont.src )
endif(KONSOLE_GENERATE_LINEFONT)

### Konsole Application 

qt4_add_dbus_adaptor( sessionadaptors_SRCS org.kde.konsole.Session.xml Session.h Konsole::Session )

set(konsole_KDEINIT_SRCS
        ${sessionadaptors_SRCS}
        Application.cpp
        BlockArray.cpp
        BookmarkHandler.cpp
        ColorScheme.cpp
        ColorSchemeEditor.cpp
        EditProfileDialog.cpp
        Emulation.cpp
        Filter.cpp
        History.cpp
        HistorySizeDialog.cpp
        IncrementalSearchBar.cpp
        KeyBindingEditor.cpp
        KeyboardTranslator.cpp
        MainWindow.cpp
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
        XKB.cpp
        ZModemDialog.cpp
        konsole_wcwidth.cpp
        main.cpp
   ) 


kde4_add_ui_files(konsole_KDEINIT_SRCS
                  ColorSchemeEditor.ui
                  EditProfileDialog.ui
                  KeyBindingEditor.ui
                  ManageProfilesDialog.ui
                  RemoteConnectionDialog.ui
                 )

kde4_add_kdeinit_executable( konsole ${konsole_KDEINIT_SRCS})

target_link_libraries(kdeinit_konsole ${KDE4_KPTY_LIBRARY} ${KDE4_KIO_LIBS} ${QT_QTXML_LIBRARY} ${KDE4_KNOTIFYCONFIG_LIBS} )

# XRender library for true transparency support    
if(X11_Xrender_FOUND)
    target_link_libraries(kdeinit_konsole ${X11_Xrender_LIB} )
endif(X11_Xrender_FOUND)
    
if(X11_XTest_FOUND)
  target_link_libraries(kdeinit_konsole ${X11_XTest_LIB} )
endif(X11_XTest_FOUND)

install(TARGETS kdeinit_konsole  DESTINATION ${INSTALL_TARGETS_DEFAULT_ARGS} )

    
target_link_libraries( konsole kdeinit_konsole )
install(TARGETS konsole DESTINATION ${BIN_INSTALL_DIR})

### konsoleprofile command-line tool

install(FILES konsoleprofile PERMISSIONS OWNER_READ 
                                         OWNER_WRITE 
                                         OWNER_EXECUTE
                                         GROUP_EXECUTE
                                         GROUP_READ
                                         WORLD_READ 
                                         WORLD_EXECUTE DESTINATION ${BIN_INSTALL_DIR})


### Embedded Konsole KPart

set(konsolepart_PART_SRCS 
   ${sessionadaptors_SRCS}
   BlockArray.cpp
   BookmarkHandler.cpp 
   ColorScheme.cpp
   ColorSchemeEditor.cpp
   EditProfileDialog.cpp
   Emulation.cpp 
   Filter.cpp 
   History.cpp
   HistorySizeDialog.cpp
   IncrementalSearchBar.cpp
   KeyBindingEditor.cpp 
   KeyboardTranslator.cpp
   Part.cpp
   ProcessInfo.cpp
   Profile.cpp
   Pty.cpp 
   Screen.cpp 
   ScreenWindow.cpp
   Session.cpp
   SessionController.cpp
   ShellCommand.cpp
   ProfileList.cpp
   ProfileListWidget.cpp 
   SessionManager.cpp
   ManageProfilesDialog.cpp
   TabTitleFormatAction.cpp
   TerminalCharacterDecoder.cpp
   TerminalDisplay.cpp
   ViewContainer.cpp
   ViewManager.cpp
   ViewProperties.cpp 
   ViewSplitter.cpp
   Vt102Emulation.cpp 
   XKB.cpp   
   ZModemDialog.cpp 
   konsole_wcwidth.cpp 
) 

kde4_add_plugin(konsolepart WITH_PREFIX ${konsolepart_PART_SRCS})

set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)

target_link_libraries(konsolepart ${KDE4_KPARTS_LIBS} ${KDE4_KPTY_LIBRARY} ${KDE4_KIO_LIBS} ${KDE4_KNOTIFYCONFIG_LIBS})
if(X11_XTest_LIB)
  target_link_libraries(konsolepart  ${X11_XTest_LIB}  )
endif(X11_XTest_LIB)

install(TARGETS konsolepart  DESTINATION ${PLUGIN_INSTALL_DIR} )


### KWriteD Daemon

set(kded_kwrited_PART_SRCS kwrited.cpp )

kde4_add_plugin(kded_kwrited ${kded_kwrited_PART_SRCS})

target_link_libraries(kded_kwrited  ${KDE4_KDEUI_LIBS} ${KDE4_KPTY_LIBS})

install(TARGETS kded_kwrited  DESTINATION ${PLUGIN_INSTALL_DIR} )


