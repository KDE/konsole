#!/bin/bash --
#
# display ANSI colours and test bold/blink attributes
# orginates from Eterm distribution
#-------------------------------------------------------------------------

ESC=$'\x1b'
CSI="${ESC}["
RST="${CSI}m"

echo ""; echo "${RST}"
echo "       40      41      42      43      44      45      46      47      49"
echo "       40      41      42      43      44      45      46      47      49"
for fg in 30 31 32 33 34 35 36 37 39 90 91 92 93 94 95 96 97
do
    l1="$fg  ";
    l2="    ";
    l3="    ";
    l4="    ";
    for bg in 40 41 42 43 44 45 46 47 49
    do
	l1="${l1}${CSI}${fg};${bg}m Normal ${RST}"
	l2="${l2}${CSI}${fg};${bg};1m Bold   ${RST}"
	l3="${l3}${CSI}${fg};${bg};5m Blink  ${RST}"
	l4="${l4}${CSI}${fg};${bg};1;5m Bold!  ${RST}"
    done
    echo "$l1"
    echo "$l2"
    echo "$l3"
    echo "$l4"
done
