#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9983
// max buffer size sent over network
// TODO: choose a better size
#define BUF_MAX 256
// max size for a message sent
#define MESSAGE_MAX 256
// max length of author field
#define MESSAGE_AUTHOR_LEN 13
// max length of timestamp field
#define MESSAGE_TIMESTAMP_LEN 9
// current user's name
#define USERNAME "Jef Koek"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

// To serialize the text that could be arbitrary length the lenght is encoded after the author
// string and before the text.
struct Message {
    u8 author[MESSAGE_AUTHOR_LEN];
    u8 timestamp[MESSAGE_TIMESTAMP_LEN]; // HH:MM:SS
    u16 text_len;                             // length of the text including null terminator
    char *text;
} typedef Message;

// printf without buffering using write syscall, works when using sockets
void writef(char *format, ...);

u16 str_len(char *str);

// save the message msg to file in binary format, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 message_fsave(Message *msg, FILE *f);
// load the message msg from file f, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 message_fload(Message *msg, FILE *f);

// Encode msg and send it to fd
// return -1 if send() returns -1. Otherwise returns number of bytes sent.
// NOTE: this function should not alter the content stored in msg.
u32 message_send(Message *msg, u32 fd);
// Decode data from fd and populate msg with it
// if recv() returns 0 or -1 it will return early and return 0 or -1 accordingly.
// Otherwise returns the number of bytes received
u32 message_receive(Message *msg, u32 fd);

void writef(char *format, ...)
{
    char buf[255 + 1];
    va_list args;
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    int n = 0;
    while (*(buf + n) != 0)
        n++;
    write(0, buf, n);
}

// Returns the length of the string plus the null terminator
u16 str_len(char *str)
{
    if (*str == 0)
        return 0;

    u16 i = 0;
    while (str[i])
        i++;

    return i + 1;
}

void str_cpy(char *to, char *from)
{
    while ((*to++ = *from++))
        ;
}

// Save msg to file f
// Returns 0 on success, returns 1 if msg->text is NULL, returns 2 if mfg->len is 0
u8 message_fsave(Message *msg, FILE *f)
{
    if (msg->text == NULL) {
        return 1;
    } else if (msg->text_len == 0)
        return 2;

    fwrite(&msg->timestamp, sizeof(*msg->timestamp) * MESSAGE_TIMESTAMP_LEN, 1, f);
    fwrite(&msg->author, sizeof(*msg->author) * MESSAGE_AUTHOR_LEN, 1, f);
    fwrite(&msg->text_len, sizeof(msg->text_len), 1, f);
    fwrite(&msg->text, msg->text_len, 1, f);

    return 0;
}

u8 message_fload(Message *msg, FILE *f)
{
    fread(msg, sizeof(*msg->timestamp) * MESSAGE_TIMESTAMP_LEN + sizeof(*msg->author) * MESSAGE_AUTHOR_LEN, 1, f);
    u16 len;
    fread(&len, sizeof(len), 1, f);
    if (len == 0) {
        // TODO: Error: empty message should not be allowed
        // empty message
        msg->text = NULL;
        return 1;
    }
    char txt[len];
    fgets(txt, len, f);
    memcpy(msg->text, txt, len);

    return 0;
}

u32 message_send(Message *msg, u32 serverfd)
{
    // for protocol see README.md
    u32 buf_len = sizeof(buf_len) + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN + msg->text_len;
    char buf[buf_len];
    u32 offset;

    memcpy(buf, &buf_len, sizeof(buf_len));
    offset = sizeof(buf_len);
    memcpy(buf + offset, msg->author, MESSAGE_AUTHOR_LEN);
    offset += MESSAGE_AUTHOR_LEN;
    memcpy(buf + offset, msg->timestamp, MESSAGE_TIMESTAMP_LEN);
    offset += MESSAGE_TIMESTAMP_LEN;
    memcpy(buf + offset, msg->text, msg->text_len);

    u32 n = send(serverfd, &buf, buf_len, 0);
    if (n == -1)
        return n;

    return n;
}

u32 message_receive(Message *msg, u32 clientfd)
{
    // for protocol see README.md
    // must all be of the s
    u32 nrecv = 0, buf_len = 0;
    // limit on what can be received with recv()
    u32 buf_size = 20;
    // temporary buffer to receive message data over a stream
    char recv_buf[BUF_MAX] = {0};

    nrecv = recv(clientfd, recv_buf, buf_size, 0);
    if (nrecv == 0 || nrecv == -1)
        return nrecv;

    memcpy(&buf_len, recv_buf, sizeof(buf_len));

    u32 i = 0;
    while (nrecv < buf_len) {
        // advance the copying by the amounts of bytes received each time
        i = recv(clientfd, recv_buf + nrecv, buf_size, 0);
        if (i == 0 || i == -1)
            return nrecv;
        nrecv += i;
    }

    memcpy(msg, recv_buf + sizeof(buf_len), MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN);
    msg->text = recv_buf + sizeof(buf_len) + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN;
    msg->text_len = buf_len - sizeof(buf_len) - MESSAGE_AUTHOR_LEN - MESSAGE_TIMESTAMP_LEN;

    return nrecv;
}
