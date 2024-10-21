// minimal client implementation
#include "common.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

u32 serverfd;

// NOTE: Errno could be unset and contain an error for a previous command
void debug_panic(const char *msg)
{
    writef("%s errno: %d\n", msg, errno);
    raise(SIGINT);
}

// get current time in timestamp string
void timestamp(char timestamp[MESSAGE_TIMESTAMP_LEN])
{
    time_t now;
    struct tm *ltime;
    time(&now);
    ltime = localtime(&now);
    strftime(timestamp, MESSAGE_TIMESTAMP_LEN, "%H:%M:%S", ltime);
}

int main(void)
{
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd == -1)
        debug_panic("Error while getting socket.");

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    if (connect(serverfd, (struct sockaddr *)&address, sizeof(address)))
        debug_panic("Error while connecting.");

    struct message input = {
        .author    = "Friendship",
    };
    input.text = "Hello from send";
    input.len  = str_len(input.text);
    printf("input.len: %d\n", input.len);
    timestamp(input.timestamp);

    send_message(input, serverfd);

    return 0;
}
