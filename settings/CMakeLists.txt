


########### next target ###############

set(kcm_konsole_PART_SRCS schemaeditor.cpp sessioneditor.cpp kcmkonsole.cpp )

kde4_automoc(${kcm_konsole_PART_SRCS})

kde4_add_ui3_files(kcm_konsole_PART_SRCS schemadialog.ui )

kde4_add_ui_files(kcm_konsole_PART_SRCS kcmkonsoledialog.ui sessiondialog.ui )

kde4_add_plugin(kcm_konsole ${kcm_konsole_PART_SRCS})


target_link_libraries(kcm_konsole ${KDE4_KDE3SUPPORT_LIBS} ${KDE4_KDEFX_LIBS})

install(TARGETS kcm_konsole  DESTINATION ${PLUGIN_INSTALL_DIR})


########### install files ###############

install( FILES konsole.desktop DESTINATION ${SERVICES_INSTALL_DIR} )

