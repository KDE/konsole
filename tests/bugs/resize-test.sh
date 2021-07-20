#!/bin/sh

echo "Make sure konsole is built with 'cmake .. -DECM_ENABLE_SANITIZERS=address'"
echo

echo $(printf 'a%.0s' $(seq 1 $(($(stty size | cut -d\  -f2) - 1))))

echo
echo Now make the window smaller

