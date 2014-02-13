

########### install files ###############

install( PROGRAMS konsole.desktop DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES terminalemulator.desktop  DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
install( FILES konsolepart.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES konsolehere.desktop DESTINATION ${SERVICES_INSTALL_DIR}/ServiceMenus )
install( FILES konsolerun.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/ServiceMenus )

install( FILES konsole.notifyrc konsoleui.rc sessionui.rc partui.rc DESTINATION  ${DATA_INSTALL_DIR}/konsole )
