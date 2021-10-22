do_tests() {
    tput cup 0 0; printf "$@"

    tput cup  0 $((COLUMNS-1)); printf "AB"
    tput cup  3 $((COLUMNS-1)); printf "A\bB"
    tput cup  6 $((COLUMNS-1)); printf A; tput  cud 1; printf B
    tput cup  9 $((COLUMNS-1)); printf A; tput  cuu 1; printf B
    tput cup 12 $((COLUMNS-1)); printf A; tput  cub 1; printf B
    tput cup 15 $((COLUMNS-1)); printf A; tput  cuf 1; printf B

    tput cup 18 1; echo -n -- "-- Press RETURN to run next test --"
    read
    #sleep 5
}

clear; tput smam
do_tests "No Reverse Wrap - AutoWrap Mode"

clear; tput rmam
do_tests "No Reverse Wrap - No AutoWrap Mode"


clear; tput smam; printf "\e[?45h"
do_tests "Reverse Wrap - AutoWrap Mode"

clear; tput rmam; printf "\e[?45h"
do_tests "Reverse Wrap - No AutoWrap Mode"


clear; tput smam; printf "\e[?45l"
