#!/bin/sh
set -x
gcc -ggdb -Wall -pedantic -std=c99 -o client client.c
gcc -ggdb -Wall -pedantic -std=c99 -o server server.c
gcc -ggdb -Wall -pedantic -std=c99 -o send send.c
gcc -ggdb -Wall -pedantic -std=c99 -o recv recv.c
