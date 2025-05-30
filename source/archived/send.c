// minimal client implementation

#include <arpa/inet.h>
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHATTY_IMPL
#include "chatty.h"
#undef CHATTY_IMPL
#include "protocol.h"

int
main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: send <author> <msg>\n");
        return 1;
    }

    s32 err, serverfd;

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverfd != -1);

    const struct sockaddr_in address = {AF_INET, htons(PORT), {0}, {0}};
    err = connect(serverfd, (struct sockaddr*)&address, sizeof(address));
    assert(err == 0);

    // Get our ID
    ID id = 0;
    {
        // get author len
        u32 author_len = strlen(argv[1]);
        assert(author_len + 1 <= AUTHOR_LEN); // add 1 for null terminator

        // Introduce ourselves
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_INTRODUCTION);
        IntroductionMessage message;
        memcpy(message.author, argv[1], author_len);
        s32 nsend = sendAnyMessage(serverfd, header, &message);
        assert(nsend != -1);

        // Get id
        IDMessage id_message;
        s32 nrecv = recvAnyMessageType(serverfd, &header, &id_message, HEADER_TYPE_ID);
        assert(nrecv != -1);
        fprintf(stderr, "Got id: %lu\n", id_message.id);
        id = id_message.id;
    }

    // convert text to wide string
    u32 text_len = strlen(argv[2]) + 1;
    u32 text_wide[text_len];
    u32 size = mbstowcs((wchar_t*)text_wide, argv[2], text_len - 1);
    assert(size == text_len - 1);
    text_wide[text_len - 1] = 0;

    HeaderMessage header = HEADER_INIT(HEADER_TYPE_TEXT);
    header.id = id;
    TextMessage message;
    bzero(&message, TEXTMESSAGE_SIZE);
    message = (TextMessage){.timestamp = time(NULL), .len = text_len};

    s32 nsend = send(serverfd, &header, sizeof(header), 0);
    assert(nsend != -1);
    fprintf(stderr, "header bytes sent: %d\n", nsend);

    nsend = send(serverfd, &message, TEXTMESSAGE_SIZE, 0);
    assert(nsend != -1);
    fprintf(stderr, "message bytes sent: %d\n", nsend);

    u32 text_size = message.len * sizeof(*message.text);
    nsend = send(serverfd, text_wide, text_size, 0);
    fprintf(stderr, "text bytes sent: %d\n", nsend);

    return 0;
}
