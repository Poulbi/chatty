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

// wrapper for write
void writef(char *format, ...)
{
    va_list args;
    char buf[BUF_MAX +1];
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    int n = 0;
    while (*(buf + n) != 0)
        n++;
    write(0, buf, n);
}

struct message {
    char buf[MSG_MAX];
    int buf_len;
    char timestamp[9]; // HH:MM:SS
    char author[12];
};
