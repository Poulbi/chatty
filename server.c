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
    struct pollfd* unifd; // Index in fds array
    struct pollfd* bifd;  // Index in fds array
} Client;
#define CLIENT_FMT "[%s](%lu)"
#define CLIENT_ARG(client) client.author, client.id

typedef enum {
    UNIFD = 0,
    BIFD
} ClientFD;

// TODO: remove global variable
// For handing out new ids to connections.
// Start at 1 because this makes 0 an invalid client id.
global_variable u32 nclients = 1;

// Returns client matching id in clients nclients number of clients.
// Returns 0 if no client was found or if id was 0.
Client*
getClientByID(Client* clients, u32 nclients, ID id)
{
    if (!id) return 0;

    for (u32 i = 0; i < nclients; i++)
	{
        if (clients[i].id == id)
            return clients + i;
    }
    return 0;
}

// Returns client matching fd in clients nclients number of clients.
// Returns 0 if no clients was found or if fd was -1.
Client* 
getClientByFD(Client* clients, u32 nclients, s32 fd)
{
    if (fd == -1) return 0;

    for (u32 i = 0; i < nclients; i++)
    {
        if ((clients[i].unifd && clients[i].unifd->fd == fd) ||
            (clients[i].bifd && clients[i].bifd->fd == fd))
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

    if (wide)
	{
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
sendToOthers(Client* clients, u32 nclients, Client* client, ClientFD type, HeaderMessage* header, void* anyMessage)
{
    s32 nsend;
    for (u32 i = 0; i < nclients; i ++)
	{
        if (clients + i == client) continue;

        if (type == UNIFD)
        {
            nsend = sendAnyMessage(client->unifd->fd, *header, anyMessage);
        }
        else if (type == BIFD)
        {
            nsend = sendAnyMessage(client->bifd->fd, *header, anyMessage);
        }
        assert(nsend != -1);
        loggingf("sendToOthers "CLIENT_FMT"|%s %d bytes\n", CLIENT_ARG((*client)), headerTypeString(header->type), nsend);
    }
}

// Send header and anyMessage to each connection in fds that is nfds number of connections.
// Type will filter out only connections matching the type.
void
sendToAll(Client* clients, u32 nclients, ClientFD type, HeaderMessage* header, void* anyMessage)
{
    s32 nsend;
    for (u32 i = 0; i < nclients; i++)
	{
        if (type == UNIFD)
        {
            if (clients[i].unifd && clients[i].unifd->fd != -1)
                nsend = sendAnyMessage(clients[i].unifd->fd, *header, anyMessage);
        }
        else if (type == BIFD)
        {
            if (clients[i].bifd && clients[i].bifd->fd != -1)
                nsend = sendAnyMessage(clients[i].bifd->fd, *header, anyMessage);
        }
        assert(nsend != -1);
        loggingf("sendToAll|[%s]->"CLIENT_FMT" %d bytes\n", headerTypeString(header->type),
                                                            CLIENT_ARG(clients[i]),
                                                            nsend);
    }
}

// Disconnect a client by closing the matching file descriptors
void
disconnect(Client* client)
{
    loggingf("Disconnecting "CLIENT_FMT"\n", CLIENT_ARG((*client)));
    if (client->unifd && client->unifd->fd != -1)
    {
        close(client->unifd->fd);
        client->unifd->fd = -1;
        client->unifd = 0;
    }
    if (client->bifd && client->bifd->fd != -1)
    {
        close(client->bifd->fd);
        client->bifd->fd = -1;
        client->bifd = 0;
    }
}

// Disconnects fds+conn from fds with nfds connections, then send a PresenceMessage to other
// clients about disconnection.
void
disconnectAndNotify(Client* clients, u32 nclients, Client* client)
{
    disconnect(client);

    local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_PRESENCE);
    header.id = client->id;
    PresenceMessage message = {.type = PRESENCE_TYPE_DISCONNECTED};
    sendToAll(clients, nclients, UNIFD, &header, &message);
}

// Receive authentication from pollfd->fd and create client out of it.  Look in
// clientsArena if it already exists.  Otherwise push a new onto the arena and write its information
// to clients_file.
// See "Authentication" in chatty.h
Client*
authenticate(Arena* clientsArena, s32 clients_file, struct pollfd* pollfd, HeaderMessage header)
{
    s32 nrecv = 0;
    Client* client = 0;

    loggingf("authenticate (%d)|" HEADER_FMT "\n", pollfd->fd, HEADER_ARG(header));

    /* Scenario 1: Search for existing client */
    if (header.type == HEADER_TYPE_ID)
    {
        client = getClientByID((Client*)clientsArena->addr, nclients, header.id);
        if (!client)
        {
            loggingf("authenticate (%d)|notfound\n", pollfd->fd);
            header.type = HEADER_TYPE_ERROR;
            ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_NOTFOUND);
            sendAnyMessage(pollfd->fd, header, &error_message);
            return 0;
        }

        loggingf("authenticate (%d)|found [%s](%lu)\n", pollfd->fd, client->author, client->id);
        if (!client->unifd)
            client->unifd = pollfd; 
        else if(!client->bifd)
            client->bifd = pollfd;
        else
            assert(0);

        header.type = HEADER_TYPE_ERROR;
        ErrorMessage error_message = ERROR_INIT(ERROR_TYPE_SUCCESS);
        sendAnyMessage(pollfd->fd, header, &error_message);

        return client;
    }

    /* Scenario 2: Create a new client */
    if (header.type == HEADER_TYPE_INTRODUCTION)
    {
        IntroductionMessage message;
        nrecv = recv(pollfd->fd, &message, sizeof(message), 0);
        if (nrecv != sizeof(message))
        {
            loggingf("authenticate (%d)|err: %d/%lu bytes\n", pollfd->fd, nrecv, sizeof(message));
            return 0;
        }

        // Copy metadata from IntroductionMessage
        client = ArenaPush(clientsArena, sizeof(*client));
        memcpy(client->author, message.author, AUTHOR_LEN);
        client->id = nclients;

        if (!client->unifd)
            client->unifd = pollfd; 
        else if(!client->bifd)
            client->bifd = pollfd;
        else
            assert(0);

        nclients++;

#ifdef IMPORT_ID
        write(clients_file, client, sizeof(*client));
#endif
        loggingf("authenticate (%d)|Added [%s](%lu)\n", pollfd->fd, client->author, client->id);

        // Send ID to new client
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
        header.id = client->id;
        s32 nsend = send(pollfd->fd, &header, sizeof(header), 0);
        assert(nsend != -1);
        assert(nsend == sizeof(header));

        return client;
    }

    loggingf("authenticate (%d)|Wrong header expected %s or %s\n", pollfd->fd,
                                                                  headerTypeString(HEADER_TYPE_INTRODUCTION),
                                                                  headerTypeString(HEADER_TYPE_ID));
    return 0;
}

int
main(int argc, char** argv)
{
    signal(SIGPIPE, SIG_IGN);

    logfd = 2;
    // optional logging
    if (argc > 1)
    {
        if (*argv[1] == '-')
            if (argv[1][1] == 'l')
            {
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

    s32 clients_file;
#ifdef IMPORT_ID
    clients_file = open(CLIENTS_FILE, O_RDWR | O_CREAT | O_APPEND, 0600);
    assert(clients_file != -1);
    struct stat statbuf;
    assert(fstat(clients_file, &statbuf) != -1);

    read(clients_file, clients, statbuf.st_size);
    if (statbuf.st_size > 0)
    {
        ArenaPush(&clientsArena, statbuf.st_size);
        loggingf("Imported %lu client(s)\n", statbuf.st_size / sizeof(*clients));
        nclients += statbuf.st_size / sizeof(*clients);

        // Reset pointers on imported clients
        for (u32 i = 0; i < nclients; i++)
        {
            clients[i].unifd = 0;
            clients[i].bifd = 0;
        }
    }
    for (u32 i = 0; i < nclients; i++)
        loggingf("Imported: " CLIENT_FMT "\n", CLIENT_ARG(clients[i]));
#else
    clients_file = 0;
#endif

    // Initialize the rest of the fds array
    for (u32 i = FDS_CLIENTS; i < MAX_CONNECTIONS; i++)
        fds[i] = newpollfd;

    while (1)
	{
        s32 err = poll(fds, FDS_SIZE, TIMEOUT);
        assert(err != -1);

        if (fds[FDS_STDIN].revents & POLLIN)
        {
            u8 c; // exit on ctrl-d
            if (!read(fds[FDS_STDIN].fd, &c, 1))
                break;
        }
        else if (fds[FDS_SERVER].revents & POLLIN)
        {
            // TODO: what if we are not aligned by 2 anymore?
            s32 clientfd = accept(serverfd, 0, 0);

            if (clientfd == -1)
            {
                loggingf("Error while accepting connection (%d)\n", clientfd);
                continue;
            }
            else
                loggingf("New connection(%d)\n", clientfd);

            // TODO: find empty space in arena (fragmentation)
            if (nclients + 1 == MAX_CONNECTIONS)
            {
                local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_ERROR);
                local_persist ErrorMessage message = ERROR_INIT(ERROR_TYPE_TOOMANYCONNECTIONS);
                sendAnyMessage(clientfd, header, &message);
                if (clientfd != -1)
                    close(clientfd);
                loggingf("Max clients reached. Rejected connection\n");
            }
            else
            {
                // no more space, allocate
                struct pollfd* pollfd = ArenaPush(&fdsArena, sizeof(*pollfd));
                pollfd->fd = clientfd;
                loggingf("Added pollfd(%d)\n", clientfd);
            }
        }

        for (u32 conn = FDS_CLIENTS; conn < FDS_SIZE; conn++)
        {
            if (!(fds[conn].revents & POLLIN)) continue;
            if (fds[conn].fd == -1) continue;
            loggingf("Message(%d)\n", fds[conn].fd);

            // We received a message, try to parse the header
            HeaderMessage header;
            s32 nrecv = recv(fds[conn].fd, &header, sizeof(header), 0);
            assert(nrecv != -1);

            Client* client;
            if (nrecv != sizeof(header))
            {
                client = getClientByFD(clients, nclients, fds[conn].fd);
                if (client)
                {
                    loggingf(CLIENT_FMT" %d/%lu bytes\n", CLIENT_ARG((*client)), nrecv, sizeof(header));
                    disconnectAndNotify(clients, nclients, client);
                    loggingf("Disconnected(%lu) [%s]\n", client->id, client->author);
                }
                else
                {
                    loggingf("Got error/disconnect from unauthenticated client\n");
                    close(fds[conn].fd);
                    fds[conn].fd = -1;
                }
                continue;
            }
            loggingf("Received(%d) -> " HEADER_FMT "\n", fds[conn].fd, HEADER_ARG(header));

            // Authentication
            if (header.id)
            {
                client = getClientByID(clients, nclients, header.id);
                if (!client)
                {
                    loggingf("No client for id %d\n", fds[conn].fd);

                    header.type = HEADER_TYPE_ERROR;
                    ErrorMessage message = ERROR_INIT(ERROR_TYPE_NOTFOUND);

                    sendAnyMessage(fds[conn].fd, header, &message);

                    // Reject connection
                    fds[conn].fd = -1;
                    close(fds[conn].fd);
                    continue;
                }
            }
            else
            {
                loggingf("No client for connection(%d)\n", fds[conn].fd);

                client = authenticate(&clientsArena, clients_file, fds + conn, header);

                if (!client)
                    loggingf("Could not initialize client (%d)\n", fds[conn].fd);
                else if (!client->bifd)
                {
                    // Send connected message to other clients if this was the first time -> bifd is
                    // not set yet.
                    local_persist HeaderMessage header = HEADER_INIT(HEADER_TYPE_PRESENCE);
                    header.id = client->id;
                    PresenceMessage message = {.type = PRESENCE_TYPE_CONNECTED};
                    sendToOthers(clients, nclients, client, UNIFD, &header, &message);
                }
                continue;
            }

            switch (header.type) {
            /* Send text message to all other clients */
            case HEADER_TYPE_TEXT:
            {
                TextMessage* text_message = recvTextMessage(&msgsArena, fds[conn].fd);
                loggingf("Received(%d)", fds[conn].fd);
                printTextMessage(text_message, client, 0);

                sendToOthers(clients, nclients, client, UNIFD, &header, text_message);
            } break;
            /* Send back client information */
            case HEADER_TYPE_ID:
            {
                HeaderMessage header = HEADER_INIT(HEADER_TYPE_INTRODUCTION);
                IntroductionMessage introduction_message;
                header.id = client->id;
                memcpy(introduction_message.author, client->author, AUTHOR_LEN);

                s32 nrecv = sendAnyMessage(fds[conn].fd, header, &introduction_message);
                assert(nrecv != -1);
            } break;
            default:
                loggingf("Unhandled '%s' from "CLIENT_FMT"(%d)\n", headerTypeString(header.type),
                                                                   CLIENT_ARG((*client)),
                                                                   fds[conn].fd);
                disconnectAndNotify(client, nclients, client);
                continue;
            }
        }
    }

#ifdef IMPORT_ID
    close(clients_file);
#endif

    return 0;
}
