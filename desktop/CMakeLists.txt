

########### install files ###############

install( FILES konsole.desktop quick-access-konsole.desktop  DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES terminalemulator.desktop  DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
install( FILES konsolepart.desktop konsole-script.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES konsolehere.desktop  DESTINATION
${SERVICES_INSTALL_DIR}/ServiceMenus )
install( FILES kwrited.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
install( FILES konsole.notifyrc tips konsoleui.rc sessionui.rc DESTINATION  ${DATA_INSTALL_DIR}/konsole )
