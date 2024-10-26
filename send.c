// minimal client implementation

#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: send <author> <msg>\n");
        return 1;
    }

    u32 err, serverfd, nsend;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverfd != -1);

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };
    err = connect(serverfd, (struct sockaddr *)&address, sizeof(address));
    assert(err == 0);

    {
        u32 author_len = strlen(argv[1]) + 1; // add 1 for null terminator
        assert(author_len <= AUTHOR_LEN);

        // convert text to wide string
        u32 text_len = strlen(argv[2]) + 1;
        wchar_t text_wide[text_len];
        u32 size = mbstowcs(text_wide, argv[2], text_len - 1);
        assert(size == text_len - 1);
        // null terminate
        text_wide[text_len - 1] = 0;

        u8 buf[STREAM_BUF] = {0};
        Message *m = (Message *)buf;

        memcpy(m->author, argv[1], author_len - 1);
        message_timestamp(m->timestamp);
        m->text_len = text_len;
        memcpy(&m->text, text_wide, m->text_len * sizeof(wchar_t));

        nsend = send(serverfd, buf, MESSAGELENP(m), 0);

        assert(nsend >= 0);

        printf("text_len: %d\n", text_len);
        fprintf(stdout, "Sent %d bytes.\n", nsend);
    }

    return 0;
}
