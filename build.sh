#!/bin/sh
set -x
gcc external/keyboard.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o chatty chatty.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o server server.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o send send.c
