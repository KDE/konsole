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
