kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#EXTRA_DIST = Table.Codes Table.Graphic Table.VT52 \
#	awk.table.graphic db.trans db2.pl genTC.pl t.pl \
#        vt100.gif konsole1.gif konsole2.gif techref.html
