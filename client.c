// Client for chatty

// initial size for the messages array
#define MESSAGES_SIZE 5

// clang-format off
#define TB_IMPL
#include "termbox2.h"
// clang-format on
#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

enum { FD_SERVER = 0,
       FD_TTY,
       FD_RESIZE,
       FD_MAX };

// offset of the input prompt
int curs_offs_x   = 2;
int prompt_offs_y = 3;

// filedescriptor for server
static int serverfd;
// Input message to be send
struct message input = {
    .author    = USERNAME,
    .timestamp = {0},
    .len       = 0,
};
// current amount of messages
int nmessages = 0;
// length of messages array
int messages_size = MESSAGES_SIZE;
// All messages sent and received in order
struct message messages[MESSAGES_SIZE] = {0};
// incremented each time a new message is printed
int msg_y = 0;

// Cleans up resources, should called before exiting.
void cleanup(void);
// Displays an error message msg, followed by the errno variable and exits exeuction.
void err_exit(const char *msg);
// Display the welcome ui screen containing the prompt and messages array.
void scren_welcome(void);
// Append msg to the messages array.  Returns -1 if there was no space in the messages array
// otherwise returns 0 on success.
u8 message_add(struct message msg);

void cleanup(void)
{
    tb_shutdown();
    if (serverfd)
        if (close(serverfd))
            writef("Error while closing server socket. errno: %d\n", errno);
}

// panic
void err_exit(const char *msg)
{
    cleanup();
    writef("%s errno: %d\n", msg, errno);
    _exit(1);
}

void screen_welcome(void)
{
    tb_set_cursor(curs_offs_x, global.height - prompt_offs_y);
    tb_print(0, global.height - prompt_offs_y, 0, 0, ">");

    // if there is not enough space to fit all messages, skip the n first messages of the array.
    int skip            = 0;
    int lines_available = global.height - prompt_offs_y - 1; // pad by 1 from prompt
    if (lines_available - nmessages < 0)
        skip = nmessages - lines_available;
    for (msg_y = skip; msg_y < nmessages; msg_y++) {
        tb_printf(0, msg_y - skip, 0, 0, "%s [%s]: %s", messages[msg_y].timestamp, messages[msg_y].author, messages[msg_y].text);
    }
}

u8 message_add(struct message msg)
{
    if (nmessages == messages_size) {
        return -1;
    }

    int i;
    messages[nmessages].text = input.text;
        ;
    messages[nmessages].text[input.len] = 0;
    messages[nmessages].len     = input.len;
    for (i = 0; (messages[nmessages].timestamp[i] = msg.timestamp[i]); i++)
        ;
    messages[nmessages].timestamp[i] = 0;
    for (i = 0; (messages[nmessages].author[i] = msg.author[i]); i++)
        ;

    nmessages++;
    msg_y++;

    return 0;
}

int main(void)
{
    // current event
    struct tb_event ev;
    // time for a new entered message
    time_t now;
    // localtime of new sent message
    struct tm *ltime;
    char buf[MESSAGE_MAX];
    input.text = buf;

    int serverfd, ttyfd, resizefd;
    struct message msg_recv          = {0};
    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    tb_init();
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);

    screen_welcome();
    tb_present();

    tb_get_fds(&ttyfd, &resizefd);
    serverfd = socket(AF_INET, SOCK_STREAM, 0);

    struct pollfd fds[FD_MAX] = {
        {serverfd, POLLIN, 0}, // FD_SERVER
        {   ttyfd, POLLIN, 0}, // FD_TTY
        {resizefd, POLLIN, 0}, // FD_RESIZE
    };

    if (connect(serverfd, (struct sockaddr *)&address, sizeof(address)))
        err_exit("Error while connecting.");

    for (;;) {
        if (poll(fds, FD_MAX, 50000) == -1) {
            // check if it was a resize event that interrupted the system call
            if (errno == EINTR) {
                tb_peek_event(&ev, 80);
                if (ev.type != TB_EVENT_RESIZE)
                    err_exit("Error while polling.");
                else {
                    tb_clear();
                    screen_welcome();
                }
            }
        }

        if (fds[FD_TTY].revents & POLLIN) {
            tb_poll_event(&ev);
            switch (ev.key) {
            // exit
            case TB_KEY_CTRL_C:
            case TB_KEY_CTRL_D:
            case TB_KEY_ESC:
                goto exit_loop;
            // remove line till cursor
            case TB_KEY_CTRL_U:
                while (global.cursor_x > curs_offs_x) {
                    global.cursor_x--;
                    tb_print(global.cursor_x, global.cursor_y, 0, 0, " ");
                }
                tb_set_cursor(curs_offs_x, global.cursor_y);
                input.len = 0;
                break;
            // send message
            case TB_KEY_CTRL_M:
                if (input.len <= 0)
                    break;
                while (global.cursor_x > curs_offs_x) {
                    global.cursor_x--;
                    tb_print(global.cursor_x, global.cursor_y, 0, 0, " ");
                }
                tb_set_cursor(curs_offs_x, global.cursor_y);

                // zero terminate
                input.text[input.len] = 0;

                // print new message
                time(&now);
                ltime = localtime(&now);
                strftime(input.timestamp, sizeof(input.timestamp), "%H:%M:%S", ltime);

                message_add(input);

                if (send(serverfd, &input, sizeof(input), 0) == -1)
                    err_exit("Error while sending message.");

                // reset buffer
                input.len = 0;

                // update the screen
                // NOTE: kind of wasteful cause we should only display new message
                tb_clear();
                screen_welcome();

                break;
            // remove word
            case TB_KEY_CTRL_W:
                // Delete consecutive space
                while (input.text[input.len - 1] == ' ' && global.cursor_x > curs_offs_x) {
                    global.cursor_x--;
                    input.len--;
                    tb_print(global.cursor_x, global.cursor_y, 0, 0, " ");
                }
                // Delete until next non-space
                while (input.text[input.len - 1] != ' ' && global.cursor_x > curs_offs_x) {
                    global.cursor_x--;
                    input.len--;
                    tb_print(global.cursor_x, global.cursor_y, 0, 0, " ");
                }
                input.text[input.len] = 0;
                break;
            }

            // append pressed character to input.text
            // TODO: wrap instead, allocate more ram for the message instead
            if (ev.ch > 0 && input.len < MESSAGE_MAX && input.len < global.width - 3 - 1) {
                tb_printf(global.cursor_x, global.cursor_y, 0, 0, "%c", ev.ch);
                global.cursor_x++;

                input.text[input.len++] = ev.ch;
            }

        } else if (fds[FD_SERVER].revents & POLLIN) {
            int nrecv = recv(serverfd, &msg_recv, sizeof(struct message), 0);

            if (nrecv == 0) {
                // Server closed
                // TODO: error message like (disconnected)
                break;
            } else if (nrecv == -1) {
                err_exit("Error while receiveiving from server.");
            }
            message_add(msg_recv);
            tb_clear();
            screen_welcome();

        } else if (fds[FD_RESIZE].revents & POLLIN) {
            tb_poll_event(&ev);
            if (ev.type == TB_EVENT_RESIZE) {
                tb_clear();
                screen_welcome();
            }
        }

        tb_present();
    }
exit_loop:;

    cleanup();
    return 0;
}
