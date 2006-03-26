kde4_header()

add_subdirectory( icons ) 
include_directories(  ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${KDE4_INCLUDES} )


########### install files ###############

install_files( ${DATA_INSTALL_DIR}/konsole/profiles FILES 2shells 3shells 4shells 5shells )
install_files( ${DATA_INSTALL_DIR}/konsole FILES BlackOnLightColor.schema BlackOnLightYellow.schema BlackOnWhite.schema  	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema  	README.default.Schema syscolor.schema Linux.schema XTerm.schema vim.schema    DarkPicture.schema LightPicture.schema Transparent.schema GreenTint.schema    Transparent_MC.schema GreenTint_MC.schema    Transparent_lightbg.schema    Transparent_darkbg.schema    linux.desktop mc.desktop shell.desktop su.desktop sumc.desktop    linux.keytab vt100.keytab vt420pc.keytab x11r5.keytab solaris.keytab    README.Schema README.KeyTab README.default.Keytab screen.desktop )

kde4_install_icons( ${ICON_INSTALL_DIR} hicolor )

kde4_footer()



#original Makefile.am contents follow:

#SUBDIRS= icons
#
#konsole_other_datadir = $(kde_datadir)/konsole
#konsole_other_data_DATA = BlackOnLightColor.schema BlackOnLightYellow.schema BlackOnWhite.schema \
#	Example.Schema GreenOnBlack.schema WhiteOnBlack.schema \
#	README.default.Schema syscolor.schema Linux.schema XTerm.schema vim.schema \
#  DarkPicture.schema LightPicture.schema Transparent.schema GreenTint.schema \
#  Transparent_MC.schema GreenTint_MC.schema \
#  Transparent_lightbg.schema \
#  Transparent_darkbg.schema \
#  linux.desktop mc.desktop shell.desktop su.desktop sumc.desktop \
#  linux.keytab vt100.keytab vt420pc.keytab x11r5.keytab solaris.keytab \
#  README.Schema README.KeyTab README.default.Keytab screen.desktop
#
#profiledir = $(kde_datadir)/konsole/profiles
#profile_DATA = 2shells 3shells 4shells 5shells
#
#KDE_ICON = AUTO
#
#EXTRA_DIST = $(konsole_other_data_DATA)
