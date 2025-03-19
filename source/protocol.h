#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "arena.h"
#include "chatty.h"

/// Protocol
// - every message has format Header + Message
// TODO: security
//
/// ID
// - So clients can be identified uniquely.
// - 8 bytes
// - number that increments for each new client
//
/// Strings
// - strings are sent with their null terminator
//
/// Authentication
//      Each header contains the id of the sender, because ids start at 1
//      message with id 0 is considered unauthenticated.
//
//      When the server receives a header with id 0, this can happen
//      Scenario 1: IDMessage, client already has an ID
//      1. client-> Send own ID
//      2. server-> knows ID?
//           y. server-> Success
//           n. 1. server-> Error 'notfound'
//              2. client-> exit
//      Scenario 2: IntroductionMessage, client requests a new ID
//      1. client-> Introduces
//      2. server-> Sends & Saves ID
//      3. Save ID
//
//      BIFD & UNIFD
//      Each client has 2 connections that must be authenticated one for 2-way
//      communication and one for 1-way communication.  Respectively BIFD and
//      UNIFD.  BIFD must be authenticated first and is meant for requests such
//      as getting an IntroductionMessage for a sent IDMessage.
//      UNIFD is for messages that are like notifications.  For example
//      PresenceMessage that tells us when another user connected.
//      These two connections separate these message types so we do not have to
//      worry about receiving a PresenceMessage when waiting for an a response.
//
/// Naming conventions
// Messages end with the Message suffix (eg. TextMessag, HistoryMessage)
//
// A function that is coupled to a type works like
// <noun><type> eg. (printTextMessage, formatTimestamp)

#define PROTOCOL_VERSION 0
// Size of author string including null terminator
#define AUTHOR_LEN 13
// Size of formatted timestamp string including null terminator
#define TIMESTAMP_LEN 9
#define TIMESTAMP_FORMAT "%H:%M:%S"

typedef u64 ID;

// - 2 bytes for version
// - 1 byte for message type
// - 16 bytes for checksum
typedef struct {
    u16 version;
    u8 type;
    ID id;
} HeaderMessage;

typedef enum {
    HEADER_TYPE_TEXT = 0,
    HEADER_TYPE_HISTORY,
    HEADER_TYPE_PRESENCE,
    HEADER_TYPE_ID,
    HEADER_TYPE_INTRODUCTION,
    HEADER_TYPE_ERROR
} HeaderType;
// shorthand for creating a header with a value from the enum
#define HEADER_INIT(t) {.version = PROTOCOL_VERSION, .type = t, .id = 0}
// from Tsoding video on minicel (https://youtu.be/HCAgvKQDJng?t=4546)
// sv(https://github.com/tsoding/sv)
#define HEADER_FMT "header: v%d %s(%d) [%d]"
#define HEADER_ARG(header) header.version, headerTypeString(header.type), header.type, header.id

// For sending texts to other clients
// - 13 bytes for the author
// - 8 bytes for the timestamp
// - 2 bytes for the text length
// - x*4 bytes for the text
typedef struct {
    u64 timestamp; // timestamp of when the message was sent
    u16 len;
    wchar_t* text; // placeholder for indexing
                   // wchar_t* is used, because this renders the text in the debugger
} TextMessage;
// Size of TextMessage without text pointer
#define TEXTMESSAGE_SIZE (sizeof(TextMessage) - sizeof(u32*))

// Requesting messages sent after a timestamp.
// - 8 bytes for the timestamp
typedef struct {
    u64 timestamp;
} HistoryMessage;

// Introduce the client to the server by sending the client's information.
// See "First connection".
// - 13 bytes for author
typedef struct {
    u8 author[AUTHOR_LEN];
} IntroductionMessage;
#define INTRODUCTION_FMT "introduction: %s"
#define INTRODUCTION_ARG(message) message.author

// Notifying the sender's state, such as "connected", "disconnected", "AFK", ...
// - 1 byte for type
typedef struct {
    u8 type;
} PresenceMessage;
typedef enum {
    PRESENCE_TYPE_CONNECTED = 0,
    PRESENCE_TYPE_DISCONNECTED,
    PRESENCE_TYPE_AFK
} PresenceType;

// Send an error message
// - 1 byte for type
typedef struct {
    u8 type;
} ErrorMessage;
typedef enum {
    ERROR_TYPE_BADMESSAGE = 0,
    ERROR_TYPE_NOTFOUND,
    ERROR_TYPE_SUCCESS,
    ERROR_TYPE_ALREADYCONNECTED,
    ERROR_TYPE_TOOMANYCONNECTIONS
} ErrorType;
#define ERROR_INIT(t) {.type = t}

typedef struct {
    ID id;
} IDMessage;

typedef struct {
    s32 nrecv;
    TextMessage* message;
} recvTextMessageResult;

// Returns string for type byte in HeaderMessage
u8*
headerTypeString(HeaderType type)
{
    switch (type)
    {
    case HEADER_TYPE_TEXT: return (u8*)"TextMessage";
    case HEADER_TYPE_HISTORY: return (u8*)"HistoryMessage";
    case HEADER_TYPE_PRESENCE: return (u8*)"PresenceMessage";
    case HEADER_TYPE_ID: return (u8*)"IDMessage";
    case HEADER_TYPE_INTRODUCTION: return (u8*)"IntroductionMessage";
    case HEADER_TYPE_ERROR: return (u8*)"ErrorMessage";
    default: return (u8*)"Unknown";
    }
}

u8*
presenceTypeString(PresenceType type)
{
    switch (type)
    {
    case PRESENCE_TYPE_CONNECTED: return (u8*)"connected";
    case PRESENCE_TYPE_DISCONNECTED: return (u8*)"disconnected";
    case PRESENCE_TYPE_AFK: return (u8*)"afk";
    default: return (u8*)"Unknown";
    }
}

u8*
errorTypeString(ErrorType type)
{
    switch (type)
    {
    case ERROR_TYPE_BADMESSAGE: return (u8*)"bad message";
    case ERROR_TYPE_NOTFOUND: return (u8*)"not found";
    case ERROR_TYPE_SUCCESS: return (u8*)"success";
    case ERROR_TYPE_ALREADYCONNECTED: return (u8*)"already connected";
    case ERROR_TYPE_TOOMANYCONNECTIONS: return (u8*)"too many connections";
    default: return (u8*)"Unknown";
    }
}

// Formats time t into tmsp string
void
formatTimestamp(u8 timestamp_str[TIMESTAMP_LEN], u64 timestamp)
{
    struct tm* ltime;
    ltime = localtime((time_t*)&timestamp);
    strftime((char*)timestamp_str, TIMESTAMP_LEN, TIMESTAMP_FORMAT, ltime);
}

// Receive a message from fd and store it in the msgsArena,
// Returns pointer to the allocated memory
TextMessage*
recvTextMessage(Arena* msgsArena, u32 fd)
{
    TextMessage* message = ArenaPush(msgsArena, TEXTMESSAGE_SIZE);

    // Receive everything but the text so we can know the text's size and act accordingly
    s32 nrecv = recv(fd, message, TEXTMESSAGE_SIZE, 0);
    assert(nrecv != -1);
    assert(nrecv == TEXTMESSAGE_SIZE);

    // Allocate memory for text and receive in that memory
    u32 text_size = message->len * sizeof(*message->text);
    ArenaPush(msgsArena, text_size);

    nrecv = recv(fd, (u8*)&message->text, text_size, 0);
    assert(nrecv != -1);
    assert(nrecv == (s32)(message->len * sizeof(*message->text)));

    return message;
}

typedef struct {
    HeaderMessage* header;
    void* message;
} Message;

u32
getMessageSize(HeaderType type)
{
    u32 size = 0;
    switch (type)
    {
    case HEADER_TYPE_ERROR: size = sizeof(ErrorMessage); break;
    case HEADER_TYPE_HISTORY: size = sizeof(HistoryMessage); break;
    case HEADER_TYPE_INTRODUCTION: size = sizeof(IntroductionMessage); break;
    case HEADER_TYPE_PRESENCE: size = sizeof(PresenceMessage); break;
    case HEADER_TYPE_ID: size = sizeof(IDMessage); break;
    default: assert(0);
    }
    return size;
}

s32
recvAnyMessageType(s32 fd, HeaderMessage* header, void *anyMessage, HeaderType type)
{
    s32 nrecv = recv(fd, header, sizeof(*header), 0);
    if (nrecv == -1 || nrecv == 0)
        return nrecv;
    assert(nrecv == sizeof(*header));

    s32 size = 0;
    switch (type)
    {
    case HEADER_TYPE_ERROR:
    case HEADER_TYPE_HISTORY:
    case HEADER_TYPE_INTRODUCTION:
    case HEADER_TYPE_PRESENCE:
    case HEADER_TYPE_ID:
        size = getMessageSize(header->type);
        break;
    case HEADER_TYPE_TEXT:
    {
        TextMessage* message = anyMessage;
        size = TEXTMESSAGE_SIZE + message->len * sizeof(*message->text);
    } break;
    default: assert(0); break;
    }
    assert(header->type == type);

    nrecv = recv(fd, anyMessage, size, 0);
    assert(nrecv != -1);
    assert(nrecv == size);

    return size;
}

// Get any message into arena
Message
recvAnyMessage(Arena* arena, s32 fd)
{
    HeaderMessage* header = ArenaPush(arena, sizeof(*header));
    s32 nrecv = recv(fd, header, sizeof(*header), 0);
    assert(nrecv != -1);
    assert(nrecv == sizeof(*header));

    s32 size = 0;
    switch (header->type)
    {
    case HEADER_TYPE_TEXT:
    {
        Message result;
        result.header = header;
        result.message = recvTextMessage(arena, fd);
        return result;
    } break;
    default:
    {
        size = getMessageSize(header->type);
    } break;
    }

    void* message = ArenaPush(arena, size);
    nrecv = recv(fd, message, size, 0);
    assert(nrecv != -1);
    assert(nrecv == size);

    Message result;
    result.header = header;
    result.message = message;

    return result;
}

Message
waitForMessageType(Arena* arena, Arena* queueArena, u32 fd, HeaderType type)
{
    Message message;
    while (1)
    {
        message = recvAnyMessage(arena, fd);
        if (message.header->type == type)
            break;
        ArenaPush(queueArena, getMessageSize(message.header->type));
    }
    return message;
}

// Generic sending function for sending any type of message to fd
// Returns number of bytes sent in message or -1 if there was an error.
s32
sendAnyMessage(u32 fd, HeaderMessage header, void* anyMessage)
{
    s32 nsend_total;
    s32 nsend = send(fd, &header, sizeof(header), 0);
    if (nsend == -1) return nsend;
    LoggingF("sendAnyMessage (%d)|sending "HEADER_FMT"\n", fd, HEADER_ARG(header));
    assert(nsend == sizeof(header));
    nsend_total = nsend;

    s32 size = 0;
    switch (header.type)
    {
    case HEADER_TYPE_ERROR:
    case HEADER_TYPE_HISTORY:
    case HEADER_TYPE_INTRODUCTION:
    case HEADER_TYPE_PRESENCE:
    case HEADER_TYPE_ID:
        size = getMessageSize(header.type);
        break;
    case HEADER_TYPE_TEXT:
    {
        nsend = send(fd, anyMessage, TEXTMESSAGE_SIZE, 0);
        assert(nsend != -1);
        assert(nsend == TEXTMESSAGE_SIZE);
        nsend_total += nsend;
        // set size to remaning text size that should be sent
        TextMessage* message = (TextMessage*)anyMessage;
        size = message->len * sizeof(*message->text);
        nsend = 0;

        anyMessage = &message->text;
    } break;
    default:
        LoggingF("sendAnyMessage (%d)|Cannot send %s\n", fd, headerTypeString(header.type));
        return -1;
    }

    nsend = send(fd, anyMessage, size, 0);
    if (nsend == -1) return nsend;
    assert(nsend == size);
    nsend_total += nsend;

    return nsend_total;
}

#endif
