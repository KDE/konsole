kde4_header()

add_subdirectory( VT100 ) 
include_directories(  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} )


########### install files ###############


kde4_footer()



#original Makefile.am contents follow:

#
#SUBDIRS=VT100
#
#EXTRA_DIST = More README.first histBuffer missing.codes missing.keys
#
