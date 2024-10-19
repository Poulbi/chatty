#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#define PORT 9983
// max size for a message sent
#define BUF_MAX 255
// max length of messages
#define MSG_MAX 256
// current user's name
#define USERNAME "unrtdqttr"

void writef(char* format, ...);

struct message {
    char buf[MSG_MAX];
    int buf_len;
    char timestamp[9]; // HH:MM:SS
    char author[12];
};
