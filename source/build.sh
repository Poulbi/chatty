#!/bin/sh

ScriptDir="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDir"
BuildDir="$ScriptDir"/../build

mkdir -p "$BuildDir"
printf 'chatty.c\n'
gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -o "$BuildDir"/chatty chatty.c
printf 'server.c\n'
gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o "$BuildDir"/server server.c

# printf 'archived/input_box.c\n'
# gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -I . -o "$BuildDir"/input_box archived/input_box.c
