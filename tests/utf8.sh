#!/bin/sh --
#
# Switch utf-8 mode
#
#-------------------------------------------------------------------------

case $1 in
  on) echo "%G""UTF-8 on";;
  off) echo "%@""UTF-8 off";;
  *) echo "usage: $0 [on|off]";;
esac
