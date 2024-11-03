#ifndef CHATTY_H

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

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef enum {
    False = 0,
    True = 1
} Bool;

// port for chatty
#define PORT 9983
// max number of bytes that can be logged at once
#define LOGMESSAGE_MAX 2048
#define LOG_FMT "%H:%M:%S "
#define LOG_LEN 10

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes((u64)Value) * 1024)
#define Terabytes(Value) (Gigabytes((u64)Value) * 1024)
#define PAGESIZE 4096
#define local_persist static
#define global_variable
#define internal static

// Enable/Disable saving clients permanently to file
// #define IMPORT_ID

global_variable s32 logfd;

u32
wstrlen(u32* str)
{
    u32 i = 0;
    while (str[i] != 0)
        i++;
    return i;
}

void
loggingf(char* format, ...)
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
    write(logfd, timestamp, LOG_LEN - 1);

    write(logfd, buf, n);
}

// Arena Allocator
struct Arena {
    void* addr;
    u64 size;
    u64 pos;
} typedef Arena;

#define PushArray(arena, type, count) (type*)ArenaPush((arena), sizeof(type) * (count))
#define PushArrayZero(arena, type, count) (type*)ArenaPushZero((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

// Returns arena in case of success, or 0 if it failed to alllocate the memory
void
ArenaAlloc(Arena* arena, u64 size)
{
    arena->addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    assert(arena->addr != MAP_FAILED);
    arena->pos = 0;
    arena->size = size;
}

void
ArenaRelease(Arena* arena)
{
    munmap(arena->addr, arena->size);
}

void*
ArenaPush(Arena* arena, u64 size)
{
    u8* mem;
    mem = (u8*)arena->addr + arena->pos;
    arena->pos += size;
    assert(arena->pos <= arena->size);
    return mem;
}

#endif
#define CHATTY_H
