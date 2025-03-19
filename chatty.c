#define TB_IMPL
#include "external/termbox2.h"
#undef TB_IMPL

#include <arpa/inet.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define TIMEOUT_POLL 60 * 1000
// time to reconnect in seconds
#define TIMEOUT_RECONNECT 1
#define MAX_INPUT_LEN 512
// Filepath where user ID is stored
#define ID_FILE "_id"
// Filepath where logged
#define LOGFILE "chatty.log"
// enable logging
#define LOGGING
// Number of spaces inserted when pressing Tab/Ctrl+I
#define TAB_WIDTH 4

#ifndef Assert
#ifdef DEBUG
#define Assert(expr) if (!(expr)) \
    { \
        tb_shutdown(); \
        raise(SIGTRAP); \
    }
#else
#define Assert(expr) ;
#endif // DEBUG
#endif // Assert

#define CHATTY_IMPL
#include "chatty.h"
#include "protocol.h"

#define TEXTBOX_MAX_INPUT MAX_INPUT_LEN
#include "ui.h"

#define ARENA_IMPL
#include "arena.h"

enum { FDS_BI = 0, // for one-way communication with the server (eg. TextMessage)
       FDS_UNI,      // For two-way communication with the server (eg. IDMessage)
       FDS_TTY,
       FDS_RESIZE,
       FDS_MAX };

typedef struct {
    u8 Author[AUTHOR_LEN];
    ID ID;
} User;
#define USER_FMT "[%s](%lu)"
#define USER_ARG(client) client.Author, client.ID

typedef struct { 
    s32 NumRead;
    u32 Error;
} command_output;

// User used by chatty
global_variable User user = {0};
// Address of chatty server
global_variable struct sockaddr_in address;

// fill str array with char
void
fillstr(u32* Str, u32 ch, u32 Len)
{
    for (u32 i = 0; i < Len; i++)
        Str[i] = ch;
}

// Centered popup displaying message in the appropriate cololrs
void
popup(u32 fg, u32 bg, u8* text)
{
    u32 len = strlen((char*)text);
    Assert(len > 0);
    tb_print(global.width / 2 - len / 2, global.height / 2, fg, bg, (char*)text);
}

// Returns client in clientsArena matching id
// Returns user if the id was the user's ID
// Returns 0 if nothing was found
User*
get_user_by_id(Arena* clientsArena, ID id)
{
    // User is not in the clientsArena
    if (id == user.ID) return &user;

    User* clients = clientsArena->addr;
    for (u64 i = 0; i < (clientsArena->pos / sizeof(*clients)); i++)
    {
        if (clients[i].ID == id)
            return clients + i;
    }
    return 0;
}

// Request information of client from fd byd id and add it to clientsArena
// Returns pointer to added client
User*
add_user_info(Arena* clientsArena, s32 fd, u64 id)
{
    // Request information about ID
    HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
    header.id = user.ID;
    IDMessage message = {id};
    s32 nsend = sendAnyMessage(fd, header, &message);
    Assert(nsend != -1);

    // Wait for response
    IntroductionMessage introduction_message;
    recvAnyMessageType(fd, &header, &introduction_message, HEADER_TYPE_INTRODUCTION);

    // Add the information
    User* client = ArenaPush(clientsArena, sizeof(*client));
    memcpy(client->Author, introduction_message.author, AUTHOR_LEN);
    client->ID = id;

    LoggingF("Got " USER_FMT "\n", USER_ARG((*client)));
    return client;
}

// Tries to connect to address and populates resulting file descriptors in ConnectionResult.
s32
get_connection(struct sockaddr_in* address)
{
    s32 fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) return -1;

    s32 err = connect(fd, (struct sockaddr*)address, sizeof(*address));
    if (err) return -1;

    return fd;
}

// Authenticates a file descriptor with either the user's id if non-zero or 
// it's information if id is zero.
// Returns 0 if an error occurred.  Non-zero on success.
u32
authenticate(User* user, s32 fd)
{
    /* Scenario 1: Already have an ID */
    if (user->ID)
    {
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_ID);
        IDMessage message = {user->ID};
        s32 nsend = sendAnyMessage(fd, header, &message);
        Assert(nsend != -1);

        ErrorMessage error_message;
        s32 nrecv = recvAnyMessageType(fd, &header, &error_message, HEADER_TYPE_ERROR);
        Assert(nrecv != -1);
        // TODO: handle not found
        if (nrecv == 0)
            return 0;

        if (error_message.type == ERROR_TYPE_SUCCESS)
            return 1;
        else
            return 0;
    }
    /* Scenario 2: No ID, request one from server */
    else
    {
        HeaderMessage header = HEADER_INIT(HEADER_TYPE_INTRODUCTION);
        IntroductionMessage message;
        memcpy(message.author, user->Author, AUTHOR_LEN);
        s32 nsend = sendAnyMessage(fd, header, &message);
        Assert(nsend != -1);

        IDMessage id_message;
        s32 nrecv = recvAnyMessageType(fd, &header, &id_message, HEADER_TYPE_ID);
        Assert(nrecv != -1);
        user->ID = id_message.id;
        return 1;
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
thread_reconnect(void* fds_ptr)
{
    s32 unifd, bifd;
    struct pollfd* fds = fds_ptr;
    struct timespec t = { 0, Miliseconds(300) }; // 300 miliseconds
    LoggingF("Trying to reconnect\n");
    while (1)
    {
        // timeout
        nanosleep(&t, &t);

        bifd = get_connection(&address);
        if (bifd == -1)
        {
            LoggingF("errno: %d\n", errno);
            continue;
        }
        unifd = get_connection(&address);
        if (unifd == -1)
        {
            LoggingF("errno: %d\n", errno);
            close(bifd);
            continue;
        }

        LoggingF("Reconnect succeeded (%d, %d), authenticating\n", unifd, bifd);

        if (authenticate(&user, bifd) &&
            authenticate(&user, unifd))
        {
            break;
        }

        close(bifd);
        close(unifd);

        LoggingF("Failed, retrying...\n");
    }

    fds[FDS_BI].fd = bifd;
    fds[FDS_UNI].fd = unifd;

    // Redraw screen
    raise(SIGWINCH);

    return 0;
}

command_output
run_command_get_output(char *Command, char *Argv[], u8 *OutputBuffer, int Len)
{
    command_output Result = {0};

    int CommandPipe[2];
    int Error = pipe(CommandPipe);
    Assert(Error != -1);

    int Pid = fork();
    Assert(Pid != -1);

    // Run command in child
    if (!Pid)
    {
        dup2(CommandPipe[1], STDOUT_FILENO); //redirect stdout to Pipe
        close(CommandPipe[0]);
        close(CommandPipe[1]);

        int fd = open("/dev/null", O_WRONLY);
        dup2(fd, STDERR_FILENO);

        execvp(Command, Argv);
    }

    // Wait for child
    int statval;
    waitpid(Pid, &statval, 0);

    if(WIFEXITED(statval))
    {
        int ExitCode = WEXITSTATUS(statval);
        if (ExitCode)
        {
            Result.Error = ExitCode;
        }
    }
    else
    {
        Result.Error = 1;
        return Result;
    }

    close(CommandPipe[1]);

    Result.NumRead = read(CommandPipe[0], OutputBuffer, Len);
    Assert(Result.NumRead != -1);

    return Result;
}

// home screen, the first screen the user sees
// it displays a prompt with the user input of input_len wide characters
// and the received messages from msgsArena
void
DisplayChat(Arena* ScratchArena,
            Arena* MessagesArena, u32 MessagesNum,
            Arena* ClientsArena, struct pollfd* fds,
            wchar_t Input[], u32 InputLen)
{
    rect TextBox = {
        1, 0, global.width - 2, 3,
    };
    u32 FreeHeight = global.height - TextBox.H;
    TextBox.Y = FreeHeight;

#define MIN_TEXT_WIDTH_FOR_WRAPPING 20
    s32 MinBoxWidth = TEXTBOX_MIN_WIDTH;
    s32 InputBoxTextWidth = TextBox.W - MinBoxWidth + 2;
    bool ShouldIncreaseSize = (
        (s32)InputLen >= InputBoxTextWidth &&
        InputBoxTextWidth > MIN_TEXT_WIDTH_FOR_WRAPPING
    );
    if (ShouldIncreaseSize)
    {
        TextBox.H++;
    }
#undef MIN_TEXT_WIDTH_FOR_WRAPPING

    rect TextR = {
        TextBox.X + 2, TextBox.Y + 1,
        TextBox.W - 2*TEXTBOX_PADDING_X - 2*TEXTBOX_BORDER_WIDTH,
        TextBox.H - 2*TEXTBOX_BORDER_WIDTH
    };

    if (global.height < TextBox.H || global.width < TextBox.W)
    {
        tb_hide_cursor();
        return;
    }

    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    global.cursor_x = TextR.X;
    global.cursor_y = TextR.Y;
    DrawBox(TextBox, 0);

    // InputBox(TextBox, Input, InputLen, True);

    // Print vertical bar
    s32 VerticalBarOffset = TIMESTAMP_LEN + AUTHOR_LEN + 2;
    for (u32 Y = 0; Y < FreeHeight; Y++)
        tb_print(VerticalBarOffset, Y, 0, 0, "│");

    // show error popup if server disconnected
    if (fds[FDS_UNI].fd == -1 || fds[FDS_BI].fd == -1)
    {
        popup(TB_RED, TB_BLACK, (u8*)"Server disconnected.");
    }

    // Print messages in msgsArena, if there are too many to display, start printing from an offset.
    // Looks like this:
    //  03:24:29 [1234567890ab] hello homes how are
    //  you doing?
    //  03:24:33 [TlasT]     │ I am fine
    //  03:24:33 [Fin]       │ I am too
    {

        // If there is not enough space to draw, do not draw
        if (FreeHeight <= 0) return;

        // Used to go to the next message in MessagesArena by incrementing with the messages' size.
        u8* MessageAddress = MessagesArena->addr;
        Assert(MessageAddress != 0);

        // Skip messages if there is not enough space to display them all
        u32 MessagesOffset = (MessagesNum > FreeHeight) ? MessagesNum - FreeHeight : 0;
        for (u32 MessageIndex = 0; MessageIndex < MessagesOffset; MessageIndex++)
        {
            HeaderMessage* header = (HeaderMessage*)MessageAddress;
            MessageAddress += sizeof(*header);

            switch (header->type)
            {
            case HEADER_TYPE_TEXT:
            {
                TextMessage* message = (TextMessage*)MessageAddress;
                MessageAddress += TEXTMESSAGE_SIZE;
                MessageAddress += message->len * sizeof(*message->text);
                break;
            }
            case HEADER_TYPE_PRESENCE:
                MessageAddress += sizeof(PresenceMessage);
                break;
            case HEADER_TYPE_HISTORY:
                MessageAddress += sizeof(HistoryMessage);
                break;
            default:
                // unhandled message type
                Assert(0);
            }
        }

        u32 MessageY = 0;

        for (u32 i = MessagesOffset;
            i < MessagesNum;
            i++)
        {
            if (MessageY >= FreeHeight) break;

            HeaderMessage* header = (HeaderMessage*)MessageAddress;
            MessageAddress += sizeof(*header);

            User* client = get_user_by_id(ClientsArena, header->id);
            if (!client)
            {
                LoggingF("User not known, requesting from server\n");
                client = add_user_info(ClientsArena, fds[FDS_BI].fd, header->id);
            }
            Assert(client);

            switch (header->type)
            {
            case HEADER_TYPE_TEXT:
            {
                TextMessage* message = (TextMessage*)MessageAddress;


                // Color own messages
                u32 fg = 0;
                if (user.ID == header->id)
                {
                    fg = TB_CYAN;
                }
                else
                {
                    fg = TB_MAGENTA;
                }

                // prefix is of format "HH:MM:SS [<author>] ", create it
                u8 timestamp[TIMESTAMP_LEN];
                formatTimestamp(timestamp, message->timestamp);

                tb_printf(0, MessageY, TB_WHITE, 0, "%s", timestamp);
                tb_printf(TIMESTAMP_LEN, MessageY, fg, 0, "[%s]", client->Author);

                // Only display when there is enough space
                if (global.width > VerticalBarOffset + 2)
                {
                    raw_result RawText = markdown_to_raw(ScratchArena, (wchar_t*)&message->text, message->len);
                    markdown_formatoptions MDFormat = preprocess_markdown(ScratchArena,
                                                                          (wchar_t*)&message->text,
                                                                          message->len);

                    u32 timesWrapped = tb_print_wrapped_with_markdown(VerticalBarOffset + 2, MessageY, fg, 0,
                            RawText.Text, RawText.Len,
                            global.width, global.height, MDFormat);

                    // Free the memory
                    ScratchArena->pos = 0;

                    MessageY += timesWrapped;
                }
                else
                {
                    // We still displayed the timestamp so we need to increment the Y.
                    MessageY++;
                }

                u32 message_size = TEXTMESSAGE_SIZE + message->len * sizeof(*message->text);
                MessageAddress += message_size;
            } break;
            case HEADER_TYPE_PRESENCE:
            {
                PresenceMessage* message = (PresenceMessage*)MessageAddress;
                tb_printf(TIMESTAMP_LEN, MessageY, TB_MAGENTA, 0, "[%s]", client->Author);

                // Wrap Text in '*'
                u8 *Text  = presenceTypeString(message->type);
                u32 Len = 0;
                while(Text[Len]) Len++;
                u32 FormattedText[Len+2];
                FormattedText[0] = '*';
                FormattedText[Len+1] = '*';
                for (u32 i = 1; i < Len + 1; i++) FormattedText[i] = Text[i-1];

                tb_print_markdown(VerticalBarOffset + 2, MessageY, 0, 0, FormattedText, Len + 2);

                MessageY++;
                MessageAddress += sizeof(*message);
            } break;
            case HEADER_TYPE_HISTORY:
            {
                HistoryMessage* message = (HistoryMessage*)MessageAddress;
                MessageAddress += sizeof(*message);
                // TODO: implement
            } break;
            default:
                tb_printf(0, MessageY, 0, 0, "%s", headerTypeString(header->type));
                MessageY++;
                break;
            }
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
    Assert(arg_len <= AUTHOR_LEN - 1);
    memcpy(user.Author, argv[1], arg_len);
    user.Author[arg_len] = '\0';

    s32 err = 0; // error code for functions

    u32 MessagesNum = 0; // Number of messages in msgsArena
    s32 nrecv = 0;     // number of bytes received

    wchar_t Input[MAX_INPUT_LEN] = {0}; // input buffer
    u32 InputIndex = 0;               // number of characters in input

    Arena ScratchArena;
    Arena MessagesArena;
    Arena ClientsArena;
    ArenaAlloc(&MessagesArena, Megabytes(64));   // Messages received & sent
    ArenaAlloc(&ClientsArena, Megabytes(1)); // Arena for storing clients
    ArenaAlloc(&ScratchArena, Megabytes(1)); // Arena for storing clients

    struct tb_event ev; // event fork keypress & resize
    u8 quit = 0;        // boolean to indicate if we want to quit the main loop
    u8* quitmsg = 0;    // this string will be printed before returning from main

    pthread_t thr_rec; // thread for reconnecting to server when disconnected

#ifdef LOGGING
    LogFD = open(LOGFILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
    Assert(LogFD != -1);
#else
    logfd = 2; // stderr
#endif

    // poopoo C cannot infer type
    struct pollfd fds[FDS_MAX] = {
        {-1, POLLIN, 0}, // FDS_BI
        {-1, POLLIN, 0}, // FDS_UNI
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
    Assert(nread != -1);
#endif
    /* Authentication */
    {
        s32 unifd, bifd;
        bifd = get_connection(&address);
        if (bifd == -1)
        {
            LoggingF("errno: %d\n", errno);
            return 1;
        }
        unifd = get_connection(&address);
        if (unifd == -1)
        {
            LoggingF("errno: %d\n", errno);
            return 1;
        }
        LoggingF("(%d,%d)\n", bifd, unifd);
        if (!authenticate(&user, bifd) ||
            !authenticate(&user, unifd))
        {
            LoggingF("errno: %d\n", errno);
            return 1;
        }
        else
        {
            LoggingF("Authenticated (%d,%d)\n", bifd, unifd);
        }
        fds[FDS_BI].fd = bifd;
        fds[FDS_UNI].fd = unifd;
    }

#ifdef IMPORT_ID
    // Save id
    write(idfile, &user.id, sizeof(user.id));
#endif

    LoggingF("Got ID: %lu\n", user.ID);

    // for wide character printing
    Assert(setlocale(LC_ALL, ""));

    // init
    tb_init();
    tb_get_fds(&fds[FDS_TTY].fd, &fds[FDS_RESIZE].fd);

    DisplayChat(&ScratchArena,
                &MessagesArena, MessagesNum,
                &ClientsArena, fds,
                Input, InputIndex);
    tb_present();

    // main loop
    while (!quit)
    {
        err = poll(fds, FDS_MAX, TIMEOUT_POLL);
        // ignore resize events and use them to redraw the screen
        Assert(err != -1 || errno == EINTR);

        tb_clear();

        if (fds[FDS_UNI].revents & POLLIN)
        {
            // got data from server
            HeaderMessage header;
            nrecv = recv(fds[FDS_UNI].fd, &header, sizeof(header), 0);
            Assert(nrecv != -1);

            // Server disconnects
            if (nrecv == 0)
            {
                // close diconnected server's socket
                err = close(fds[FDS_UNI].fd);
                Assert(err == 0);
                fds[FDS_UNI].fd = -1; // ignore
                // start trying to reconnect in a thread
                err = pthread_create(&thr_rec, 0, &thread_reconnect, (void*)fds);
                Assert(err == 0);
            }
            else
            {
                if (header.version != PROTOCOL_VERSION)
                {
                    LoggingF("Header received does not match version\n");
                    continue;
                }

                void* addr = ArenaPush(&MessagesArena, sizeof(header));
                memcpy(addr, &header, sizeof(header));

                // Messages handled from server
                switch (header.type)
                {
                case HEADER_TYPE_TEXT:
                    recvTextMessage(&MessagesArena, fds[FDS_UNI].fd);
                    MessagesNum++;
                    break;
                case HEADER_TYPE_PRESENCE:;
                    PresenceMessage* message = ArenaPush(&MessagesArena, sizeof(*message));
                    nrecv = recv(fds[FDS_UNI].fd, message, sizeof(*message), 0);
                    Assert(nrecv != -1);
                    Assert(nrecv == sizeof(*message));
                    MessagesNum++;
                    break;
                default:
                    LoggingF("Got unhandled message: %s\n", headerTypeString(header.type));
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
                while (InputIndex)
                {
                    if (Input[InputIndex - 1] == L' ')
                    {
                        Input[InputIndex - 1] = 0;
                        InputIndex--;
                        continue;
                    }
                    break;
                }
                // delete until whitespace
                while (InputIndex)
                {
                    if (Input[InputIndex - 1] == L' ')
                        break;
                    // erase
                    Input[InputIndex - 1] = 0;
                    InputIndex--;
                }
                break;
            case TB_KEY_CTRL_Z:
            {
                pid_t pid = getpid();
                tb_shutdown();
                kill(pid, SIGSTOP);
                tb_init();
            } break;
            case TB_KEY_CTRL_Y: // Paste clipboard contents to input
            {
                u32 OutputBufferLen = MAX_INPUT_LEN - InputIndex;
                if (OutputBufferLen <= 0) break;

                u8 OutputBuffer[OutputBufferLen];

                char *PathName = "xclip";
                char *Argv[] = {PathName, "-o", "-sel", "c", 0};

                command_output Output = run_command_get_output(PathName, Argv, OutputBuffer, OutputBufferLen - 1);
                if (Output.Error) break;

                // Remove trailing whitespace
                int BufferIndex = Output.NumRead - 1;
                while (BufferIndex > 0 &&
                        (OutputBuffer[BufferIndex] == '\n' ||
                         OutputBuffer[BufferIndex] == '\t'))
                {
                    OutputBuffer[BufferIndex] = 0;
                    BufferIndex--;
                }

                // Append to output
                for (s32 BufferIndex = 0; BufferIndex < Output.NumRead; BufferIndex++)
                {
                    // convert u8 to u32
                    u32 ch = OutputBuffer[BufferIndex];
                    Input[InputIndex] = ch;
                    InputIndex++;
                }

            } break;
            case TB_KEY_CTRL_I:
            {
                for (u32 i = 0;
                    i < TAB_WIDTH && InputIndex < MAX_INPUT_LEN - 1;
                    i++)
                {
                    Input[InputIndex] = L' ';
                    InputIndex++;
                }
            } break;
            case TB_KEY_BACKSPACE2:
                if (InputIndex) InputIndex--;
                Input[InputIndex] = 0;
                break;
            case TB_KEY_CTRL_D:
            case TB_KEY_CTRL_C:
                quit = 1;
                break;
            case TB_KEY_CTRL_M: // send message
            {
                raw_result RawText = markdown_to_raw(0, Input, InputIndex);

                if (RawText.Len == 0)
                    // do not send empty message
                    break;
                if (fds[FDS_UNI].fd == -1)
                    // do not send message to disconnected server
                    break;

                // null terminate
                Input[InputIndex] = 0;
                InputIndex++;

                // Save header
                HeaderMessage* header = ArenaPush(&MessagesArena, sizeof(*header));
                header->version = PROTOCOL_VERSION;
                header->type = HEADER_TYPE_TEXT;
                header->id = user.ID;

                // Save message
                TextMessage* sendmsg = ArenaPush(&MessagesArena, TEXTMESSAGE_SIZE);
                sendmsg->timestamp = time(0);
                sendmsg->len = InputIndex;

                u32 text_size = InputIndex * sizeof(*Input);
                ArenaPush(&MessagesArena, text_size);
                memcpy(&sendmsg->text, Input, text_size);

                sendAnyMessage(fds[FDS_UNI].fd, *header, sendmsg);

                MessagesNum++;
                // also clear input
            } // fallthrough
            case TB_KEY_CTRL_U: // clear input
                bzero(Input, InputIndex * sizeof(*Input));
                InputIndex = 0;
                break;
            default:
                if (ev.ch == 0)
                    break;

                // TODO: show error
                if (InputIndex == MAX_INPUT_LEN - 1) // last byte reserved for \0
                    break;

                // append key to input buffer
                Input[InputIndex] = ev.ch;
                InputIndex++;
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

        DisplayChat(&ScratchArena, &MessagesArena, MessagesNum, &ClientsArena, fds, Input, InputIndex);

        tb_present();
    }

    tb_shutdown();

    if (quitmsg != 0)
        printf("%s\n", quitmsg);

    return 0;
}
