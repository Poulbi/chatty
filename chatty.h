#ifndef CHATTY_IMPL

#include <assert.h>
#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
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

#define Kilobytes(Value) ((Value) * 1024)
#define Megabytes(Value) (Kilobytes(Value) * 1024)
#define Gigabytes(Value) (Megabytes((u64)Value) * 1024)
#define Terabytes(Value) (Gigabytes((u64)Value) * 1024)
#define PAGESIZE 4096

struct Arena {
    void* addr;
    u64 size;
    u64 pos;
} typedef Arena;

#define PushArray(arena, type, count) (type*)ArenaPush((arena), sizeof(type) * (count))
#define PushArrayZero(arena, type, count) (type*)ArenaPushZero((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

Arena*
ArenaAlloc(u64 size)
{
    Arena* arena = (Arena*)malloc(sizeof(Arena));

    arena->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (arena->addr == MAP_FAILED)
        return NULL;
    arena->pos = 0;
    arena->size = size;

    return arena;
}

void
ArenaRelease(Arena* arena)
{
    munmap(arena->addr, arena->size);
    free(arena);
}

void*
ArenaPush(Arena* arena, u64 size)
{
    u8* mem;
    mem = (u8*)arena->addr + arena->pos;
    arena->pos += size;
    return mem;
}

/// Protocol
// - every message has format Header + Message
// TODO: authentication
// TODO: encryption

/// Protocol Header
// - 2 bytes for version
// - 1 byte for message type
// - 16 bytes for checksum
//
// Text Message
// - 12 bytes for the author
// - 8 bytes for the timestamp
// - 2 bytes for the text length
// - x*4 bytes for the text
//
// History Message
// This message is for requesting messages sent after a timestamp.
// - 8 bytes for the timestamp

/// Naming convention
// Messages end with the Message suffix (eg. TextMessag, HistoryMessage)
// A function that is coupled to a type works like
// <noun><type> eg. (printTextMessage, formatTimestamp)

#define PROTOCOL_VERSION 0

typedef struct {
    u16 version;
    u8 type;
} HeaderMessage;

enum { HEADER_TYPE_TEXT = 0,
       HEADER_TYPE_HISTORY,
       HEADER_TYPE_PRESENCE };
#define HEADER_TEXTMESSAGE {.version = PROTOCOL_VERSION, .type = HEADER_TYPE_TEXT};
#define HEADER_HISTORYMESSAGE {.version = PROTOCOL_VERSION, .type = HEADER_TYPE_HISTORY};
#define HEADER_PRESENCEMESSAGE {.version = PROTOCOL_VERSION, .type = HEADER_TYPE_PRESENCE};

// Size of author string including null terminator
#define AUTHOR_LEN 13
// Size of formatted timestamp string including null terminator
#define TIMESTAMP_LEN 9

typedef struct {
    u8 checksum[16];
    u8 author[AUTHOR_LEN];
    u64 timestamp;
    u16 len;   // including null terminator
    u32* text; // placeholder for indexing
               // TODO: 0-length field?
} TextMessage;

// Size of TextMessage without text pointer, used when receiving the message over a stream
#define TEXTMESSAGE_TEXT_SIZE(m) (m.len * sizeof(*m.text))
#define TEXTMESSAGE_SIZE (sizeof(TextMessage) - sizeof(u32*))

typedef struct {
    u64 timestamp;
} HistoryMessage;

typedef struct {
    u8 author[AUTHOR_LEN];
    u8 type;
} PresenceMessage;
enum { PRESENCE_TYPE_CONNECTED = 0,
       PRESENCE_TYPE_DISCONNECTED };

// Returns string for type byte in HeaderMessage
u8*
headerTypeString(u8 type)
{
    switch (type) {
    case HEADER_TYPE_TEXT: return (u8*)"TextMessage";
    case HEADER_TYPE_HISTORY: return (u8*)"HistoryMessage";
    case HEADER_TYPE_PRESENCE: return (u8*)"PresenceMessage";
    default: return (u8*)"Unknown";
    }
}

u8*
presenceTypeString(u8 type)
{
    switch (type) {
    case PRESENCE_TYPE_CONNECTED: return (u8*)"connected";
    case PRESENCE_TYPE_DISCONNECTED: return (u8*)"disconnected";
    default: return (u8*)"Unknown";
    }
}

// from Tsoding video on minicel (https://youtu.be/HCAgvKQDJng?t=4546)
// sv(https://github.com/tsoding/sv)
#define PH_FMT "header: v%d %s(%d)"
#define PH_ARG(header) header.version, headerTypeString(header.type), header.type

void
formatTimestamp(u8 tmsp[TIMESTAMP_LEN], u64 t)
{
    struct tm* ltime;
    ltime = localtime((time_t*)&t);
    strftime((char*)tmsp, TIMESTAMP_LEN, "%H:%M:%S", ltime);
}

void
printTextMessage(TextMessage* message, u8 wide)
{
    u8 timestamp[TIMESTAMP_LEN] = {0};
    formatTimestamp(timestamp, message->timestamp);

    assert(setlocale(LC_ALL, "") != NULL);

    if (wide)
        wprintf(L"TextMessage: %s [%s] %ls\n", timestamp, message->author, (wchar_t*)&message->text);
    else {
        u8 str[message->len];
        wcstombs((char*)str, (wchar_t*)&message->text, message->len * sizeof(*message->text));
        printf("TextMessage: %s [%s] (%d)%s\n", timestamp, message->author, message->len, str);
    }
}

// Receive a message from fd and store it to the msgsArena,
// if dest is not NULL point it to the new message created on msgsArena
// Returns the number of bytes received
u32
recvTextMessage(Arena* msgsArena, u32 fd, TextMessage** dest)
{
    s32 nrecv = 0;

    TextMessage* message = ArenaPush(msgsArena, TEXTMESSAGE_SIZE);
    if (dest != NULL)
        *dest = message;

    // Receive everything but the text so we can know the text's size and act accordingly
    nrecv = recv(fd, message, TEXTMESSAGE_SIZE, 0);
    assert(nrecv != -1);
    assert(nrecv == TEXTMESSAGE_SIZE);

    nrecv = 0;

    // Allocate memory for text and receive in that memory
    u32 text_size = message->len * sizeof(*message->text);
    ArenaPush(msgsArena, text_size);

    nrecv = recv(fd, (u8*)&message->text, text_size, 0);
    assert(nrecv != -1);
    assert(nrecv == message->len * sizeof(*message->text));

    return TEXTMESSAGE_SIZE + nrecv;
}

u32
wstrlen(u32* str)
{
    u32 i = 0;
    while (str[i] != 0)
        i++;
    return i;
}

#endif
#define CHATTY_H
