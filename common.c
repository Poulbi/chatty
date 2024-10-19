#include "common.h"
#include "config.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>

void writef(char *format, ...)
{
    va_list args;
    char buf[BUF_MAX + 1];
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

    fwrite(&msg->timestamp, sizeof(*msg->timestamp) * MSG_TIMESTAMP_LEN, 1, f);
    fwrite(&msg->author, sizeof(*msg->author) * MSG_AUTHOR_LEN, 1, f);
    fwrite(&len, sizeof(len), 1, f);
    fputs(msg->text, f);

    return err;
}

u8 load_message(struct message *msg, FILE *f)
{
    fread(msg, sizeof(*msg->timestamp) * MSG_TIMESTAMP_LEN + sizeof(*msg->author) * MSG_AUTHOR_LEN, 1, f);
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
