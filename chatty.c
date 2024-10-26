#define TB_IMPL
#include "termbox2.h"

#include "arena.h"
#include "common.h"

#include <arpa/inet.h>
#include <assert.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>

#define TIMEOUT_POLL 60 * 1000
// time to reconnect in seconds
#define TIMEOUT_RECONNECT 1
// The input buffer is tied to an arena, INPUT_LEN specifies the intial number of wide characters
// allocated, and the INPUT_GROW specifies by how much the input should grow when it exceeds the
// buffer.
#define INPUT_LEN (256 * sizeof(wchar_t))
#define INPUT_GROW (64 * sizeof(wchar_t))

// must be of AUTHOR_LEN -1
static u8 username[AUTHOR_LEN] = "(null)";
// file descriptros for polling
static struct pollfd *fds = NULL;
// mutex for locking fds when in thread_reconnect()
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

enum { FDS_SERVER = 0,
       FDS_TTY,
       FDS_RESIZE,
       FDS_MAX };

void *thread_reconnect(void *address_ptr);
void fillstr(wchar_t *str, wchar_t ch, u32 len);
void popup(u32 fg, u32 bg, char *text);
u32 tb_printf_wrap(u32 x, u32 y, u32 fg, u32 bg, wchar_t *text, u32 fg_pfx, u32 bg_pfx, char *pfx, s32 limit);
void screen_home(Arena *msgsArena, wchar_t input[]);

int main(int argc, char **argv)
{
    // Use first argument as username
    if (argc > 1) {
        u32 arg_len = strlen(argv[1]);
        assert(arg_len <= AUTHOR_LEN - 1);
        memcpy(username, argv[1], arg_len);
        username[arg_len] = '\0';
    }

    s32 err = 0;                                 // error code for functions
    Arena *msgsArena = ArenaAlloc();             // Messages received & sent
    Arena *msgTextArena = ArenaAlloc();          // Text from received & sent messages
    Arena *bufArena = ArenaAlloc();              // data in buf
    u8 *buf = ArenaPush(bufArena, STREAM_LIMIT); // buffer used for receiving and sending messages
    Message *mbuf = (Message *)buf;              // index for buf as a message
    u32 nrecv = 0;                               // number of bytes received
    u32 recv_len = 0;                            // total length of the received stream
    u32 nsend = 0;                               // number of bytes sent
    Message *recv_msg = NULL;                    // message received pushed on the msgsArena

    Arena *inputArena = ArenaAlloc();                  // data in input
    wchar_t *input = ArenaPush(inputArena, INPUT_LEN); // input buffer
    u32 input_len = 0;                                 // length of the input

    struct tb_event ev; // event fork keypress & resize
    u8 quit = 0;        // boolean to indicate if we want to quit the main loop
    u8 *quitmsg = NULL; // this string will be printed before returning from main

    pthread_t thr_rec; // thread for reconnecting to server when disconnected

    // poopoo C cannot infer type
    fds = (struct pollfd[FDS_MAX]){
        {-1, POLLIN, 0}, // FDS_SERVER
        {-1, POLLIN, 0}, // FDS_TTY
        {-1, POLLIN, 0}, // FDS_RESIZE
    };

    const struct sockaddr_in address = {
        AF_INET,
        htons(PORT),
        {0},
    };

    // Connecting to server
    {
        s32 serverfd;
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        assert(serverfd > 2); // greater than STDERR

        err = connect(serverfd, (struct sockaddr *)&address, sizeof(address));
        if (err != 0) {
            perror("Server");
            return 1;
        }
        fds[FDS_SERVER].fd = serverfd;
    }

    // for wide character printing
    assert(setlocale(LC_ALL, "") != NULL);

    // init
    tb_init();
    tb_get_fds(&fds[FDS_TTY].fd, &fds[FDS_RESIZE].fd);

    screen_home(msgsArena, input);
    tb_present();

    // main loop
    while (!quit) {
        err = poll(fds, FDS_MAX, TIMEOUT_POLL);
        // ignore resize events and use them to redraw the screen
        assert(err != -1 || errno == EINTR);

        tb_clear();

        if (fds[FDS_SERVER].revents & POLLIN) {
            // got data from server
            nrecv = recv(fds[FDS_SERVER].fd, buf, STREAM_LIMIT, 0);
            assert(nrecv != -1);

            // Server disconnects
            if (nrecv == 0) {
                // close diconnected server's socket
                err = close(fds[FDS_SERVER].fd);
                assert(err == 0);
                fds[FDS_SERVER].fd = -1; // ignore
                // start trying to reconnect in a thread
                err = pthread_create(&thr_rec, NULL, &thread_reconnect, (void *)&address);
                assert(err == 0);

            } else {
                recv_msg = ArenaPush(msgsArena, sizeof(*mbuf));
                // copy everything but the text
                memcpy(recv_msg, buf, AUTHOR_LEN + TIMESTAMP_LEN + sizeof(mbuf->text_len));
                // allocate memeory for text
                recv_msg->text = ArenaPush(msgTextArena, mbuf->text_len * sizeof(*mbuf->text));

                // If we did not receive the entire message receive the remaining part
                recv_len = sizeof(*recv_msg) - sizeof(recv_msg->text) + recv_msg->text_len * sizeof(*recv_msg->text);
                if (recv_len > nrecv) {
                    // allocate needed space for buf
                    if (recv_len > bufArena->pos)
                        ArenaPush(bufArena, recv_len - bufArena->pos);

                    // receive remaining bytes
                    u32 nr = recv(fds[FDS_SERVER].fd, buf + nrecv, recv_len - nrecv, 0);
                    assert(nr != -1);
                    nrecv += nr;
                    assert(nrecv == recv_len);
                }

                // copy the text to the allocated space
                memcpy(recv_msg->text, buf + TIMESTAMP_LEN + AUTHOR_LEN + sizeof(recv_msg->text_len), recv_msg->text_len * sizeof(*mbuf->text));
            }
        }

        if (fds[FDS_TTY].revents & POLLIN) {
            // got a key event
            tb_poll_event(&ev);

            switch (ev.key) {
            case TB_KEY_CTRL_W:
                // delete consecutive whitespace
                while (input_len) {
                    if (input[input_len - 1] == L' ') {
                        input[input_len - 1] = 0;
                        input_len--;
                        continue;
                    }
                    break;
                }
                // delete until whitespace
                while (input_len) {
                    if (input[input_len - 1] == L' ')
                        break;
                    // erase
                    input[input_len - 1] = 0;
                    input_len--;
                }
                break;
            case TB_KEY_CTRL_D:
            case TB_KEY_CTRL_C:
                quit = 1;
                break;
            case TB_KEY_CTRL_M: // send message
                if (input_len == 0)
                    // do not send empty message
                    break;
                if (fds[FDS_SERVER].fd == -1)
                    // do not send message to disconnected server
                    break;

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
                nsend = send(fds[FDS_SERVER].fd, buf, MESSAGELENP(mbuf), 0);
                assert(nsend > 0);

            case TB_KEY_CTRL_U: // clear input
                bzero(input, input_len * sizeof(wchar_t));
                input_len = 0;
                break;
            default:
                if (ev.ch == 0)
                    break;

                // append key to input buffer
                input[input_len] = ev.ch;
                input_len++;
                if (input_len * sizeof(*input) == inputArena->pos)
                    ArenaPush(inputArena, INPUT_GROW);
            }
            if (quit)
                break;
        }

        // These are used to redraw the screen from threads
        if (fds[FDS_RESIZE].revents & POLLIN) {
            // ignore
            tb_poll_event(&ev);
        }

        screen_home(msgsArena, input);

        tb_present();
    }

    tb_shutdown();

    if (quitmsg != NULL)
        printf("%s\n", quitmsg);

    ArenaRelease(msgTextArena);
    ArenaRelease(msgsArena);
    ArenaRelease(bufArena);
    ArenaRelease(inputArena);

    return 0;
}

// Takes as paramter `struct sockaddr_in*` and uses it to connect to the server.
// When the server sends a disconnect message this function must be called with the fds struct as
// paramter.  To indicate that the server is offline the fds[FDS_SERVER] is set to -1.  When online
// it is set to a non-zero value.
// Returns NULL.
void *thread_reconnect(void *address_ptr)
{
    u32 serverfd, err;
    struct sockaddr_in *address = address_ptr;

    while (1) {
        serverfd = socket(AF_INET, SOCK_STREAM, 0);
        assert(serverfd > 2); // greater than STDERR
        err = connect(serverfd, (struct sockaddr *)address, sizeof(*address));
        if (err == 0)
            break;
        assert(errno == ECONNREFUSED);
        sleep(TIMEOUT_RECONNECT);
    }

    // if the server would send a disconnect again and the polling catches up there could be two
    // threads accessing fds.
    pthread_mutex_lock(&mutex);
    fds[FDS_SERVER].fd = serverfd;
    pthread_mutex_unlock(&mutex);

    // ask to redraw screen
    raise(SIGWINCH);

    return NULL;
}

// fill str array with char
void fillstr(wchar_t *str, wchar_t ch, u32 len)
{
    for (u32 i = 0; i < len; i++)
        str[i] = ch;
}

// Centered popup displaying message in the appropriate cololrs
void popup(u32 fg, u32 bg, char *text)
{
    u32 len = strlen(text);
    assert(len > 0);
    tb_print(global.width / 2 - len / 2, global.height / 2, fg, bg, text);
}

// Print `text` of text_len` wide characters wrapped to limit.  x, y, fg and
// bg will be passed to the tb_printf() function calls.
// pfx is a string that will be printed first and will not be wrapped on characters like msg->text,
// this is useful when for example: printing messages and wanting to have consistent
// timestamp+author name.
// Returns the number of lines printed.
// TODO: remove text_len and calculate it in the function
// TODO: add y limit
// TODO:(bug) text after pfx is wrapped one too soon
// TODO: text == NULL to know how many lines *would* be printed
// TODO: check if text[i] goes out of bounds
u32 tb_printf_wrap(u32 x, u32 y, u32 fg, u32 bg, wchar_t *text, u32 fg_pfx, u32 bg_pfx, char *pfx, s32 limit)
{
    assert(limit > 0);

    // lines y, incremented after each wrap
    s32 ly = y;
    // character the text is split on
    wchar_t t = 0;
    // index used for searching in string
    s32 i = limit;
    // previous i for windowing through the text
    s32 offset = 0;
    // used when retrying to get a longer limit
    u32 failed = 0;

    u32 text_len = 0;
    while (text[text_len] != 0)
        text_len++;

    // NOTE: We can assume that we need to wrap, therefore print a newline after the prefix string
    if (pfx != NULL) {
        tb_printf(x, ly, fg_pfx, bg_pfx, "%s", pfx);

        // If the text fits on one line print the text and return
        // Otherwise print the text on the next line
        s32 pfx_len = strlen(pfx);
        if (limit > pfx_len + text_len) {
            tb_printf(x + pfx_len, y, fg, bg, "%ls", text);
            return 1;
        } else {
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

    while (i < text_len) {
        // search backwards for whitespace
        while (i > offset && text[i] != L' ')
            i--;

        // retry with bigger limit
        if (i == offset) {
            offset = i;
            failed++;
            i += limit + failed * limit;
            continue;
        } else {
            failed = 0;
        }

        t = text[i];
        text[i] = 0;
        tb_printf(x, ly, fg, bg, "%ls", text + offset);
        text[i] = t;

        i++; // after the space
        ly++;

        offset = i;
        i += limit;
    }
    tb_printf(x, ly, fg, bg, "%ls", text + offset);
    ly++;

    return ly - y;
}

// home screen, the first screen the user sees
// it displays a prompt for user input and the received messages from msgsArena
void screen_home(Arena *msgsArena, wchar_t input[])
{
    // config options
    const u32 box_max_len = 80;
    const u32 box_x = 0, box_y = global.height - 3, box_pad_x = 1, box_mar_x = 1, box_bwith = 1, box_height = 3;
    u32 input_len = 0;
    while (input[input_len] != 0)
        input_len++;
    const u32 prompt_x = box_x + box_pad_x + box_mar_x + box_bwith + input_len;

    // the minimum height required is the hight for the box prompt
    // the minimum width required is that one character should fit in the box prompt
    if (global.height < box_height ||
        global.width < (box_x + box_mar_x * 2 + box_pad_x * 2 + box_bwith * 2 + 1)) {
        // + 1 for cursor
        tb_hide_cursor();
        return;
    } else {
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
        u32 freesp = global.height - box_height;
        if (freesp <= 0)
            goto draw_prompt;

        Message *messages = msgsArena->memory;
        assert(messages != NULL);
        // on what line to print the current message, used for scrolling
        u32 msg_y = 0;

        u32 nmessages = (msgsArena->pos / sizeof(Message));
        u32 offs = (nmessages > freesp) ? nmessages - freesp : 0;

        for (u32 i = offs; i < nmessages; i++) {
            // Color user's own messages
            u32 fg = 0;
            if (strncmp((char *)username, (char *)messages[i].author, AUTHOR_LEN) == 0) {
                fg = TB_CYAN;
            } else {
                fg = TB_MAGENTA;
            }

            u32 ty = 0;
            char pfx[AUTHOR_LEN + TIMESTAMP_LEN - 2 + 5] = {0};
            sprintf(pfx, "%s [%s] ", messages[i].timestamp, messages[i].author);
            ty = tb_printf_wrap(0, msg_y, TB_WHITE, 0, messages[i].text, fg, 0, pfx, global.width);
            msg_y += ty;
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
            wchar_t box_up[box_len + 1];
            wchar_t box_in[box_len + 1];
            wchar_t box_down[box_len + 1];
            wchar_t lr = L'─', ur = L'╭', rd = L'╮', dr = L'╰', ru = L'╯', ud = L'│';

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

            if (input_len > freesp) {
                wchar_t *text_offs = input + (input_len - freesp);
                tb_printf(box_x + box_mar_x + box_pad_x + box_bwith, box_y + 1, 0, 0, "%ls", text_offs);
                global.cursor_x = box_x + box_pad_x + box_mar_x + box_bwith + freesp;
            } else {
                global.cursor_x = prompt_x;
                tb_printf(box_x + box_mar_x + box_pad_x + box_bwith, box_y + 1, 0, 0, "%ls", input);
            }
        }

        if (fds[FDS_SERVER].fd == -1) {
            // show error popup
            popup(TB_RED, TB_BLACK, "Server disconnected.");
        }
    }
}
