#!/bin/sh
build () {
    (
        set -x
        gcc -ggdb -Wall -pedantic -std=c99 -I./external -o ${1%.c} $@
    )
}

if [ "$1" ]; then
    build "$1"
    exit
fi

[ -x ./external/keyboard ] || build external/keyboard.c
build chatty.c
build server.c
build send.c
