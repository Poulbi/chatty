// minimal client implementation

#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chatty.h"

int
main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: send <author> <msg>\n");
        return 1;
    }

    s32 err, serverfd, nsend;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverfd != -1);

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };
    err = connect(serverfd, (struct sockaddr*)&address, sizeof(address));
    assert(err == 0);

    // convert text to wide string
    u32 text_len = strlen(argv[2]) + 1;
    u32 text_wide[text_len];
    u32 size = mbstowcs((wchar_t*)text_wide, argv[2], text_len - 1);
    assert(size == text_len - 1);
    text_wide[text_len - 1] = 0;
    u32 author_len = strlen(argv[1]);
    assert(author_len + 1 <= AUTHOR_LEN); // add 1 for null terminator

    // Introduce ourselves
    {
        HeaderMessage header = HEADER_PRESENCEMESSAGE;
        PresenceMessage message;
        memcpy(message.author, argv[1], author_len);
        nsend = send(serverfd, &header, sizeof(header), 0);
        assert(nsend != -1);
        nsend = send(serverfd, &message, sizeof(message), 0);
        assert(nsend != -1);
    }

    HeaderMessage header = HEADER_TEXTMESSAGE;
    TextMessage* message;

    u8 buf[text_len * sizeof(*text_wide) + TEXTMESSAGE_SIZE];
    bzero(buf, sizeof(buf));
    message = (TextMessage*)buf;

    memcpy(message->author, argv[1], author_len);
    message->timestamp = time(NULL);
    message->len = text_len;
    memcpy(&message->text, text_wide, text_len * sizeof(*message->text));

    nsend = send(serverfd, &header, sizeof(header), 0);
    assert(nsend != -1);
    printf("header bytes sent: %d\n", nsend);
    nsend = send(serverfd, buf, sizeof(buf), 0);
    assert(nsend != -1);

    printf("text length: %d\n", text_len);
    printf("buf size: %lu\n", sizeof(buf));
    printf("text size: %lu\n", sizeof(*text_wide) * text_len);
    printf("message bytes sent: %d\n", nsend);

    return 0;
}
