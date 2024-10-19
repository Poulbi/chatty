#!/bin/sh
set -x
gcc -g -Wall -pedantic -std=c99 -o chatty client.c common.c
gcc -g -Wall -pedantic -std=c99 -o server server.c common.c
