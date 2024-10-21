#!/bin/sh
set -x
gcc -g -Wall -pedantic -std=c99 -o chatty client.c
gcc -g -Wall -pedantic -std=c99 -o server server.c
gcc -g -Wall -pedantic -std=c99 -o send send.c
gcc -g -Wall -pedantic -std=c99 -o recv recv.c
