#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <wchar.h>

#define AUTHOR_LEN 13
#define TIMESTAMP_LEN 9
// port to listen on
#define PORT 9983
// buffer size for holding data received from recv()
// TODO: choose a good size
#define STREAM_BUF 256
// max data received in one recv() call on serverfd
// TODO: choose a good size
#define STREAM_LIMIT 512
// max message that can be displayed with writef()
#define WRITEF_MAX 256


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

struct Message {
    u8 author[AUTHOR_LEN];
    u8 timestamp[TIMESTAMP_LEN];
    // includes null terminator
    u16 text_len;
    wchar_t *text;
} typedef Message;

#define MESSAGELEN(m) (AUTHOR_LEN + TIMESTAMP_LEN + sizeof(m.text_len)*sizeof(wchar_t) + m.text_len)
#define MESSAGELENP(m) (AUTHOR_LEN + TIMESTAMP_LEN + sizeof(m->text_len) + m->text_len*(sizeof(wchar_t)))

void message_timestamp(u8 str[TIMESTAMP_LEN])
{
    time_t now;
    struct tm *ltime;
    time(&now);
    ltime = localtime(&now);
    strftime((char *)str, TIMESTAMP_LEN, "%H:%M:%S", ltime);
}

#endif
