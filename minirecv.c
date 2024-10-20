#include "common.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

int main(void)
{
    int serverfd, clientfd;
    int on = 1;

    const struct sockaddr_in address = {
        AF_INET,
        htons(9999),
        {0},
    };

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    if (bind(serverfd, (struct sockaddr *)&address, sizeof(address)))
        return 1;

    listen(serverfd, 256);

    writef("serverfd: %d\n", serverfd);
    clientfd = accept(serverfd, 0, 0);
    writef("clientfd: %d\n", clientfd);

    struct pollfd fds[1] = {
        {clientfd, POLLIN, 0},
    };

    for (;;) {
        int ret = poll(fds, 1, 50000);
        if (ret == -1)
            return 2;

        if (fds[0].revents & POLLIN) {
            int nrecv;

            char buf[20];
            
            nrecv = recv(clientfd, buf, sizeof(buf), 0);
            printf("received %d bytes\n", nrecv);
            nrecv = recv(clientfd, buf, sizeof(buf), 0);
            printf("received %d bytes\n", nrecv);
            nrecv = recv(clientfd, buf, sizeof(buf), 0);
            printf("received %d bytes\n", nrecv);

            return 3;

            if (nrecv == -1) {
                return errno;
            } else if (nrecv == 0) {
                writef("Disconnect.\n");
                fds[0].fd = -1;
                fds[0].revents = 0;
                close(clientfd);
            } 

            writef("received: %d bytes\n", nrecv);
        }
    }

    return 0;
}
