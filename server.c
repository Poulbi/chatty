#include "chatty.h"
#include "protocol.h"

#include <assert.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

// timeout on polling
#define TIMEOUT 60 * 1000
// max pending connections
#define MAX_CONNECTIONS 16
// Get number of connections from arena position
// NOTE: this is somewhat wrong, because of when disconnections happen
#define FDS_SIZE (fdsArena.pos / sizeof(struct pollfd))
#define CLIENTS_SIZE (clientsArena.pos / sizeof(Client))

// Enable/Disable saving clients permanently to file
#define IMPORT_ID
// Where to save clients
#define CLIENTS_FILE "_clients"
// Where to write logs
#define LOGFILE "server.log"
// Log to LOGFILE instead of stderr
// #define LOGGING

// enum for indexing the fds array
enum { FDS_STDIN = 0,
       FDS_SERVER,
       FDS_CLIENTS };

// Client information
typedef struct {
    u8 author[AUTHOR_LEN]; // matches author property on other message types
    ID id;
    struct pollfd* pollunifd; // Index in fds array
    struct pollfd* pollbifd;  // Index in fds array
} Client;
#define CLIENT_FMT "[%s](%lu)"
#define CLIENT_ARG(client) client.author, client.id

typedef enum {
    UNIFD = 0,
    BIFD
} ClientFD;

// TODO: remove
// For handing out new ids to connections.
global_variable u32 nclients = 0;

// Returns client matching id in clients.
// clientsArena is used to get an upper bound.
// Returns 0 if there was no client found.
Client*
getClientByID(Arena* clientsArena, ID id)
{
    Client* clients = clientsArena->addr;
    for (u32 i = 0; i < (clientsArena->pos / sizeof(*clients)); i++) {
        if (clients[i].id == id)
            return clients + i;
    }
    return 0;
}

// Print TextMessage prettily
void
printTextMessage(TextMessage* message, Client* client, u8 wide)
{
    u8 timestamp[TIMESTAMP_LEN] = {0};
    formatTimestamp(timestamp, message->timestamp);

    if (wide) {
        setlocale(LC_ALL, "");
        wprintf(L"TextMessage: %s [%s] %ls\n", timestamp, client->author, (wchar_t*)&message->text);
    } else {
        u8 str[message->len];
        wcstombs((char*)str, (wchar_t*)&message->text, message->len * sizeof(*message->text));
        loggingf("TextMessage: %s [%s] (%d)%s\n", timestamp, client->author, message->len, str);
    }
}

// Send header and anyMessage to each connection in fds that is nfds number of connections except
// for connfd.
// Type will filter out only connections matching the type.
void
sendToOthers(struct pollfd* fds, u32 nfds, s32 connfd, ClientFD type, HeaderMessage* header, void* anyMessage)
{
    s32 nsend;
    for (u32 i = FDS_CLIENTS + type; i < nfds; i += 2) {
        if (fds[i].fd == connfd) continue;
        if (fds[i].fd == -1) continue;

        nsend = sendAnyMessage(fds[i].fd, header, anyMessage);
        loggingf("sendToOthers(%d)|[%s]->%d %d bytes\n", connfd, headerTypeString(header->type), fds[i].fd, nsend);
    }
}

// Send header and anyMessage to each connection in fds that is nfds number of connections.
// Type will filter out only connections matching the type.
void
sendToAll(struct pollfd* fds, u32 nfds, ClientFD type, HeaderMessage* header, void* anyMessage)
{
    for (u32 i = FDS_CLIENTS + type; i < nfds; i += 2) {
        if (fds[i].fd == -1) continue;
        s32 nsend = sendAnyMessage(fds[i].fd, header, anyMessage);
        loggingf("sendToAll|[%s]->%d %d bytes\n", headerTypeString(header->type), fds[i].fd, nsend);
    }
}

// Disconnect a client by closing the matching file descriptors
void
disconnect(struct pollfd* pollfd, Client* client)
{
    loggingf("Disconnecting "CLIENT_FMT"\n", CLIENT_ARG((*client)));
    if (pollfd[UNIFD].fd != -1) {
        close(pollfd[UNIFD].fd);
    }
    if (pollfd[BIFD].fd != -1) {
        close(pollfd[BIFD].fd);
    }
    pollfd[UNIFD].fd = -1;
    pollfd[BIFD].fd = -1;
    // TODO: mark as free
    if (client) {
        client->pollunifd = 0;
        client->pollbifd = 0;
    }
}

// Disconnects fds+conn from fds with nfds connections, then send a PresenceMessage to other
// clients about disconnection.
void
disconnectAndNotify(Client* client, struct pollfd* fds, u32 nfds, u32 conn)
{
    disconnect(fds + conn, client);

    local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_PRESENCE);
    PresenceMessage message = {.id = client->id, .type = PRESENCE_TYPE_DISCONNECTED};
    sendToAll(fds, nfds, UNIFD, &header, &message);
}

// Receive authentication from pollfd->fd and create client out of it.  Look in
// clientsArena if it already exists.  Otherwise push a new onto the arena and write its information
// to clients_file.
// See "Authentication" in chatty.h
Client*
authenticate(Arena* clientsArena, s32 clients_file, struct pollfd* clientfds)
{
    s32 nrecv = 0;
    Client* clients = clientsArena->addr;

    HeaderMessage header;
    nrecv = recv(clientfds[BIFD].fd, &header, sizeof(header), 0);
    if (nrecv != sizeof(header)) {
        loggingf("authenticate(%d)|err: %d/%lu bytes\n", clientfds[BIFD].fd, nrecv, sizeof(header));
        return 0;
    }
    loggingf("authenticate(%d)|" HEADER_FMT "\n", clientfds[BIFD].fd, HEADER_ARG(header));

    Client* client = 0;
    // Scenario 1: Search for existing client
    if (header.type == HEADER_TYPE_ID) {
        IDMessage message;
        nrecv = recv(clientfds[BIFD].fd, &message, sizeof(message), 0);
        if (nrecv != sizeof(message)) {
            loggingf("authenticate(%d)|err: %d/%lu bytes\n", clientfds[BIFD].fd, nrecv, sizeof(message));
            return 0;
        }

        client = getClientByID(clientsArena, message.id);
        if (client) {
            loggingf("authenticate(%d)|found [%s](%lu)\n", clientfds[BIFD].fd, client->author, client->id);
            header.type = HEADER_TYPE_ERROR;
            // TODO: allow multiple connections
            if (client->pollunifd != 0 || client->pollbifd != 0) {
                loggingf("authenticate(%d)|err: already connected\n", clientfds[BIFD].fd);
                ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_ALREADYCONNECTED);
                sendAnyMessage(clientfds[BIFD].fd, &header, &error_message);
                return 0;
            }
            ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_SUCCESS);
            sendAnyMessage(clientfds[BIFD].fd, &header, &error_message);
        } else {
            loggingf("authenticate(%d)|notfound\n", clientfds[BIFD].fd);
            header.type = HEADER_TYPE_ERROR;
            ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_NOTFOUND);
            sendAnyMessage(clientfds[BIFD].fd, &header, &error_message);
            return 0;
        }
        // Scenario 2: Create a new client
    } else if (header.type == HEADER_TYPE_INTRODUCTION) {
        IntroductionMessage message;
        nrecv = recv(clientfds[BIFD].fd, &message, sizeof(message), 0);
        if (nrecv != sizeof(message)) {
            loggingf("authenticate(%d)|err: %d/%lu bytes\n", clientfds[BIFD].fd, nrecv, sizeof(message));
            return 0;
        }

        // Copy metadata from IntroductionMessage
        client = ArenaPush(clientsArena, sizeof(*clients));
        memcpy(client->author, message.author, AUTHOR_LEN);
        client->id = nclients;
        nclients++;

        // Save client
#ifdef IMPORT_ID
        write(clients_file, client, sizeof(*client));
#endif
        loggingf("authenticate(%d)|Added [%s](%lu)\n", clientfds[BIFD].fd, client->author, client->id);

        HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
        IDMessage id_message = {.id = client->id};
        sendAnyMessage(clientfds[BIFD].fd, &header, &id_message);
    } else {
        loggingf("authenticate(%d)|Wrong header expected %s or %s\n", clientfds[BIFD].fd, headerTypeString(HEADER_TYPE_INTRODUCTION), headerTypeString(HEADER_TYPE_ID));
        return 0;
    }
    assert(client != 0);

    client->pollunifd = clientfds;
    client->pollbifd = clientfds + 1;

    return client;
}

int
main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);

    logfd = 2;
    // optional logging
    if (argc > 1) {
        if (*argv[1] == '-')
            if (argv[1][1] == 'l') {
                logfd = open(LOGFILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
                assert(logfd != -1);
            }
    }

    s32 serverfd;
    // Start listening on the socket
    {
        s32 err;
        u32 on = 1;
        serverfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        assert(serverfd > 2);

        err = setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, (u8*)&on, sizeof(on));
        assert(!err);

        const struct sockaddr_in address = {
            AF_INET,
            htons(PORT),
            {0},
            {0},
        };

        err = bind(serverfd, (const struct sockaddr*)&address, sizeof(address));
        assert(!err);

        err = listen(serverfd, MAX_CONNECTIONS);
        assert(!err);
        loggingf("Listening on :%d\n", PORT);
    }

    Arena clientsArena;
    Arena fdsArena;
    Arena msgsArena;
    ArenaAlloc(&clientsArena, MAX_CONNECTIONS * sizeof(Client));
    ArenaAlloc(&fdsArena, MAX_CONNECTIONS * 2 * sizeof(struct pollfd));
    ArenaAlloc(&msgsArena, Megabytes(128)); // storing received messages
    struct pollfd* fds = fdsArena.addr;
    Client* clients = clientsArena.addr;

    // Initializing fds
    struct pollfd* fdsAddr;
    struct pollfd newpollfd = {-1, POLLIN, 0}; // for copying with events already set
    // initialize fds structure
    newpollfd.fd = 0;
    fdsAddr = ArenaPush(&fdsArena, sizeof(*fds));
    memcpy(fdsAddr, &newpollfd, sizeof(*fds));
    // add serverfd
    newpollfd.fd = serverfd;
    fdsAddr = ArenaPush(&fdsArena, sizeof(*fds));
    memcpy(fdsAddr, &newpollfd, sizeof(*fds));
    newpollfd.fd = -1;

#ifdef IMPORT_ID
    s32 clients_file = open(CLIENTS_FILE, O_RDWR | O_CREAT | O_APPEND, 0600);
    assert(clients_file != -1);
    struct stat statbuf;
    assert(fstat(clients_file, &statbuf) != -1);

    read(clients_file, clients, statbuf.st_size);
    if (statbuf.st_size > 0) {
        ArenaPush(&clientsArena, statbuf.st_size);
        loggingf("Imported %lu client(s)\n", statbuf.st_size / sizeof(*clients));
        nclients += statbuf.st_size / sizeof(*clients);
    }
    for (u32 i = 0; i < nclients; i++)
        loggingf("Imported: " CLIENT_FMT "\n", CLIENT_ARG(clients[i]));
#endif

    // Initialize the rest of the fds array
    for (u32 i = FDS_CLIENTS; i < MAX_CONNECTIONS; i++)
        fds[i] = newpollfd;
    // Reset file descriptors on imported clients
    for (u32 i = 0; i < CLIENTS_SIZE; i++) {
        clients[i].pollunifd = 0;
        clients[i].pollbifd = 0;
    }

    while (1) {
        s32 err = poll(fds, FDS_SIZE, TIMEOUT);
        assert(err != -1);

        if (fds[FDS_STDIN].revents & POLLIN) {
            u8 c; // exit on ctrl-d
            if (!read(fds[FDS_STDIN].fd, &c, 1))
                break;
        } else if (fds[FDS_SERVER].revents & POLLIN) {
            // TODO: what if we are not aligned by 2 anymore?
            s32 unifd = accept(serverfd, 0, 0);
            s32 bifd = accept(serverfd, 0, 0);

            if (unifd == -1 || bifd == -1) {
                loggingf("Error while accepting connection (%d,%d)\n", unifd, bifd);
                if (unifd != -1) close(unifd);
                if (bifd != -1) close(bifd);
                continue;
            } else
                loggingf("New connection(%d,%d)\n", unifd, bifd);

            // TODO: find empty space in arena
            if (nclients + 1 == MAX_CONNECTIONS) {
                local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_ERROR);
                local_persist ErrorMessage message = ERROR_INIT(ERROR_TYPE_TOOMANYCONNECTIONS);
                sendAnyMessage(unifd, &header, &message);
                if (unifd != -1)
                    close(unifd);
                if (bifd != -1)
                    close(bifd);
                loggingf("Max clients reached. Rejected connection\n");
            } else {
                // no more space, allocate
                struct pollfd* clientfds = ArenaPush(&fdsArena, 2 * sizeof(*clientfds));
                clientfds[UNIFD].fd = unifd;
                clientfds[UNIFD].events = POLLIN;
                clientfds[BIFD].fd = bifd;
                clientfds[BIFD].events = POLLIN;
                loggingf("Added pollfd(%d,%d)\n", unifd, bifd);
            }
        }

        // Check for messages from clients in their unifd
        for (u32 conn = FDS_CLIENTS; conn < FDS_SIZE; conn += 2) {
            if (!(fds[conn].revents & POLLIN)) continue;
            if (fds[conn].fd == -1) continue;
            loggingf("Message unifd (%d)\n", fds[conn].fd);

            // Get client associated with connection
            Client* client = 0;
            for (u32 j = 0; j < CLIENTS_SIZE; j++) {
                if (!clients[j].pollunifd)
                    continue;
                if (clients[j].pollunifd == fds + conn) {
                    client = clients + j;
                    break;
                }
            }
            if (!client) {
                loggingf("No client associated(%d)\n", fds[conn].fd);
                close(fds[conn].fd);
                continue;
            }
            loggingf("Found client(%lu) [%s] (%d)\n", client->id, client->author, fds[conn].fd);

            // We received a message, try to parse the header
            HeaderMessage header;
            s32 nrecv = recv(fds[conn].fd, &header, sizeof(header), 0);
            if (nrecv == 0) {
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                loggingf("Disconnected(%lu) [%s]\n", client->id, client->author);
                continue;
            } else if (nrecv != sizeof(header)) {
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                loggingf("error(%lu) [%s] %d/%lu bytes\n", client->id, client->author, nrecv, sizeof(header));
                continue;
            }
            loggingf("Received(%d) -> " HEADER_FMT "\n", fds[conn].fd, HEADER_ARG(header));

            switch (header.type) {
            case HEADER_TYPE_TEXT: {
                TextMessage* text_message = recvTextMessage(&msgsArena, fds[conn].fd);
                loggingf("Received(%d)", fds[conn].fd);
                printTextMessage(text_message, client, 0);

                sendToOthers(fds, FDS_SIZE, fds[conn].fd, UNIFD, &header, text_message);
            } break;
            // handle request for information about client id
            default:
                loggingf("Unhandled '%s' from client(%d)\n", headerTypeString(header.type), fds[conn].fd);
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                continue;
            }
        }

        // Check for messages from clients in their bifd
        for (u32 conn = FDS_CLIENTS + BIFD; conn < FDS_SIZE; conn += 2) {
            if (!(fds[conn].revents & POLLIN)) continue;
            if (fds[conn].fd == -1) continue;
            loggingf("Message bifd (%d)\n", fds[conn].fd);

            // Get client associated with connection
            Client* client = 0;
            for (u32 j = 0; j < CLIENTS_SIZE; j++) {
                if (!clients[j].pollbifd)
                    continue;
                if (clients[j].pollbifd == fds + conn) {
                    client = clients + j;
                    break;
                }
            }
            if (!client) {
                loggingf("No client for connection(%d)\n", fds[conn].fd);
#ifdef IMPORT_ID
                client = authenticate(&clientsArena, clients_file, fds + conn - 1);
#else
                client = authenticate(&clientsArena, 0, fds + conn - 1);
#endif
                // If the client sent an IDMessage but no ID was found authenticate() could return null
                if (!client) {
                    loggingf("Could not initialize client\n");
                    disconnect(fds + conn, 0);
                } else { // client was added/connected
                    local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_PRESENCE);
                    PresenceMessage message = {.id = client->id, .type = PRESENCE_TYPE_CONNECTED};
                    sendToOthers(fds, FDS_SIZE, fds[conn - BIFD].fd, UNIFD, &header, &message);
                }
                continue;
            }
            loggingf("Found client(%lu) [%s] (%d)\n", client->id, client->author, fds[conn].fd);

            // We received a message, try to parse the header
            HeaderMessage header;
            s32 nrecv = recv(fds[conn].fd, &header, sizeof(header), 0);
            if (nrecv == 0) {
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                loggingf("Disconnected(%lu) [%s]\n", client->id, client->author);
                continue;
            } else if (nrecv != sizeof(header)) {
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                loggingf("error(%lu) [%s] %d/%lu bytes\n", client->id, client->author, nrecv, sizeof(header));
                continue;
            }
            loggingf("Received(%d) -> " HEADER_FMT "\n", fds[conn].fd, HEADER_ARG(header));

            switch (header.type) {
            case HEADER_TYPE_ID: {
                // handle request for information about client id
                IDMessage id_message;
                nrecv = recv(fds[conn].fd, &id_message, sizeof(id_message), 0);

                Client* client = getClientByID(&clientsArena, id_message.id);
                if (!client) {
                    local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_ERROR);
                    local_persist ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_NOTFOUND);
                    sendAnyMessage(fds[conn].fd, &header, &error_message);
                    loggingf("Could not find %lu\n", id_message.id);
                    break;
                }
                HeaderMessage header = HEADER_INIT(HEADER_TYPE_INTRODUCTION);
                IntroductionMessage introduction_message;
                memcpy(introduction_message.author, client->author, AUTHOR_LEN);

                sendAnyMessage(fds[conn].fd, &header, &introduction_message);
            } break;
            default:
                loggingf("Unhandled '%s' from client(%d)\n", headerTypeString(header.type), fds[conn].fd);
                disconnectAndNotify(client, fds, FDS_SIZE, conn);
                continue;
            }
        }
    }

#ifdef IMPORT_ID
    close(clients_file);
#endif

    return 0;
}
