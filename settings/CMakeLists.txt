

add_definitions (-DQT3_SUPPORT)

########### next target ###############

set(kcm_konsole_PART_SRCS schemaeditor.cpp sessioneditor.cpp kcmkonsole.cpp )

if(Q_WS_X11)
 set(kdesktop_xml  ${CMAKE_SOURCE_DIR}/workspace/kdesktop/org.kde.kdesktop.Desktop.xml)
 QT4_ADD_DBUS_INTERFACE( kcm_konsole_PART_SRCS ${kdesktop_xml} kdesktop_interface )
 set(kdesktop_background_xml  ${CMAKE_SOURCE_DIR}/workspace/kdesktop/org.kde.kdesktop.Background.xml)
 QT4_ADD_DBUS_INTERFACE( kcm_konsole_PART_SRCS ${kdesktop_background_xml} kdesktop_background_interface )
endif(Q_WS_X11)

set(klauncher_xml  ${DBUS_INTERFACES_DIR}/org.kde.KLauncher.xml)
QT4_ADD_DBUS_INTERFACE( kcm_konsole_PART_SRCS ${klauncher_xml} klauncher_interface )


kde4_automoc(${kcm_konsole_PART_SRCS})

kde4_add_ui_files(kcm_konsole_PART_SRCS kcmkonsoledialog.ui sessiondialog.ui schemadialog.ui )

kde4_add_plugin(kcm_konsole ${kcm_konsole_PART_SRCS})


target_link_libraries(kcm_konsole ${KDE4_KDE3SUPPORT_LIBS} ${KDE4_KDEFX_LIBS})

install(TARGETS kcm_konsole  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES konsole.desktop DESTINATION ${SERVICES_INSTALL_DIR} )

