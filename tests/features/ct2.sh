#!/bin/sh --
#
# display ANSI colours and test bold/blink attributes
# orginates from Eterm distribution
#-------------------------------------------------------------------------

echo ""; echo "[m"
echo "     40  41  42  43  44  45  46  47  49"
for fg in 30 31 32 33 34 35 36 37 39
do
    l1=" $fg ";
    l2="[1m $fg [m";
    for bg in 40 41 42 43 44 45 46 47 49
    do
	l1="${l1}[${fg};${bg}m xx [m"
	l2="${l2}[${fg};${bg};1m XX [m"
    done
    echo "$l1"
    echo "$l2"
done
