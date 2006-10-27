configure_file (config-konsole.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h )

OPTION(KONSOLE_GENERATE_LINEFONT "Konsole: regenerate linefont file" OFF)

########### next target ###############

set(fontembedder_SRCS fontembedder.cpp )

kde4_automoc(${fontembedder_SRCS})

kde4_add_executable(fontembedder ${fontembedder_SRCS})

target_link_libraries(fontembedder  ${KDE4_KIO_LIBS} )


if(KONSOLE_GENERATE_LINEFONT)
	add_custom_command(OUTPUT ${CMAKE_SOURCE_DIR}/linefont.h COMMAND ${CMAKE_CURRENT_BINARY_DIR}/fontembedder ARGS ${CMAKE_SOURCE_DIR}/linefont.src DEPENDS ${CMAKE_SOURCE_DIR}/linefont.src )
endif(KONSOLE_GENERATE_LINEFONT)

########### next target ###############

set(konsole_KDEINIT_SRCS 
   TEPty.cpp 
   BlockArray.cpp 
   main.cpp 
   konsole.cpp 
   schema.cpp 
   TESession.cpp 
   TEWidget.cpp 
   TEmuVt102.cpp 
   TEScreen.cpp 
   TEmulation.cpp 
   TEHistory.cpp 
   keytrans.cpp 
   konsole_wcwidth.cpp 
   konsolebookmarkhandler.cpp 
   zmodem_dialog.cpp 
   printsettings.cpp 
   konsoleadaptor.cpp
   sessionadaptor.cpp
   konsolescriptingadaptor.cpp
   sessionscriptingadaptor.cpp 
   TerminalCharacterDecoder.cpp
   SessionManager.cpp
   ViewSplitter.cpp
   ViewContainer.cpp  )

kde4_automoc(${konsole_KDEINIT_SRCS})

kde4_add_kdeinit_executable( konsole ${konsole_KDEINIT_SRCS})

target_link_libraries(kdeinit_konsole  ${KDE4_KDEPRINT_LIBS} ${QT_QTXML_LIBRARY} knotifyconfig )

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

########### next target ###############

set(konsolepart_PART_SRCS 
   sessionadaptor.cpp
   sessionscriptingadaptor.cpp
   TEPty.cpp 
   BlockArray.cpp 
   konsole_part.cpp 
   schema.cpp 
   TESession.cpp 
   zmodem_dialog.cpp 
   TEWidget.cpp 
   TEmuVt102.cpp 
   TEScreen.cpp 
   konsole_wcwidth.cpp 
   TEmulation.cpp 
   TEHistory.cpp 
   keytrans.cpp 
   TerminalCharacterDecoder.cpp
   SessionManager.cpp )

kde4_automoc(${konsolepart_PART_SRCS})


kde4_add_plugin(konsolepart WITH_PREFIX ${konsolepart_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} konsolepart )

target_link_libraries(konsolepart  ${KDE4_KDE3SUPPORT_LIBS}  )
if(X11_XTest_LIB)
  target_link_libraries(konsolepart  ${X11_XTest_LIB}  )
endif(X11_XTest_LIB)

install(TARGETS konsolepart  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############

set(kded_kwrited_PART_SRCS kwrited.cpp )

kde4_automoc(${kded_kwrited_PART_SRCS})


kde4_add_plugin(kded_kwrited ${kded_kwrited_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kded_kwrited )

target_link_libraries(kded_kwrited  ${KDE4_KDE3SUPPORT_LIBS})

install(TARGETS kded_kwrited  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES x-konsole.desktop  DESTINATION  ${MIME_INSTALL_DIR}/application )

