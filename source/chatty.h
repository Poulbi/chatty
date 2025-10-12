#ifndef CHATTY_H
#define CHATTY_H

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

// port for chatty
#define PORT 9983
// max number of bytes that can be logged at once
#define LOGMESSAGE_MAX 2048
#define LOG_FMT "%H:%M:%S "
#define LOG_LEN 10
// Enable/Disable saving clients permanently to file
// #define IMPORT_ID

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes((u64)Value) * 1024)
#define Terabytes(Value) (Gigabytes((u64)Value) * 1024)
#define PAGESIZE 4096
#define local_persist static
#define global_variable
#define internal static

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 b32;

void Loggingf(char* format, ...);

#endif // CHATTY_H

#ifdef CHATTY_IMPL

global_variable s32 LogFD;

void
LoggingF(char* format, ...)
{
    char buf[LOGMESSAGE_MAX];
    va_list args;
    va_start(args, format);
    
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    int n = 0;
    while (*(buf + n) != 0) n++;
    
    u64 t = time(0);
    u8 timestamp[LOG_LEN];
    struct tm* ltime = localtime((time_t*)&t);
    strftime((char*)timestamp, LOG_LEN, LOG_FMT, ltime);
    write(LogFD, timestamp, LOG_LEN - 1);
    
    write(LogFD, buf, n);
}

#undef CHATTY_IMPL
#endif // CHATTY_IMPL
