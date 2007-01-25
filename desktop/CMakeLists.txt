

########### install files ###############

install( FILES konsole.desktop konsolesu.desktop  DESTINATION  ${XDG_APPS_DIR} )
install( FILES terminalemulator.desktop  DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
install( FILES konsolepart.desktop konsole-script.desktop  DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES konsolehere.desktop  DESTINATION  ${DATA_INSTALL_DIR}/konqueror/servicemenus )
install( FILES kwrited.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
install( FILES konsole.notifyrc tips  DESTINATION  ${DATA_INSTALL_DIR}/konsole )
