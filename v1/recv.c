// Minimal server implementation for probing out things

#include "common.h"
#include <arpa/inet.h>
#include <assert.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void)
{
    u32 serverfd, clientfd;
    u8 on = 1;

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    u32 err = bind(serverfd, (struct sockaddr *)&address, sizeof(address));
    assert(err == 0);

    err = listen(serverfd, 256);
    assert(err == 0);

    clientfd = accept(serverfd, 0, 0);
    assert(clientfd != -1);

    struct pollfd fds[1] = {
        {clientfd, POLLIN, 0},
    };

    for (;;) {
        int ret = poll(fds, 1, 50000);
        assert(ret != -1);

        if (fds[0].revents & POLLIN) {
            u8 recv_buf[BUF_MAX];
            u32 nrecv = recv(clientfd, recv_buf, sizeof(recv_buf), 0);
            assert(nrecv >= 0);

            writef("client(%d): %d bytes received.\n", clientfd, nrecv);
            if (nrecv == 0) {
                writef("client(%d): disconnected.\n", clientfd);
                fds[0].fd = -1;
                fds[0].revents = 0;
                err = close(clientfd);
                assert(err == 0);

                return 0;
            }
        }
    }

    return 0;
}
