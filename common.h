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
#define MESSAGE_AUTHOR_LEN 12
// max length of timestamp field
#define MESSAGE_TIMESTAMP_LEN 9
// current user's name
#define USERNAME "Jef Koek"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// To serialize the text that could be arbitrary length the lenght is encoded after the author
// string and before the text.
struct message {
    char author[MESSAGE_AUTHOR_LEN];
    char timestamp[MESSAGE_TIMESTAMP_LEN]; // HH:MM:SS
    u16 len;                               // length of the text including null terminator
    char *text;
};

// printf without buffering using write syscall, works when using sockets
void writef(char *format, ...);

u16 str_len(char *str);
void str_cpy(char *to, char *from);

// save the message msg to file in binary format, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 save_message(struct message *msg, FILE *f);
// load the message msg from file f, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 load_message(struct message *msg, FILE *f);

// Send a stream of bytes containing msg
// return -1 if send() returns -1. Otherwise returns number of bytes sent.
u32 send_message(struct message msg, u32 serverfd);
// Receives a stream of bytes and populates msg with the data received
// if recv() returns 0 or -1 it will return early and return 0 or -1 accordingly.
// Otherwise returns the number of bytes received
u32 receive_message(struct message *msg, u32 clientfd);

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
u8 save_message(struct message *msg, FILE *f)
{
    if (msg->text == NULL) {
        return 1;
    } else if (msg->len == 0)
        return 2;

    fwrite(&msg->timestamp, sizeof(*msg->timestamp) * MESSAGE_TIMESTAMP_LEN, 1, f);
    fwrite(&msg->author, sizeof(*msg->author) * MESSAGE_AUTHOR_LEN, 1, f);
    fwrite(&msg->len, sizeof(msg->len), 1, f);
    fputs(msg->text, f);

    return 0;
}

u8 load_message(struct message *msg, FILE *f)
{
    fread(msg, sizeof(*msg->timestamp) * MESSAGE_TIMESTAMP_LEN + sizeof(*msg->author) * MESSAGE_AUTHOR_LEN, 1, f);
    u16 len;
    fread(&len, sizeof(len), 1, f);
    if (len == 0) {
        // TODO: Error: empty message should not be allowed
        // empty message
        msg->text = "";
        return 1;
    }
    char txt[len];
    fgets(txt, len, f);
    msg->text = txt;

    return 0;
}

u32 send_message(struct message msg, u32 serverfd)
{
    // stream length : message author : message timestamp : message text + \0
    u32 buf_len = sizeof(buf_len) + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN + msg.len;
    char buf[buf_len];
    u32 offset;

    memcpy(buf, &buf_len, sizeof(buf_len));
    offset = sizeof(buf_len);
    memcpy(buf + offset, msg.author, MESSAGE_AUTHOR_LEN);
    offset += MESSAGE_AUTHOR_LEN;
    memcpy(buf + offset, msg.timestamp, MESSAGE_TIMESTAMP_LEN);
    offset += MESSAGE_TIMESTAMP_LEN;
    memcpy(buf + offset, msg.text, msg.len);

    u32 n = send(serverfd, &buf, buf_len, 0);
    if (n == -1)
        return n;

    writef("%d bytes sent.\n", n);
    return n;
}

u32 receive_message(struct message *msg, u32 clientfd)
{
    // must all be of the s
    u32 nrecv, buf_len;
    // limit on what can be received with recv()
    u32 buf_size = 20;
    // temporary buffer to receive message data over a stream
    char recv_buf[BUF_MAX];

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

    struct message received = {0};
    memcpy(&received, recv_buf + sizeof(buf_len), MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN);
    received.text = recv_buf + sizeof(buf_len) + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN;
    received.len  = buf_len - sizeof(buf_len) - MESSAGE_AUTHOR_LEN - MESSAGE_TIMESTAMP_LEN;

    // assume clientfd is serverfd + 1;
    writef("Received %d bytes from client(%d): %s [%s] (%d)%s\n", nrecv, clientfd - 3, received.timestamp, received.author, received.len, received.text);
    return nrecv;
}
