kde4_header()

include_directories( ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############

set(kcm_konsole_PART_SRCS schemaeditor.cpp sessioneditor.cpp kcmkonsole.cpp )

kde4_automoc(${kcm_konsole_PART_SRCS})

kde4_add_ui3_files(kcm_konsole_PART_SRCS schemadialog.ui sessiondialog.ui kcmkonsoledialog.ui )

kde4_add_plugin(kcm_konsole ${kcm_konsole_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kcm_konsole )

target_link_libraries(kcm_konsole  ${LIB_KDE3SUPPORT} kdefx )

install_targets(${LIB_INSTALL_DIR}/kde4 kcm_konsole )


########### install files ###############

install_files( ${APPLNK_INSTALL_DIR}/.hidden FILES kcmkonsole.desktop )

kde4_footer()



#original Makefile.am contents follow:

#kde_module_LTLIBRARIES = kcm_konsole.la
#
#kcm_konsole_la_SOURCES =  schemaeditor.cpp sessioneditor.cpp kcmkonsole.cpp \
#	schemadialog.ui sessiondialog.ui kcmkonsoledialog.ui
#
#kcm_konsole_la_LDFLAGS  = $(all_libraries) -module -avoid-version -no-undefined
#kcm_konsole_la_LIBADD = -lkdeui $(LIB_KIO)
#AM_CPPFLAGS = $(all_includes) 
#
#METASOURCES = AUTO 
#
#noinst_HEADERS = kcmkonsole.h schemaeditor.h sessioneditor.h schemadialog.h sessiondialog.h kcmkonsoledialog.h
#
#kcmkonsole_data_DATA = kcmkonsole.desktop
#kcmkonsole_datadir = $(kde_appsdir)/.hidden
#
#messages: rc.cpp
#	$(XGETTEXT) *.cpp -o $(podir)/kcmkonsole.pot
