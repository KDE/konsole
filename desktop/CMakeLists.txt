

########### install files ###############

# quick-access-konsole.desktop is not installed at this time because 
# the feature is not polished enough to include

install( FILES konsole.desktop DESTINATION  ${XDG_APPS_INSTALL_DIR} )
install( FILES terminalemulator.desktop  DESTINATION  ${SERVICETYPES_INSTALL_DIR} )
install( FILES konsolepart.desktop DESTINATION  ${SERVICES_INSTALL_DIR} )
install( FILES konsolehere.desktop  DESTINATION
${SERVICES_INSTALL_DIR}/ServiceMenus )
install( FILES kwrited.desktop  DESTINATION  ${SERVICES_INSTALL_DIR}/kded )
install( FILES konsole.notifyrc konsoleui.rc sessionui.rc partui.rc DESTINATION  ${DATA_INSTALL_DIR}/konsole )
