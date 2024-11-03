#define TB_IMPL
#include "termbox2.h"

#include "chatty.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <assert.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>

#define TIMEOUT_POLL 60 * 1000
// time to reconnect in seconds
#define TIMEOUT_RECONNECT 1
#define INPUT_LIMIT 512
// Filepath where user ID is stored
#define ID_FILE "_id"
// Filepath where logged
#define LOGFILE "chatty.log"
// enable logging
#define LOGGING

enum { FDS_UNI = 0, // for one-way communication with the server (eg. TextMessage)
       FDS_BI,      // For two-way communication with the server (eg. IDMessage)
       FDS_TTY,
       FDS_RESIZE,
       FDS_MAX };

typedef struct {
    u8 author[AUTHOR_LEN];
    ID id;
} User;
#define USER_FMT "[%s](%lu)"
#define USER_ARG(client) client.author, client.id

// User used by chatty
global_variable User user = {0};
// Address of chatty server
global_variable struct sockaddr_in address;

// fill str array with char
void
fillstr(u32* str, u32 ch, u32 len)
{
    for (u32 i = 0; i < len; i++)
        str[i] = ch;
}

// Centered popup displaying message in the appropriate cololrs
void
popup(u32 fg, u32 bg, char* text)
{
    u32 len = strlen(text);
    assert(len > 0);
    tb_print(global.width / 2 - len / 2, global.height / 2, fg, bg, text);
}

// Returns client in clientsArena matching id
// Returns user if the id was the user's ID
// Returns 0 if nothing was found
User*
getUserByID(Arena* clientsArena, ID id)
{
    if (id == user.id) return &user;

    User* clients = clientsArena->addr;
    for (u64 i = 0; i < (clientsArena->pos / sizeof(*clients)); i++)
    {
        if (clients[i].id == id)
            return clients + i;
    }
    return 0;
}

// Request information of client from fd byd id and add it to clientsArena
// Returns pointer to added client
User*
addUserInfo(Arena* clientsArena, s32 fd, u64 id)
{
    // Request information about ID
    HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
    header.id = id;
    s32 nsend = send(fd, &header, sizeof(header), 0);
    assert(nsend != -1);
    assert(nsend == sizeof(header));

    // Wait for response
    IntroductionMessage introduction_message;
    recvAnyMessageType(fd, &header, &introduction_message, HEADER_TYPE_INTRODUCTION);

    // Add the information
    User* client = ArenaPush(clientsArena, sizeof(*client));
    memcpy(client->author, introduction_message.author, AUTHOR_LEN);
    client->id = id;

    loggingf("Got " USER_FMT "\n", USER_ARG((*client)));
    return client;
}

// Tries to connect to address and populates resulting file descriptors in ConnectionResult.
s32
getConnection(struct sockaddr_in* address)
{
    s32 fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    s32 err = connect(fd, (struct sockaddr*)address, sizeof(*address));
    if (err) return -1;

    return fd;
}

ID
authenticate(User* user, s32 fd)
{
    if (user->id)
    {
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
        header.id = user->id;
        s32 nsend = send(fd, &header, sizeof(header), 0);
        assert(nsend == -1);
        assert(nsend == sizeof(header));

        s32 nrecv = recv(fd, &header, sizeof(header), 0);
        if (nrecv == 0)
            return 0;
        assert(nrecv != -1);
        assert(nrecv == sizeof(header));
        assert(header.type == HEADER_TYPE_ID);
        if (header.id == user->id)
            return header.id;
        else
            return 0;
    }
    else
    {
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_INTRODUCTION);
        IntroductionMessage message;
        memcpy(message.author, user->author, AUTHOR_LEN);
        sendAnyMessage(fd, header, &message);

        s32 nrecv = recv(fd, &header, sizeof(header), 0);
        if (nrecv == 0)
            return 0;
        assert(nrecv != -1);
        assert(nrecv == sizeof(header));
        assert(header.type == HEADER_TYPE_ID);
        assert(header.id);
        user->id = header.id;
        return header.id;
    }
}

// Connect to *address_ptr of type `struct sockaddr_in*`.  If it failed wait for TIMEOUT_RECONNECT
// seconds.
// This function is meant to be run by a thread.
// An offline server means fds[FDS_SERVER] is set to -1.  When online
// it is set to with the appropriate file descriptor.
// Returns 0.
#define Miliseconds(s) (s*1000*1000)
void*
threadReconnect(void* fds_ptr)
{
    s32 unifd, bifd;
    struct pollfd* fds = fds_ptr;
    struct timespec t = { 0, Miliseconds(300) }; // 300 miliseconds
    loggingf("Trying to reconnect\n");
    while (1)
    {
        // timeout
        nanosleep(&t, &t);

        unifd = getConnection(&address);
        if (unifd == -1)
        {
            loggingf("errno: %d\n", errno);
            continue;
        }
        bifd = getConnection(&address);
        if (bifd == -1)
        {
            loggingf("errno: %d\n", errno);
            close(unifd);
            continue;
        }

        loggingf("Reconnect succeeded (%d, %d), authenticating\n", unifd, bifd);

        if (authenticate(&user, unifd) &&
            authenticate(&user, bifd))
        {
            break;
        }

        close(unifd);
        close(bifd);
        loggingf("Failed, retrying...\n");
    }

    fds[FDS_BI].fd = bifd;
    fds[FDS_UNI].fd = unifd;

    // Redraw screen
    raise(SIGWINCH);

    return 0;
}

// Print `text` wrapped to limit_x.  It will print no more than limit_y lines.  x, y, fg and
// bg will be passed to the tb_printf() function calls.
// pfx is a string that will be printed first and will not be wrapped on characters like msg->text,
// this is useful when for example: printing messages and wanting to have consistent
// timestamp+author name.
// Returns the number of lines printed.
// TODO: (bug) text after pfx is wrapped one too soon
// TODO: text == 0 to know how many lines *would* be printed
// - no this should be a separate function
// TODO: check if text[i] goes out of bounds
u32
tb_printf_wrap(u32 x, u32 y, u32 fg, u32 bg, u32* text, s32 text_len, u32 fg_pfx, u32 bg_pfx, u8* pfx, s32 limit_x, u32 limit_y)
{
    assert(limit_x > 0);

    // lines y, incremented after each wrap
    s32 ly = y;
    // character the text is split on
    u32 t = 0;
    // index used for searching in string
    s32 i = limit_x;
    // previous i for windowing through the text
    s32 offset = 0;
    // used when retrying to get a longer limit
    u32 failed = 0;

    // NOTE: We can assume that we need to wrap, therefore print a newline after the prefix string
    if (pfx != 0)
    {
        tb_printf(x, ly, fg_pfx, bg_pfx, "%s", pfx);

        // If the text fits on one line print the text and return
        // Otherwise print the text on the next line
        s32 pfx_len = strlen((char*)pfx);
        if (limit_x > pfx_len + text_len)
        {
            tb_printf(x + pfx_len, y, fg, bg, "%ls", text);
            return 1;
        }
        else
        {
            ly++;
        }
    }

    /// Algorithm
    // 1. Start at limit
    // 2. Look backwards for whitespace
    // 3. Whitespace found?
    //  n) failed++
    //     i = limit + limit*failed
    //     step 2.
    //  y) step 4.
    // 4. failed = 0
    // 5. terminate text at i found
    // 6. print text
    // 7. restore text[i]
    // 8. step 2. until i >= text_len
    // 9. print remaining part of the string

    while (i < text_len && ly - y < limit_y)
    {
        // search backwards for whitespace
        while (i > offset && text[i] != L' ')
            i--;

        // retry with bigger limit
        if (i == offset)
        {
            offset = i;
            failed++;
            i += limit_x + failed * limit_x;
            continue;
        }
        else
        {
            failed = 0;
        }

        t = text[i];
        text[i] = 0;
        tb_printf(x, ly, fg, bg, "%ls", text + offset);
        text[i] = t;

        i++; // after the space
        ly++;

        offset = i;
        i += limit_x;
    }
    if ((u32)ly <= limit_y)
    {
        tb_printf(x, ly, fg, bg, "%ls", text + offset);
        ly++;
    }

    return ly - y;
}

// home screen, the first screen the user sees
// it displays a prompt with the user input of input_len wide characters
// and the received messages from msgsArena
void
screen_home(Arena* msgsArena, u32 nmessages, Arena* clientsArena, struct pollfd* fds, u32 input[], u32 input_len)
{
    // config options
    const s32 box_max_len = 80;
    const s32 box_x = 0, box_y = global.height - 3, box_pad_x = 1, box_mar_x = 1, box_bwith = 1, box_height = 3;
    const u32 prompt_x = box_x + box_pad_x + box_mar_x + box_bwith + input_len;

    // the minimum height required is the hight for the box prompt
    // the minimum width required is that one character should fit in the box prompt
    if (global.height < box_height ||
        global.width < (box_x + box_mar_x * 2 + box_pad_x * 2 + box_bwith * 2 + 1))
    {
        // + 1 for cursor
        tb_hide_cursor();
        return;
    }
    else
    {
        // show cursor
        // TODO: show cursor as block character instead of using the real cursor
        bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    }

    // Print messages in msgsArena, if there are too many to display, start printing from an offset.
    // Looks like this:
    //  03:24:29 [1234567890ab] hello homes how are
    //  you doing?
    //  03:24:33 [TlasT] I am fine
    {
        u32 free_y = global.height - box_height;
        if (free_y <= 0)
            goto draw_prompt;

        u8* addr = msgsArena->addr;
        assert(addr != 0);
        // on what line to print the current message, used for scrolling
        u32 msg_y = 0;

        u32 offs = (nmessages > free_y) ? nmessages - free_y : 0;
        // skip offs ccount messages
        for (u32 i = 0; i < offs; i++)
        {
            HeaderMessage* header = (HeaderMessage*)addr;
            addr += sizeof(*header);
            switch (header->type)
            {
            case HEADER_TYPE_TEXT:
            {
                TextMessage* message = (TextMessage*)addr;
                addr += TEXTMESSAGE_SIZE;
                addr += message->len * sizeof(*message->text);
                break;
            }
            case HEADER_TYPE_PRESENCE:
                addr += sizeof(PresenceMessage);
                break;
            case HEADER_TYPE_HISTORY:
                addr += sizeof(HistoryMessage);
                break;
            default:
                // unhandled message type
                assert(0);
            }
        }

        // In each case statement advance the addr pointer by the size of the message
        for (u32 i = offs; i < nmessages && msg_y < free_y; i++)
        {
            HeaderMessage* header = (HeaderMessage*)addr;
            addr += sizeof(*header);

            // Get User for message
            User* client;
            switch (header->type)
            {
            case HEADER_TYPE_TEXT:
            case HEADER_TYPE_PRESENCE:
                client = getUserByID(clientsArena, header->id);
                if (!client)
                {
                    loggingf("User not known, requesting from server\n");
                    client = addUserInfo(clientsArena, fds[FDS_BI].fd, header->id);
                }
                assert(client);
                break;
            }

            switch (header->type)
            {
            case HEADER_TYPE_TEXT:
            {
                TextMessage* message = (TextMessage*)addr;

                // Color own messages
                u32 fg = 0;
                if (user.id == header->id)
                {
                    fg = TB_CYAN;
                }
                else
                {
                    fg = TB_MAGENTA;
                }

                // prefix is of format "HH:MM:SS [<author>] ", create it
                u8 pfx[AUTHOR_LEN - 1 + TIMESTAMP_LEN - 1 + 4 + 1] = {0};
                u8 timestamp[TIMESTAMP_LEN];
                formatTimestamp(timestamp, message->timestamp);
                sprintf((char*)pfx, "%s [%s] ", timestamp, client->author);

                msg_y += tb_printf_wrap(0, msg_y, TB_WHITE, 0, (u32*)&message->text, message->len, fg, 0, pfx, global.width, free_y - msg_y);

                u32 message_size = TEXTMESSAGE_SIZE + message->len * sizeof(*message->text);
                addr += message_size;
            } break;
            case HEADER_TYPE_PRESENCE:
            {
                PresenceMessage* message = (PresenceMessage*)addr;
                tb_printf(0, msg_y, 0, 0, " [%s] *%s*", client->author, presenceTypeString(message->type));
                msg_y++;
                addr += sizeof(*message);
            } break;
            case HEADER_TYPE_HISTORY:
            {
                HistoryMessage* message = (HistoryMessage*)addr;
                addr += sizeof(*message);
                // TODO: implement
            } break;
            default:
                tb_printf(0, msg_y, 0, 0, "%s", headerTypeString(header->type));
                msg_y++;
                break;
            }
        }

    draw_prompt:
        // Draw prompt box which is a box made out of
        // should look like this: ╭───────╮
        //                        │ text█ │
        //                        ╰───────╯
        // the text is padded to the left and right by box_pad_x
        // the middle/inner part is opaque
        // TODO: wrapping when the text is bigger & alternated with scrolling when there is not
        // enough space.
        {
            u32 box_len = 0;
            if (global.width >= box_max_len + 2 * box_mar_x)
                box_len = box_max_len;
            else
                box_len = global.width - box_mar_x * 2;

            // +2 for corners and null terminator
            u32 box_up[box_len + 1];
            u32 box_in[box_len + 1];
            u32 box_down[box_len + 1];
            u32 lr = L'─', ur = L'╭', rd = L'╮', dr = L'╰', ru = L'╯', ud = L'│';

            // top bar
            box_up[0] = ur;
            fillstr(box_up + 1, lr, box_len - 1);
            box_up[box_len - 1] = rd;
            box_up[box_len] = 0;
            // inner part
            fillstr(box_in + 1, L' ', box_len - 1);
            box_in[0] = ud;
            box_in[box_len - 1] = ud;
            box_in[box_len] = 0;
            // bottom bar
            box_down[0] = dr;
            fillstr(box_down + 1, lr, box_len - 1);
            box_down[box_len - 1] = ru;
            box_down[box_len] = 0;

            tb_printf(box_x + box_mar_x, box_y, 0, 0, "%ls", box_up);
            tb_printf(box_x + box_mar_x, box_y + 1, 0, 0, "%ls", box_in);
            tb_printf(box_x + box_mar_x, box_y + 2, 0, 0, "%ls", box_down);

            global.cursor_y = box_y + 1;

            // NOTE: wrapping would be better.
            // Scroll the text when it exceeds the prompt's box length
            u32 freesp = box_len - box_pad_x * 2 - box_bwith * 2;
            if (freesp <= 0)
                return;

            if (input_len > freesp)
            {
                u32* text_offs = input + (input_len - freesp);
                tb_printf(box_x + box_mar_x + box_pad_x + box_bwith, box_y + 1, 0, 0, "%ls", text_offs);
                global.cursor_x = box_x + box_pad_x + box_mar_x + box_bwith + freesp;
            }
            else
            {
                global.cursor_x = prompt_x;
                tb_printf(box_x + box_mar_x + box_pad_x + box_bwith, box_y + 1, 0, 0, "%ls", input);
            }
        }

        if (fds[FDS_UNI].fd == -1 || fds[FDS_BI].fd == -1)
        {
            // show error popup
            popup(TB_RED, TB_BLACK, "Server disconnected.");
        }
    }
}

int
main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: chatty <username>\n");
        return 1;
    }

    u32 arg_len = strlen(argv[1]);
    assert(arg_len <= AUTHOR_LEN - 1);
    memcpy(user.author, argv[1], arg_len);
    user.author[arg_len] = '\0';

    s32 err = 0; // error code for functions

    u32 nmessages = 0; // Number of messages in msgsArena
    s32 nrecv = 0;     // number of bytes received

    u32 input[INPUT_LIMIT] = {0}; // input buffer
    u32 ninput = 0;               // number of characters in input

    Arena msgsArena;
    Arena clientsArena;
    ArenaAlloc(&msgsArena, Megabytes(64));   // Messages received & sent
    ArenaAlloc(&clientsArena, Megabytes(1)); // Arena for storing clients

    struct tb_event ev; // event fork keypress & resize
    u8 quit = 0;        // boolean to indicate if we want to quit the main loop
    u8* quitmsg = 0;    // this string will be printed before returning from main

    pthread_t thr_rec; // thread for reconnecting to server when disconnected

#ifdef LOGGING
    logfd = open(LOGFILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
    assert(logfd != -1);
#else
    logfd = 2; // stderr
#endif

    // poopoo C cannot infer type
    struct pollfd fds[FDS_MAX] = {
        {-1, POLLIN, 0}, // FDS_UNI
        {-1, POLLIN, 0}, // FDS_BI
        {-1, POLLIN, 0}, // FDS_TTY
        {-1, POLLIN, 0}, // FDS_RESIZE
    };

    address = (struct sockaddr_in){
        AF_INET,
        htons(PORT),
        {0},
        {0},
    };

#ifdef IMPORT_ID
    // File for storing the user's ID.
    u32 idfile = open(ID_FILE, O_RDWR | O_CREAT, 0600);
    s32 nread = read(idfile, &user.id, sizeof(user.id));
    assert(nread != -1);
#endif
    /* Authentication */
    {
        s32 unifd, bifd;
        unifd = getConnection(&address);
        if (unifd == -1)
        {
            loggingf("errno: %d\n", errno);
            return 1;
        }
        bifd = getConnection(&address);
        if (bifd == -1)
        {
            loggingf("errno: %d\n", errno);
            return 1;
        }
        loggingf("(%d,%d)\n", unifd, bifd);
        if (!authenticate(&user, unifd))
        {
            loggingf("errno: %d\n", errno);
            return 1;
        }
        fds[FDS_UNI].fd = unifd;
        fds[FDS_BI].fd = bifd;
    }

#ifdef IMPORT_ID
    // Save id
    write(idfile, &user.id, sizeof(user.id));
#endif

    loggingf("Got ID: %lu\n", user.id);

    // for wide character printing
    assert(setlocale(LC_ALL, "") != 0);

    // init
    tb_init();
    tb_get_fds(&fds[FDS_TTY].fd, &fds[FDS_RESIZE].fd);

    screen_home(&msgsArena, nmessages, &clientsArena, fds, input, ninput);
    tb_present();

    // main loop
    while (!quit)
    {
        err = poll(fds, FDS_MAX, TIMEOUT_POLL);
        // ignore resize events and use them to redraw the screen
        assert(err != -1 || errno == EINTR);

        tb_clear();

        if (fds[FDS_UNI].revents & POLLIN)
        {
            // got data from server
            HeaderMessage header;
            nrecv = recv(fds[FDS_UNI].fd, &header, sizeof(header), 0);
            assert(nrecv != -1);

            // Server disconnects
            if (nrecv == 0)
            {
                // close diconnected server's socket
                err = close(fds[FDS_UNI].fd);
                assert(err == 0);
                fds[FDS_UNI].fd = -1; // ignore
                // start trying to reconnect in a thread
                err = pthread_create(&thr_rec, 0, &threadReconnect, (void*)fds);
                assert(err == 0);
            }
            else
            {
                if (header.version != PROTOCOL_VERSION)
                {
                    loggingf("Header received does not match version\n");
                    continue;
                }

                void* addr = ArenaPush(&msgsArena, sizeof(header));
                memcpy(addr, &header, sizeof(header));

                // Messages handled from server
                switch (header.type)
                {
                case HEADER_TYPE_TEXT:
                    recvTextMessage(&msgsArena, fds[FDS_UNI].fd);
                    nmessages++;
                    break;
                case HEADER_TYPE_PRESENCE:;
                    PresenceMessage* message = ArenaPush(&msgsArena, sizeof(*message));
                    nrecv = recv(fds[FDS_UNI].fd, message, sizeof(*message), 0);
                    assert(nrecv != -1);
                    assert(nrecv == sizeof(*message));
                    nmessages++;
                    break;
                default:
                    loggingf("Got unhandled message: %s\n", headerTypeString(header.type));
                    break;
                }
            }
        }

        if (fds[FDS_TTY].revents & POLLIN)
        {
            // got a key event
            tb_poll_event(&ev);

            switch (ev.key)
            {
            case TB_KEY_CTRL_W:
                // delete consecutive whitespace
                while (ninput)
                {
                    if (input[ninput - 1] == L' ')
                    {
                        input[ninput - 1] = 0;
                        ninput--;
                        continue;
                    }
                    break;
                }
                // delete until whitespace
                while (ninput)
                {
                    if (input[ninput - 1] == L' ')
                        break;
                    // erase
                    input[ninput - 1] = 0;
                    ninput--;
                }
                break;
            case TB_KEY_CTRL_Z:
            {
                pid_t pid = getpid();
                tb_shutdown();
                kill(pid, SIGSTOP);
                tb_init();
            } break;

            case TB_KEY_CTRL_D:
            case TB_KEY_CTRL_C:
                quit = 1;
                break;
            case TB_KEY_CTRL_M: // send message
                if (ninput == 0)
                    // do not send empty message
                    break;
                if (fds[FDS_UNI].fd == -1)
                    // do not send message to disconnected server
                    break;

                // null terminate
                input[ninput] = 0;
                ninput++;

                // Save header
                HeaderMessage* header = ArenaPush(&msgsArena, sizeof(*header));
                header->version = PROTOCOL_VERSION;
                header->type = HEADER_TYPE_TEXT;
                header->id = user.id;

                // Save message
                TextMessage* sendmsg = ArenaPush(&msgsArena, TEXTMESSAGE_SIZE);
                sendmsg->timestamp = time(0);
                sendmsg->len = ninput;

                u32 text_size = ninput * sizeof(*input);
                ArenaPush(&msgsArena, text_size);
                memcpy(&sendmsg->text, input, text_size);

                sendAnyMessage(fds[FDS_UNI].fd, *header, sendmsg);

                nmessages++;
                // also clear input
            case TB_KEY_CTRL_U: // clear input
                bzero(input, ninput * sizeof(*input));
                ninput = 0;
                break;
            default:
                if (ev.ch == 0)
                    break;

                // TODO: show error
                if (ninput == INPUT_LIMIT - 1) // last byte reserved for \0
                    break;

                // append key to input buffer
                input[ninput] = ev.ch;
                ninput++;
            }
            if (quit)
                break;
        }

        // These are used to redraw the screen from threads
        if (fds[FDS_RESIZE].revents & POLLIN)
        {
            // ignore
            tb_poll_event(&ev);
        }

        screen_home(&msgsArena, nmessages, &clientsArena, fds, input, ninput);

        tb_present();
    }

    tb_shutdown();

    if (quitmsg != 0)
        printf("%s\n", quitmsg);

    return 0;
}
