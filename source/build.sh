#!/bin/sh

ScriptDir="$(dirname "$(readlink -f "$0")")"
cd "$ScriptDir"
BuildDir="$ScriptDir"/../build

CompilerFlags="
-DIMPORT_ID=1
"

WarningFlags="
-Wall
-Wextra
-Wno-unused-variable
-Wno-unused-parameter
-Wno-unused-but-set-variable
-Wno-maybe-uninitialized
-Wno-sign-compare
"

Mode="$1"
if [ "$Mode" != "release" ]
then
 Mode="debug"
fi
printf '[Mode %s]\n' "$Mode"

if [ "$Mode" = "debug" ]
then
 CompilerFlags="$CompilerFlags
 -DDEBUG=1
 -ggdb -g3
 "
elif [ "$Mode" = "release" ]
then
 CompilerFlags="$CompilerFlags
 -O3
 "
fi

mkdir -p "$BuildDir"

printf 'chatty.c\n'
gcc $CompilerFlags $WarningFlags -I external -o "$BuildDir"/chatty chatty.c

printf 'server.c\n'
gcc $CompilerFlags $WarningFlags -o "$BuildDir"/server server.c

# printf 'archived/input_box.c\n'
# gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -I . -o "$BuildDir"/input_box archived/input_box.c
