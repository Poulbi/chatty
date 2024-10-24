#define TB_IMPL
#include "termbox2.h"

#include "arena.h"
#include "common.h"

#include <arpa/inet.h>
#include <assert.h>
#include <locale.h>
#include <poll.h>
#include <sys/socket.h>

#define TIMEOUT_POLL 60 * 1000
// time to reconnect in seconds
#define TIMEOUT_RECONNECT 1

// must be of AUTHOR_LEN -1
static char username[AUTHOR_LEN] = "(null)";

enum { FDS_SERVER = 0,
       FDS_TTY,
       FDS_RESIZE,
       FDS_MAX };

// fill str array with char
void fillstr(u8 *str, u8 ch, u8 len)
{
    for (int i = 0; i < len; i++) {
        str[i] = ch;
    }
}

// home screen, the first screen the user sees
// it displays a prompt for user input and the received messages from msgsArena
void screen_home(Arena *msgsArena, wchar_t input[], u32 input_len)
{
    Message *messages = msgsArena->memory;
    assert(messages != NULL);
    for (int i = 0; i < (msgsArena->pos / sizeof(Message)); i++) {
        // Color user's own messages
        u32 fg = 0;
        if (strncmp(username, (char *)messages[i].author, AUTHOR_LEN) == 0) {
            fg = TB_CYAN;
        } else {
            fg = TB_WHITE;
        }

        // TODO: wrap when exceeding prompt size
        tb_printf(0, i, fg, 0, "%s [%s] %ls", messages[i].timestamp, messages[i].author, messages[i].text);
    }

    int len = global.width * 80 / 100;
    wchar_t su[len + 2];
    wchar_t sd[len + 2];
    wchar_t lr = L'─', ur = L'╭', rd = L'╮', dr = L'╰', ru = L'╯', ud = L'│';
    {
        // top bar for prompt
        su[0] = ur;
        for (int i = 1; i < len; i++) {
            su[i] = lr;
        }
        su[len] = rd;
        su[len + 1] = 0;

        // bottom bar for prompt
        sd[0] = dr;
        for (int i = 1; i < len; i++) {
            sd[i] = lr;
        }
        sd[len] = ru;
        sd[len + 1] = 0;
    }

    tb_printf(1, global.height - 3, 0, 0, "%ls", su);
    tb_printf(1, global.height - 2, 0, 0, "%lc", ud);
    global.cursor_x = 1 + 2 + input_len;
    global.cursor_y = global.height - 2;
    tb_printf(1 + 2, global.height - 2, 0, 0, "%ls", input);
    tb_printf(1 + len, global.height - 2, 0, 0, "%lc", ud);
    tb_printf(1, global.height - 1, 0, 0, "%ls", sd);
}

int main(int argc, char **argv)
{
    // Use first argument as username
    if (argc > 1) {
        u32 arg_len = strlen(argv[1]);
        assert(arg_len <= AUTHOR_LEN - 1);
        memcpy(username, argv[1], arg_len);
        username[arg_len] = '\0';
    }

    s32 err, serverfd, ttyfd, resizefd, nsend;
    setlocale(LC_ALL, ""); /* Fix unicode handling */

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverfd > 2); // greater than STDERR

    err = connect(serverfd, (struct sockaddr *)&address, sizeof(address));
    assert(err == 0);

    tb_init();
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    tb_get_fds(&ttyfd, &resizefd);

    struct pollfd fds[FDS_MAX] = {
        {serverfd, POLLIN, 0},
        {   ttyfd, POLLIN, 0},
        {resizefd, POLLIN, 0},
    };

    Arena *msgsArena = ArenaAlloc();
    // Message *messages = msgsArena->memory; // helper pointer, for indexing memory
    Arena *msgTextArena = ArenaAlloc();
    u32 nrecv = 0;
    // buffer used for receiving and sending messages
    u8 buf[STREAM_BUF] = {0};
    Message *mbuf = (Message *)buf;

    wchar_t input[256] = {0};
    u32 input_len = 0;
    struct tb_event ev;
    char *errmsg = NULL;

    // Display loop
    screen_home(msgsArena, input, input_len);
    tb_present();
    while (1) {
        err = poll(fds, FDS_MAX, TIMEOUT_POLL);
        // ignore resize events because we redraw the whole screen anyways.
        assert(err != -1 || errno == EINTR);

        tb_clear();

        if (fds[FDS_SERVER].revents & POLLIN) {
            // got data from server
            u8 timestamp[TIMESTAMP_LEN];
            message_timestamp(timestamp);

            nrecv = recv(serverfd, buf, STREAM_LIMIT, 0);
            assert(nrecv != -1);
            if (nrecv == 0) {
                // TODO: Handle disconnection, aka wait for server to reconnect.
                // Try to reconnect with 1 second timeout
                // TODO: still listen for events
                // NOTE: we want to display a popup, but still give the user the chance to do
                // ctrl+c
                u8 once = 0;
                while (1) {
                    err = close(serverfd);
                    assert(err == 0);

                    serverfd = socket(AF_INET, SOCK_STREAM, 0);
                    assert(serverfd > 2); // greater than STDERR

                    err = connect(serverfd, (struct sockaddr *)&address, sizeof(address));
                    if (err == 0) {
                        bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
                        tb_clear();
                        break;
                    }
                    assert(errno == ECONNREFUSED);

                    // Popup error message
                    if (!once) {
                        screen_home(msgsArena, input, input_len);
                        tb_hide_cursor();
                        tb_print(global.width / 2 - 10, global.height / 2, TB_RED, TB_BLACK, "Server disconnected!");
                        tb_present();
                        once = 1;
                    }

                    sleep(TIMEOUT_RECONNECT);
                }
            } else {

                Message *buf_msg = (Message *)buf; // helper for indexing memory
                Message *recvmsg = ArenaPush(msgsArena, sizeof(Message));
                // copy everything but the text
                memcpy(recvmsg, buf, AUTHOR_LEN + TIMESTAMP_LEN + sizeof(buf_msg->text_len));
                // allocate memeory for text
                recvmsg->text = ArenaPush(msgTextArena, recvmsg->text_len * sizeof(wchar_t));
                // copy the text to the allocated space
                memcpy(recvmsg->text, buf + TIMESTAMP_LEN + AUTHOR_LEN + sizeof(recvmsg->text_len), recvmsg->text_len * sizeof(wchar_t));
            }
        } else if (fds[FDS_TTY].revents & POLLIN) {
            // got a key event
            tb_poll_event(&ev);

            u8 exit = 0;
            switch (ev.key) {
            case TB_KEY_CTRL_D:
            case TB_KEY_CTRL_C:
                exit = 1;
                break;
            case TB_KEY_CTRL_M: // send message
                // null terminate
                input[input_len] = 0;
                input_len++;
                // TODO: check size does not exceed buffer

                // add to msgsArena
                Message *sendmsg = ArenaPush(msgsArena, sizeof(Message));
                memcpy(sendmsg->author, username, AUTHOR_LEN);
                message_timestamp(sendmsg->timestamp);
                sendmsg->text_len = input_len;
                sendmsg->text = ArenaPush(msgTextArena, input_len * sizeof(wchar_t));
                // copy the text to the allocated space
                memcpy(sendmsg->text, input, input_len * sizeof(wchar_t));

                // Send the message
                // copy everything but the text
                memcpy(buf, sendmsg, AUTHOR_LEN + TIMESTAMP_LEN + sizeof(wchar_t));
                memcpy(&mbuf->text, input, input_len * sizeof(wchar_t));
                nsend = send(serverfd, buf, AUTHOR_LEN + TIMESTAMP_LEN + input_len * sizeof(wchar_t), 0);
                assert(nsend > 0);

            case TB_KEY_CTRL_U: // clear input
                bzero(input, input_len * sizeof(wchar_t));
                input_len = 0;
                break;
            default:
                assert(ev.ch >= 0);
                if (ev.ch == 0)
                    break;
                // append key to input buffer
                // TODO: check size does not exceed buffer
                input[input_len] = ev.ch;
                input_len++;

                break;
            }
            if (exit)
                break;

        } else if (fds[FDS_RESIZE].revents & POLLIN) {
            tb_poll_event(&ev);
        }

        screen_home(msgsArena, input, input_len);

        tb_present();
    }

    tb_shutdown();

    if (errmsg != NULL)
        printf("%s\n", errmsg);

    ArenaRelease(msgTextArena);
    ArenaRelease(msgsArena);

    return 0;
}
