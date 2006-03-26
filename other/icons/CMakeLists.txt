kde4_header()
include_directories(  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE4_INCLUDES} )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/konsole/icons FILES  )

kde4_footer()



#original Makefile.am contents follow:

#konsoleiconsdir = $(kde_datadir)/konsole/icons
#konsoleicons_ICON = AUTO
#
