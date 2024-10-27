#include "chatty.h"

#include <assert.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// timeout on polling
#define TIMEOUT 60 * 1000
// max pending connections
#define MAX_CONNECTIONS 16
// Get number of connections from arena position
// NOTE: this is somewhat wrong, because of when disconnections happen
#define FDS_SIZE (fdsArena->pos / sizeof(*fds))

// enum for indexing the fds array
enum { FDS_STDIN = 0,
       FDS_SERVER,
       FDS_CLIENTS };

// Has information on clients
// For each pollfd in fds there should be a matching client in clients
// clients[i - FDS_CLIENTS] <=> fds[i]
typedef struct {
    u8 author[AUTHOR_LEN]; // matches author property on other message types
    Bool initialized;      // boolean
} Client;

int
main(void)
{
    s32 err, serverfd, clientfd;
    u32 on = 1;

    // Start listening on the socket
    {
        serverfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
        assert(serverfd > 2);

        err = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (u8*)&on, sizeof(on));
        assert(err == 0);

        const struct sockaddr_in address = {
            AF_INET,
            htons(PORT),
            {0},
        };

        err = bind(serverfd, (const struct sockaddr*)&address, sizeof(address));
        assert(err == 0);

        err = listen(serverfd, MAX_CONNECTIONS);
        assert(err == 0);
    }

    Arena* msgsArena = ArenaAlloc(Megabytes(128)); // storing received messages
                                                   // NOTE: sent messages?
    s32 nrecv = 0;                                 // number of bytes received
    s32 nsend = 0;                                 // number of bytes sent

    Arena* clientsArena = ArenaAlloc(MAX_CONNECTIONS * sizeof(Client));
    Arena* fdsArena = ArenaAlloc(MAX_CONNECTIONS * sizeof(struct pollfd));
    struct pollfd* fds = fdsArena->addr;
    Client* clients = clientsArena->addr;

    struct pollfd* fdsAddr;
    struct pollfd newpollfd = {-1, POLLIN, 0};

    // initialize fds structure
    newpollfd.fd = 0;
    fdsAddr = ArenaPush(fdsArena, sizeof(*fds));
    memcpy(fdsAddr, &newpollfd, sizeof(*fds));
    // add serverfd
    newpollfd.fd = serverfd;
    fdsAddr = ArenaPush(fdsArena, sizeof(*fds));
    memcpy(fdsAddr, &newpollfd, sizeof(*fds));
    newpollfd.fd = -1;

    // Initialize the rest of the fds array
    for (u32 i = FDS_CLIENTS; i < MAX_CONNECTIONS; i++)
        fds[i] = newpollfd;

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
            fprintf(stdout, "New connection(%d).\n", clientfd);

            // fill up a hole
            u8 found;
            for (found = FDS_CLIENTS; found < FDS_SIZE; found++)
                if (fds[found].fd == -1)
                    break;
            if (found == FDS_SIZE) {
                // no more space, allocate
                struct pollfd* pollfd = ArenaPush(fdsArena, sizeof(*pollfd));
                pollfd->fd = clientfd;
                pollfd->events = POLLIN;
            } else if (found == MAX_CONNECTIONS) {
                // TODO: reject connection
                close(clientfd);
                fprintf(stdout, "Max clients reached.");
            } else {
                // hole found
                fds[found].fd = clientfd;
                fds[found].events = POLLIN;
                fprintf(stdout, "Added pollfd(%d).\n", clientfd);
            }
        }

        // Check for messages from clients
        for (u32 i = FDS_CLIENTS; i < (FDS_SIZE); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;
            assert(fds[i].fd != -1);

            fprintf(stdout, "Message(%d).\n", fds[i].fd);
            // If this is the first message from the client it must be a presence message indicated
            // it connected.
            Client* client = clients + i - FDS_CLIENTS;
            if (!client->initialized) {
                fprintf(stdout, " Adding to clients(%d).\n", fds[i].fd);
                // Wait for PresenceMessage from new client to get author information
                HeaderMessage header;
                // TODO: handle wrong message, disconnection, etc.
                nrecv = recv(clientfd, &header, sizeof(header), 0);
                assert(nrecv != -1);
                assert(nrecv == sizeof(header));
                if (header.type != HEADER_TYPE_PRESENCE) {
                    // TODO: reject connection
                    close(clientfd);
                    continue;
                }
                fprintf(stdout, "  Got header(%d).\n", fds[i].fd);

                PresenceMessage message;
                // TODO: handle wrong message
                nrecv = recv(clientfd, &message, sizeof(message), 0);
                assert(nrecv != -1);
                assert(nrecv == sizeof(message));
                fprintf(stdout, "  Got presence message(%d).\n", fds[i].fd);

                memcpy(client->author, message.author, AUTHOR_LEN);
                client->initialized = True;

                fprintf(stdout, "  Added to clients(%d): %s\n", fds[i].fd, client->author);

                // Notify other clients from this new one
                // Reuse header and message
                for (u32 j = FDS_CLIENTS; j < (FDS_SIZE); j++) {
                    if (fds[j].fd == fds[i].fd)
                        continue;
                    if (fds[j].fd == -1)
                        continue;
                    fprintf(stdout, "  Notifying (%d)\n", fds[j].fd);

                    nsend = send(fds[j].fd, &header, sizeof(header), 0);
                    assert(nsend != -1);
                    assert(nsend == sizeof(header));
                    nsend = send(fds[j].fd, &message, sizeof(message), 0);
                    assert(nsend != -1);
                    assert(nsend == sizeof(message));
                }
                continue;
            }

            // We received a message, try to parse the header
            HeaderMessage header;
            nrecv = recv(fds[i].fd, &header, sizeof(header), 0);
            assert(nrecv != -1);

            if (nrecv == 0) {
                fprintf(stdout, "Disconnected(%d). \n", fds[i].fd);
                shutdown(fds[i].fd, SHUT_RDWR);
                close(fds[i].fd);                             // send close to client
                fds[i].fd = -1;                               // ignore in the future
                clients[i - FDS_CLIENTS].initialized = False; // deinitialize client
                                                              //
                // Send disconnection to other connected clients
                HeaderMessage header = HEADER_PRESENCEMESSAGE;
                PresenceMessage message = {
                    .type = PRESENCE_TYPE_DISCONNECTED
                };
                memcpy(message.author, clients[i - FDS_CLIENTS].author, AUTHOR_LEN);
                for (u32 j = FDS_CLIENTS; j < FDS_SIZE; j++) {
                    if (fds[j].fd == fds[i].fd)
                        continue;
                    if (fds[j].fd == -1)
                        continue;
                    nsend = send(fds[j].fd, &header, sizeof(header), 0);
                    assert(nsend != -1);
                    assert(nsend == sizeof(header));
                    nsend = send(fds[j].fd, &message, sizeof(message), 0);
                    assert(nsend != -1);
                    assert(nsend == sizeof(message));
                }

                continue;
            }

            assert(nrecv == sizeof(header));
            fprintf(stderr, " Received(%d): %d bytes -> " PH_FMT "\n", fds[i].fd, nrecv, PH_ARG(header));

            switch (header.type) {
            case HEADER_TYPE_TEXT:;
                TextMessage* message;
                nrecv = recvTextMessage(msgsArena, fds[i].fd, &message);
                fprintf(stderr, " Received(%d): %d bytes -> ", fds[i].fd, nrecv);
                printTextMessage(message, 0);

                HeaderMessage header = HEADER_TEXTMESSAGE;
                // Send message to all other clients
                for (u32 j = FDS_CLIENTS; j < FDS_SIZE; j++) {
                    if (fds[j].fd == fds[i].fd) continue;
                    if (fds[j].fd == -1) continue;

                    // NOTE: I wonder if this is more expensive than constructing a buffer and sending
                    // that
                    u32 nsend_total = 0;
                    nsend = send(fds[j].fd, &header, sizeof(header), 0);
                    assert(nsend != 1);
                    assert(nsend == sizeof(header));
                    nsend_total += nsend;
                    nsend = send(fds[j].fd, message, TEXTMESSAGE_SIZE, 0);
                    assert(nsend != -1);
                    assert(nsend == TEXTMESSAGE_SIZE);
                    nsend_total += nsend;
                    nsend = send(fds[j].fd, &message->text, message->len * sizeof(*message->text), 0);
                    assert(nsend != -1);
                    assert(nsend == (message->len * sizeof(*message->text)));
                    nsend_total += nsend;

                    fprintf(stdout, "  Retransmitted(%d->%d) %d bytes.\n", fds[i].fd, fds[j].fd, nsend_total);
                }
                break;
            default:
                fprintf(stdout, " Got unhandled message type '%s' from client %d", headerTypeString(header.type), fds[i].fd);
                continue;
            }
        }
    }

    ArenaRelease(clientsArena);
    ArenaRelease(fdsArena);
    ArenaRelease(msgsArena);

    return 0;
}
