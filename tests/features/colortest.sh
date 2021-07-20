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
    l5="    ";
    l6="    ";
    l7="    ";
    l8="    ";
    l9="    ";
    l10="    ";
    l11="    ";
    for bg in 40 41 42 43 44 45 46 47 49
    do
	l1="${l1}${CSI}${fg};${bg}m Normal ${RST}"
	l2="${l2}${CSI}${fg};${bg};1m Bold   ${RST}"
	l3="${l3}${CSI}${fg};${bg};2m Faint  ${RST}"
	l4="${l4}${CSI}${fg};${bg};3m Italic ${RST}"
	l5="${l5}${CSI}${fg};${bg};4m Underl ${RST}"
	l6="${l6}${CSI}${fg};${bg};5m Blink  ${RST}"
	l7="${l7}${CSI}${fg};${bg};8m Concel ${RST}"
	l8="${l8}${CSI}${fg};${bg};9m Strike ${RST}"
	l9="${l9}${CSI}${fg};${bg};53m Overli ${RST}"
	l10="${l10}${CSI}${fg};${bg};1;5m Bold!  ${RST}"
	l11="${l11}${CSI}${fg};${bg};3;4m It/Und ${RST}"

    done
    echo "$l1"
    echo "$l2"
    echo "$l3"
    echo "$l4"
    echo "$l5"
    echo "$l6"
    echo "$l7"
    echo "$l8"
    echo "$l9"
    echo "$l10"
    echo "$l11"
done
