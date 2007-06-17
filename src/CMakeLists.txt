macro_optional_find_package(XKB)
macro_bool_to_01(X11_Xrender_FOUND HAVE_XRENDER)
if (CMAKE_SYSTEM_NAME MATCHES Linux)
   set (HAVE_PROC_CWD 1)
endif (CMAKE_SYSTEM_NAME MATCHES Linux)
configure_file (config-konsole.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h )

OPTION(KONSOLE_GENERATE_LINEFONT "Konsole: regenerate linefont file" OFF)

########### next target ###############

set(fontembedder_SRCS fontembedder.cpp )

kde4_automoc(${fontembedder_SRCS})

kde4_add_executable(fontembedder ${fontembedder_SRCS})

target_link_libraries(fontembedder  ${KDE4_KIO_LIBS} )

#qt4_add_dbus_adaptor( konsoleadaptors_SRCS org.kde.konsole.Konsole.xml konsole.h Konsole )
#qt4_add_dbus_adaptor( konsoleadaptors_SRCS org.kde.konsole.KonsoleScripting.xml konsole.h Konsole )
#qt4_add_dbus_adaptor( sessionadaptors_SRCS org.kde.konsole.Session.xml Session.h Session )
#qt4_add_dbus_adaptor( sessionadaptors_SRCS org.kde.konsole.SessionScripting.xml Session.h Session )

if(KONSOLE_GENERATE_LINEFONT)
	add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/linefont.h COMMAND ${CMAKE_CURRENT_BINARY_DIR}/fontembedder ARGS ${CMAKE_SOURCE_DIR}/linefont.src DEPENDS ${CMAKE_SOURCE_DIR}/linefont.src )
endif(KONSOLE_GENERATE_LINEFONT)

########### next target ###############

#set(konsole_KDEINIT_SRCS 
#   ${konsoleadaptors_SRCS}
#   ${sessionadaptors_SRCS}
#   Pty.cpp 
#   BlockArray.cpp 
#   main.cpp 
#   konsole.cpp 
#   schema.cpp 
#   Session.cpp 
#   TerminalDisplay.cpp 
#   Vt102Emulation.cpp 
#   Screen.cpp 
#   Emulation.cpp 
#   History.cpp 
#   keytrans.cpp 
#   konsole_wcwidth.cpp 
#   konsolebookmarkhandler.cpp 
#   zmodem_dialog.cpp 
#   printsettings.cpp 
#   TerminalCharacterDecoder.cpp
#   SessionManager.cpp
#   ViewSplitter.cpp
#   ViewContainer.cpp  
#   NavigationItem.cpp )

set(konsole_KDEINIT_SRCS
        ${sessionadaptors_SRCS}
        BlockArray.cpp
        ColorScheme.cpp
        ColorSchemeEditor.cpp
        EditProfileDialog.cpp
        Filter.cpp
        HistorySizeDialog.cpp
        IncrementalSearchBar.cpp
        KeyboardTranslator.cpp
        KeyBindingEditor.cpp
        Application.cpp
        BookmarkHandler.cpp
        MainWindow.cpp
        ProcessInfo.cpp
        ScreenWindow.cpp
        SessionController.cpp
        ShellCommand.cpp
        Profile.cpp
        ProfileList.cpp
        ProfileListWidget.cpp
        SessionManager.cpp
        ManageProfilesDialog.cpp
        History.cpp
        Pty.cpp
        Screen.cpp
        Session.cpp
        TabTitleFormatAction.cpp
        TerminalDisplay.cpp
        Vt102Emulation.cpp
        Emulation.cpp
        RemoteConnectionDialog.cpp
        TerminalCharacterDecoder.cpp
        ViewContainer.cpp
        ViewManager.cpp
        ViewProperties.cpp
        ViewSplitter.cpp
        ZModemDialog.cpp
        konsole_wcwidth.cpp
        start.cpp
        XKB.cpp
   ) 


kde4_add_ui_files(konsole_KDEINIT_SRCS
                  ColorSchemeEditor.ui
                  EditProfileDialog.ui
                  KeyBindingEditor.ui
                  ManageProfilesDialog.ui
                  RemoteConnectionDialog.ui
                 )
kde4_automoc(${konsole_KDEINIT_SRCS})

kde4_add_kdeinit_executable( konsole ${konsole_KDEINIT_SRCS})

target_link_libraries(kdeinit_konsole  ${KDE4_KDEPRINT_LIBS} ${QT_QTXML_LIBRARY} ${KDE4_KNOTIFYCONFIG_LIBS} ${KDE4_KFILE_LIBS} )

# XRender library for true transparency support    
if(X11_Xrender_FOUND)
    target_link_libraries(kdeinit_konsole ${X11_Xrender_LIB} )
endif(X11_Xrender_FOUND)
    
if(X11_XTest_FOUND)
  target_link_libraries(kdeinit_konsole ${X11_XTest_LIB} )
endif(X11_XTest_FOUND)

install(TARGETS kdeinit_konsole  DESTINATION ${LIB_INSTALL_DIR} )

    
target_link_libraries( konsole kdeinit_konsole )
install(TARGETS konsole DESTINATION ${BIN_INSTALL_DIR})
install(FILES konsoleprofile PERMISSIONS OWNER_READ 
                                         OWNER_WRITE 
                                         OWNER_EXECUTE
                                         GROUP_EXECUTE
                                         GROUP_READ
                                         WORLD_READ 
                                         WORLD_EXECUTE DESTINATION ${BIN_INSTALL_DIR})
########### next target ###############

set(konsolepart_PART_SRCS 
#${sessionadaptors_SRCS}
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
   Part.cpp
   ProcessInfo.cpp
   Profile.cpp
   Pty.cpp 
   RemoteConnectionDialog.cpp
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


kde4_automoc(${konsolepart_PART_SRCS})

kde4_add_plugin(konsolepart WITH_PREFIX ${konsolepart_PART_SRCS})

set_target_properties(konsolepart PROPERTIES DEFINE_SYMBOL KONSOLE_PART)

target_link_libraries(konsolepart ${KDE4_KDEUI_LIBS} ${KDE4_KPARTS_LIBS} ${KDE4_KFILE_LIBS} )
if(X11_XTest_LIB)
  target_link_libraries(konsolepart  ${X11_XTest_LIB}  )
endif(X11_XTest_LIB)

install(TARGETS konsolepart  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############

set(kded_kwrited_PART_SRCS kwrited.cpp )

kde4_automoc(${kded_kwrited_PART_SRCS})


kde4_add_plugin(kded_kwrited ${kded_kwrited_PART_SRCS})


target_link_libraries(kded_kwrited  ${KDE4_KDE3SUPPORT_LIBS})

install(TARGETS kded_kwrited  DESTINATION ${PLUGIN_INSTALL_DIR} )


