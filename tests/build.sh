#!/bin/sh

ScriptDir="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDir"
WarningFlags="-Wno-unused-variable"

printf 'tests.c\n'
gcc -ggdb -Wall $WarningFlags -o tests tests.c
./tests

# printf 'archived/input_box.c\n'
# gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -I . -o "$BuildDir"/input_box archived/input_box.c
