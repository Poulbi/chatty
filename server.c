// Server for chatty

// The idea is the following:
// - tcp server that you can send messages to
// - encrypted communication
// - history upon connecting
// - date of messages sent
// - author
// - fingeprint as ID for authorship
// - client for reading the messages and sending them at the same time

// TODO:
// - min height & width
// - wrapping input
// - max y for new messages and make them scroll
// - check resize event
// - asynchronously receive/send a message
// - fix receiving messages with arbitrary text length

// TODO: send message to all other clients
// - implement different rooms
// - implement history
// - implement tls

#include "common.h"
#include "config.h"
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CONNECTIONS 5
#define FD_MAX MAX_CONNECTIONS + 1

static const char *filename = "history.dat";

enum { FD_SERVER = 0 };
int serverfd;

void err_exit(const char *msg)
{
    if (serverfd)
        if (close(serverfd))
            writef("Error while closing server socket. errno: %d\n", errno);
    fprintf(stderr, "%s errno: %d\n", msg, errno);
    _exit(1);
}

int main(void)
{
    int clientfd;
    int nclient             = 0;
    int on                  = 1;
    struct message msg_recv = {0};

    serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)))
        err_exit("Error while setting socket option.");

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    if (bind(serverfd, (struct sockaddr *)&address, sizeof(address))) {
        err_exit("Error while binding.");
    }

    if (listen(serverfd, BUF_MAX)) {
        err_exit("Error while listening");
    }

    writef("Listening on localhost:%d\n", PORT);

    struct pollfd fds[MAX_CONNECTIONS + 1] = {
        {serverfd, POLLIN, 0}, // FD_SERVER
        {      -1, POLLIN, 0},
        {      -1, POLLIN, 0},
        {      -1, POLLIN, 0},
        {      -1, POLLIN, 0},
        {      -1, POLLIN, 0},
    };

    for (;;) {
        int ret = poll(fds, FD_MAX, 50000);
        if (ret == -1)
            err_exit("Error while polling");
        else if (ret == 0) {
            writef("polling timed out.\n");
            continue;
        }

        // New client tries to connect to serverfd
        if (fds[FD_SERVER].revents & POLLIN) {
            clientfd = accept(serverfd, NULL, NULL);

            // When MAX_CONNECTIONS is reached, close new clients trying to connect.
            if (nclient == MAX_CONNECTIONS) {
                writef("Max connections reached.\n");
                if (send(clientfd, 0, 0, 0) == -1)
                    err_exit("Error while sending EOF to client socket.");
                if (shutdown(clientfd, SHUT_RDWR))
                    err_exit("Error while shutting down client socket.");
                if (close(clientfd))
                    err_exit("Error while closing client socket.");
            } else if (clientfd != -1) {
                nclient++;

                // get a new available spot in the fds array
                int i;
                for (i = 0; i < MAX_CONNECTIONS; i++)
                    if (fds[i].fd == -1)
                        break;
                fds[i].fd = clientfd;
                writef("New client: %d, %d\n", i, clientfd);

            } else {
                writef("Could not accept client errno: %d\n", errno);
            }
        }

        // Check for events on connected clients
        for (int i = 1; i <= nclient; i++) {
            if (!(fds[i].revents & POLLIN))
                continue;

            int nrecv;

            clientfd = fds[i].fd;
            nrecv    = recv(clientfd, &msg_recv, sizeof(struct message), 0);

            if (nrecv == 0) {
                printf("client %d disconnected.\n", i);
                fds[i].fd      = -1;
                fds[i].revents = 0;
                if (shutdown(clientfd, SHUT_RDWR))
                    err_exit("Error while shutting down client %d socket.");
                if (close(clientfd))
                    err_exit("Error while cloing client socket.");
                nclient--;
                break;
            } else if (nrecv == -1) {
                err_exit("Error while receiving from client socket.");
            }

            printf("client %d sent %d bytes.\n", i, nrecv);

            // TODO:
            for (int j = 1; j <= nclient; j++) {
                // skip the client that sent the message
                if (j == i)
                    continue;
                if (send(fds[j].fd, &msg_recv, nrecv, 0) == -1)
                    printf("Error while sendig message to client %d. errno: %d\n", j, errno);
                else
                    printf("Retransmitted message to client %d.\n", j);
            }

            // // TODO: check if bytes are correct
            // FILE *f = fopen("srv_recv.bin", "wb");
            // fwrite(&msg_recv, sizeof(struct message), 1, f);
            // fclose(f);
            //
            // printf("written %lu bytes to srv_recv.bin\n", sizeof(msg_recv));
            // return 0;
        }
    }

    return 0;
}
