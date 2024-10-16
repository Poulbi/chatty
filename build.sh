#!/bin/sh
gcc -g -Wall -pedantic -std=c99 -o server server.c
gcc -g -Wall -pedantic -std=c99 -o chatty client.c
