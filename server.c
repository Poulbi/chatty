#include "arena.h"
#include "common.h"

#include <assert.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>

// timeout on polling
#define TIMEOUT 60 * 1000
// max pending connections
#define PENDING_MAX 16

// the size of pollfd element in the fdsArena
// note: clientsArena and pollfd_size must have been initialisezd
#define FDS_SIZE fdsArena->pos / pollfd_size

// enum for indexing the fds array
enum { FDS_STDIN = 0,
       FDS_SERVER,
       FDS_CLIENTS };

int main(void)
{
    u32 err, serverfd, clientfd;
    u16 nclient = 0;
    u32 on = 1;

    // Start listening on the socket
    {
        serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        assert(serverfd > 2);

        err = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (u8 *)&on, sizeof(on));
        assert(err == 0);

        const struct sockaddr_in address = {
            AF_INET,
            htons(PORT),
            {0},
        };

        err = bind(serverfd, (const struct sockaddr *)&address, sizeof(address));
        assert(err == 0);

        err = listen(serverfd, PENDING_MAX);
        assert(err == 0);
    }

    Arena *msgTextArena = ArenaAlloc(); // allocating text in messages that have a dynamic sized
    Message mrecv = {0};                // message used for receiving messages from clients
    u32 nrecv = 0;                      // number of bytes received
    u32 nsend = 0;                      // number of bytes sent
    u8 buf[STREAM_BUF] = {0};           // temporary buffer for received data, NOTE: this buffer
                                        // is also use for retransmitting received messages to other
                                        // clients.

    Arena *fdsArena = ArenaAlloc();
    struct pollfd *fds = fdsArena->memory; // helper for indexing memory
    struct pollfd c = {0, POLLIN, 0};      // helper client structure fore reusing
    struct pollfd *fdsAddr;                // used for copying clients
    const u64 pollfd_size = sizeof(struct pollfd);

    // initialize fds structure
    // add stdin (c.fd == 0)
    fdsAddr = ArenaPush(fdsArena, pollfd_size);
    memcpy(fdsAddr, &c, pollfd_size);
    // add serverfd
    c.fd = serverfd;
    fdsAddr = ArenaPush(fdsArena, pollfd_size);
    memcpy(fdsAddr, &c, pollfd_size);

    while (1) {
        err = poll(fds, FDS_SIZE, TIMEOUT);
        assert(err != -1);

        if (fds[FDS_STDIN].revents & POLLIN) {
            // helps for testing and exiting gracefully
            break;
        } else if (fds[FDS_SERVER].revents & POLLIN) {
            clientfd = accept(serverfd, NULL, NULL);
            assert(clientfd != -1);
            assert(clientfd > serverfd);

            // fill up a hole
            u8 found = 0;
            for (u32 i = FDS_CLIENTS; i < FDS_SIZE; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = clientfd;
                    // note we do not have to reset .revents because poll will set it to 0 next time
                    found = 1;
                    break;
                }
            }

            // allocate an extra client because there was no empty spot in the fds array
            if (!found) {
                // add client to arena
                fdsAddr = ArenaPush(fdsArena, pollfd_size);
                c.fd = clientfd;
                memcpy(fdsAddr, &c, pollfd_size);
            }

            nclient++;
            fprintf(stdout, "connected(%d).\n", clientfd - serverfd);
        }

        for (u32 i = FDS_CLIENTS; i < (FDS_SIZE); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;
            if (fds[i].fd == -1)
                continue;

            nrecv = recv(fds[i].fd, buf, STREAM_LIMIT, 0);
            assert(nrecv >= 0);

            if (nrecv == 0) {
                fprintf(stdout, "disconnected(%d). \n", fds[i].fd - serverfd);
                shutdown(fds[i].fd, SHUT_RDWR);
                close(fds[i].fd); // send close to client
                fds[i].fd = -1;   // ignore in the future
                continue;
            }

            // TODO: Do not print the message in the logs
            fprintf(stdout, "message(%d): %d bytes.\n", fds[i].fd - serverfd, nrecv); 

            for (u32 j = FDS_CLIENTS; j < (FDS_SIZE); j++) {
                if (j == i)
                    continue;
                if (fds[j].fd == -1)
                    continue;

                nsend = send(fds[j].fd, buf, nrecv, 0);
                assert(nsend != 1);
                assert(nsend == nrecv);
                fprintf(stdout, "retransmitted(%d->%d).\n", fds[i].fd - serverfd, fds[j].fd - serverfd);
            }

            ArenaPop(msgTextArena, mrecv.text_len);
        }
    }

    ArenaRelease(fdsArena);
    ArenaRelease(msgTextArena);

    return 0;
}
