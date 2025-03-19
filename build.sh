#!/bin/sh
set -x
# gcc external/keyboard.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -o build/chatty chatty.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o build/server server.c
# gcc -DDEBUG -ggdb -Wall -pedantic -std=c99 -o build/send send.c
gcc -DDEBUG -ggdb -Wall -pedantic -std=c11 -I external -I . -o build/input_box archived/input_box.c
