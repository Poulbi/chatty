#include "common.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>


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

u16 str_len(char *str)
{
    u16 i = 0;
    while (str[i])
        i++;
    return i;
}

void str_cpy(char *to, char *from)
{
    while ((*to++ = *from++))
        ;
}

u8 save_message(struct message *msg, FILE *f)
{
    u8 err = 0;
    u16 len;
    if (msg->text == NULL) {
        len       = 0;
        msg->text = ""; // TODO: Error empty message should not be allowed.
    } else {
        len = str_len(msg->text);
    }

    if (len == 0)
        err = 1;

    fwrite(&msg->timestamp, sizeof(*msg->timestamp) * MESSAGE_TIMESTAMP_LEN, 1, f);
    fwrite(&msg->author, sizeof(*msg->author) * MESSAGE_AUTHOR_LEN, 1, f);
    fwrite(&len, sizeof(len), 1, f);
    fputs(msg->text, f);

    return err;
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
    // stream length : message author : message timestamp : message text
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
    writef("Received %d bytes from client(%d): %s [%s] %s\n", nrecv, clientfd - 3, received.timestamp, received.author, received.text);
    return nrecv;
}
