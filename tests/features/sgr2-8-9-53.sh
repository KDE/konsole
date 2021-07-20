#!/bin/sh --
#
# faint intensity support (SGR 2)
# conceal support (SGR 8)
# strikeout support (SGR 9)
# overline support (SGR 53)

# echo 'D\e[2mD\e[9mD\e[53mD\e[8mD' 

echo ""; echo "[m"
echo "    faint concel strikeout overline"
#for fg in 30 31 32 33 34 35 36 37 39
for fg in 30
do
    for bg in 40 41 42 43 44 45 46 47 49
    do
        for sgr in 2 8 9 53
        do
            l1="${l1}[${fg};${bg};1m XX [m"
            l1="${l1}[${fg};${bg};${sgr}m XX [m"
        done
        l1="${l1}\n"
    done
    echo "$l1"
done
