configure_file (config-konsole.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-konsole.h )


########### next target ###############

set(fontembedder_SRCS fontembedder.cpp )

kde4_automoc(${fontembedder_SRCS})

kde4_add_executable(fontembedder ${fontembedder_SRCS})

target_link_libraries(fontembedder  ${KDE4_KIO_LIBS} )


########### next target ###############

set(konsole_KDEINIT_SRCS 
   TEPty.cpp 
   BlockArray.cpp 
   main.cpp 
   konsole.cpp 
   schema.cpp 
   session.cpp 
   TEWidget.cpp 
   TEmuVt102.cpp 
   TEScreen.cpp 
   TEmulation.cpp 
   TEHistory.cpp 
   keytrans.cpp 
   konsole_wcwidth.cpp 
   konsolebookmarkhandler.cpp 
   konsolebookmarkmenu.cpp 
   zmodem_dialog.cpp 
   printsettings.cpp 
   konsoleadaptor.cpp
   sessionadaptor.cpp
   konsolescriptingadaptor.cpp
   sessionscriptingadaptor.cpp )

kde4_automoc(${konsole_KDEINIT_SRCS})


kde4_add_kdeinit_executable( konsole ${konsole_KDEINIT_SRCS})

target_link_libraries(kdeinit_konsole ${KDE4_KDE3SUPPORT_LIBS} ${KDE4_KDEPRINT_LIBS} ${QT_QTXML_LIBRARY} )
if(X11_XTest_FOUND)
  target_link_libraries(kdeinit_konsole ${X11_XTest_LIB} )
endif(X11_XTest_FOUND)

install(TARGETS kdeinit_konsole  DESTINATION ${LIB_INSTALL_DIR} )

target_link_libraries( konsole kdeinit_konsole )
install(TARGETS konsole DESTINATION bin)

########### next target ###############

set(konsolepart_PART_SRCS 
   konsoleadaptor.cpp
   sessionadaptor.cpp
   konsolescriptingadaptor.cpp
   sessionscriptingadaptor.cpp
   TEPty.cpp 
   BlockArray.cpp 
   konsole_part.cpp 
   schema.cpp 
   session.cpp 
   zmodem_dialog.cpp 
   TEWidget.cpp 
   TEmuVt102.cpp 
   TEScreen.cpp 
   konsole_wcwidth.cpp 
   TEmulation.cpp 
   TEHistory.cpp 
   keytrans.cpp )

kde4_automoc(${konsolepart_PART_SRCS})


kde4_add_plugin(konsolepart WITH_PREFIX ${konsolepart_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} konsolepart )

target_link_libraries(konsolepart  ${KDE4_KDE3SUPPORT_LIBS}  )
if(X11_XTest_LIB)
  target_link_libraries(konsolepart  ${X11_XTest_LIB}  )
endif(X11_XTest_LIB)

install(TARGETS konsolepart  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### next target ###############

# kvh - Temp disable kwrited until KDED and dbus are completed.

#set(kded_kwrited_PART_SRCS kwrited.cpp )

#kde4_automoc(${kded_kwrited_PART_SRCS})


#kde4_add_plugin(kded_kwrited ${kded_kwrited_PART_SRCS})

#kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kded_kwrited )

#target_link_libraries(kded_kwrited  ${KDE4_KDE3SUPPORT_LIBS})

#install(TARGETS kded_kwrited  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES x-konsole.desktop  DESTINATION  ${MIME_INSTALL_DIR}/application )




#original Makefile.am contents follow:

## This file is part of Konsole - an X terminal for KDE
#
## set the include path for X, qt and KDE
#INCLUDES = -I. $(all_includes)
#
## you can add here more. This one gets installed
## bin_PROGRAMS = kcmkonsole
#bin_PROGRAMS =
#noinst_PROGRAMS = fontembedder
#lib_LTLIBRARIES = 
#kdeinit_LTLIBRARIES = konsole.la
#kde_module_LTLIBRARIES = libkonsolepart.la kded_kwrited.la
#
#libkonsolepart_la_SOURCES = TEPty.cpp BlockArray.cpp konsole_part.cpp \
#	schema.cpp \
#	session.cpp \
#	zmodem_dialog.cpp \
#	TEWidget.cpp \
#	TEmuVt102.cpp \
#	TEScreen.cpp konsole_wcwidth.cpp \
#	TEmulation.cpp \
#	TEHistory.cpp \
#	keytrans.cpp \
#	konsoleiface.skel \
#        sessioniface.skel
#
#libkonsolepart_la_LDFLAGS = $(KDE_PLUGIN) $(all_libraries)
#libkonsolepart_la_LIBADD = $(XTESTLIB) $(LIB_KPARTS)
#
#fontembedder_SOURCES = fontembedder.cpp
#fontembedder_LDFLAGS = $(all_libraries)
#fontembedder_LDADD = $(LIB_QT)
#
## kwrited kded module
#kded_kwrited_la_SOURCES = kwrited.cpp kwrited.skel
#kded_kwrited_la_LIBADD  = $(LIB_KDECORE) 
#kded_kwrited_la_LDFLAGS = $(all_libraries) -module -avoid-version
#
## konsole kdeinit module
#konsole_la_SOURCES = TEPty.cpp BlockArray.cpp main.cpp konsole.cpp schema.cpp session.cpp TEWidget.cpp TEmuVt102.cpp \
#     TEScreen.cpp TEmulation.cpp TEHistory.cpp keytrans.cpp konsoleiface.skel sessioniface.skel \
#     konsole_wcwidth.cpp konsolebookmarkhandler.cpp konsolebookmarkmenu.cpp \
#     zmodem_dialog.cpp printsettings.cpp
#konsole_la_LDFLAGS = $(all_libraries) -module -avoid-version
#konsole_la_LIBADD = $(LIB_KDEUI) $(LIB_KIO) $(LIB_KDEPRINT) $(LIBUTIL) $(XTESTLIB) $(LIB_XRENDER)
#
## kcmkonsole_SOURCES = kcmkonsole.cpp schema.cpp
## kcmkonsole_LDADD   = $(LIB_KDEUI)
## kcmkonsole_LDFLAGS = $(all_libraries) $(KDE_RPATH)
#
#noinst_HEADERS = TEWidget.h TEPty.h TEmulation.h TEmuVt102.h \
#	TECommon.h TEScreen.h konsole.h schema.h session.h konsole_wcwidth.h \
#	kwrited.h TEHistory.h keytrans.h default.keytab.h BlockArray.h \
#        konsolebookmarkhandler.h konsolebookmarkmenu.h zmodem_dialog.h \
#        printsettings.h linefont.h
#
#METASOURCES = AUTO
#
#messages: rc.cpp
#	rm -f schemas.cpp
#	rm -f tips.cpp
#	(cd .. && $(PREPARETIPS) > konsole/tips.cpp)
#	for i in ../other/*.schema; do \
#	  grep "^title" $$i | cut -d':' -f2- | sed -e 's#^title \(.*\)$$#i18n(\"\1\")#' >> schemas.cpp ;\
#	done
#	for i in ../other/*.Keytab ../other/*.keytab; do \
#	  grep "^keyboard" $$i | sed -e 's#^keyboard \"\(.*\)\"$$#i18n(\"\1\")#' >> schemas.cpp ;\
#	done
#	$(XGETTEXT) *.cpp -o $(podir)/konsole.pot
#	rm -f schemas.cpp
#	rm -f tips.cpp
#
## setting up x-application/konsole as a special mimetype
#mimedir = $(kde_mimedir)/application
#mime_DATA = x-konsole.desktop
#
#EXTRA_DIST = $(mime_DATA)
#
#fonts: fontembedder
#	./fontembedder $(srcdir)/linefont.src > linefont.h
