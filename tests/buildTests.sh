#!/bin/sh

CLEAN_CMD="clean"

if [ "$1" == "$CLEAN_CMD" ];then
    for file in src/*.c
    do
        fname=$(echo "$file" | sed 's/\.c//g' | sed 's/src\///g')
        echo "rm -rf $fname"
        rm -rf "$fname"
    done
else
    for file in src/*.c
    do
        output=$(echo "$file" | sed 's/\.c//g' | sed 's/src\///g')
        echo "gcc $file ../Library/client_api.c -o $output"
        gcc $file ../Library/client_api.c -o $output
    done
fi