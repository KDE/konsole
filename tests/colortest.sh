#!/bin/sh --
#
# display ANSI colours and test bold/blink attributes
# orginates from Eterm distribution
#-------------------------------------------------------------------------

echo ""; echo "[m"
echo "       40      41      42      43      44      45      46      47      49"
echo "       40      41      42      43      44      45      46      47      49"
for fg in 30 31 32 33 34 35 36 37 39
do
    l1="$fg  ";
    l2="    ";
    l3="    ";
    l4="    ";
    for bg in 40 41 42 43 44 45 46 47 49
    do
	l1="${l1}[${fg};${bg}m Normal [m"
	l2="${l2}[${fg};${bg};1m Bold   [m"
	l3="${l3}[${fg};${bg};5m Blink  [m"
	l4="${l4}[${fg};${bg};1;5m Bold!  [m"
    done
    echo "$l1"
    echo "$l2"
    echo "$l3"
    echo "$l4"
done
