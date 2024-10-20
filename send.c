#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

// NOTE: Errno could be unset and contain an error for a previous command
void debug_panic(const char *msg)
{
    writef("%s errno: %d\n", msg, errno);
    raise(SIGINT);
}

int main(void)
{
    // time for a new entered message
    time_t now;
    // localtime of new sent message
    struct tm *ltime;

    int serverfd;

    struct message input = {
        .author = "Friendship",
    };

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd == -1)
        debug_panic("Error while getting socket.");

    // Set timestamp for the message
    time(&now);
    ltime = localtime(&now);
    strftime(input.timestamp, sizeof(input.timestamp), "%H:%M:%S", ltime);

    input.text = "HII!!";
    input.len  = str_len(input.text);

    const struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        {0},
    };

    if (connect(serverfd, (struct sockaddr *)&address, sizeof(address)))
        debug_panic("Error while connecting.");

    u16 buf_len = MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN + input.len;
    printf("buf_len: %d\n", buf_len);
    char buf[buf_len];
    str_cpy(buf, input.author);
    str_cpy(buf + MESSAGE_AUTHOR_LEN, input.timestamp);
    str_cpy(buf + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN, input.text);

    int n = send(serverfd, &buf, buf_len, 0);
    if (n == -1)
        debug_panic("Error while sending message");
    writef("%d bytes sent.\n", n);

    {
        input.text  = "cleared";
        u16 buf_len = MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN + input.len;
        printf("buf_len: %d\n", buf_len);
        char buf[buf_len];
        str_cpy(buf, input.author);
        str_cpy(buf + MESSAGE_AUTHOR_LEN, input.timestamp);
        str_cpy(buf + MESSAGE_AUTHOR_LEN + MESSAGE_TIMESTAMP_LEN, input.text);
        int n = send(serverfd, &buf, buf_len, 0);
        if (n == -1)
            debug_panic("Error while sending message");
        writef("%d bytes sent.\n", n);
    }

    return 0;
}
