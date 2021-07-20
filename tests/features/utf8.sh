#!/bin/bash --
#
# Switch utf-8 mode
#
#-------------------------------------------------------------------------

case $1 in
  on) echo $'\033%G'"UTF-8 on";;
  off) echo $'\033%@'"UTF-8 off";;
  *) echo "usage: $0 [on|off]";;
esac
