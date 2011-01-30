if(NOT WIN32)
    install( FILES
             default.keytab
             linux.keytab 
             #  DON'T INSTALL VT420 KEYBOARD LAYOUT - IT DOESN'T WORK WITH
             #  ACTUAL VT420 SYSTEMS - EITHER RENAME IT OR IMPROVE KONSOLE
             #  vt420pc.keytab 
             solaris.keytab    
             README.KeyTab 
             DESTINATION 
             ${DATA_INSTALL_DIR}/konsole )
else(NOT WIN32)
    configure_file( windows.keytab ${CMAKE_CURRENT_BINARY_DIR}/default.keytab COPYONLY)
    install( FILES
             ${CMAKE_CURRENT_BINARY_DIR}/default.keytab
             windows.keytab
             README.KeyTab 
             DESTINATION 
             ${DATA_INSTALL_DIR}/konsole )
endif(NOT WIN32)
