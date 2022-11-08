#!/bin/sh

sed --regexp-extended --null-data --in-place '
s/(<ToolBar.*)(<\/ToolBar>)/\1<Action name="hamburger_menu"\/>\n\2/
' \
`qtpaths --locate-file GenericDataLocation kxmlgui5/konsole/sessionui.rc`
