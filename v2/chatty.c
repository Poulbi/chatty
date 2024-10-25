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
void fillstr(wchar_t *str, wchar_t ch, u32 len)
{
    for (int i = 0; i < len; i++) {
        str[i] = ch;
    }
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
int tb_printf_wrap(u32 x, u32 y, u32 fg, u32 bg, wchar_t *text, s32 text_len, char *pfx, s32 limit)
{
    assert(limit > 0);

    /// Algorithm
    // 1. Advance by limit
    // 2. Look backwards for whitespace
    // 3. split the string at the whitespace
    // 4. print the string
    // 5. restore the string (optional)
    // 6. set the offset
    // 7. repeat step 1. until i > len
    // 8. print remaining part of the string

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

    // NOTE: We can assume that we need to wrap, therefore print a newline after the prefix string
    if (pfx != NULL) {
        tb_printf(x, ly, fg, bg, "%s", pfx);

        s32 pfx_len = strlen(pfx);
        if (limit > pfx_len + text_len) {
            // everything fits on one line
            tb_printf(x, y, fg, bg, "%s%ls", pfx, text);
            return 1;
        } else {
            ly++;
        }
    }

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
void screen_home(Arena *msgsArena, wchar_t input[], u32 input_len)
{
    // config options
    const int box_max_len = 80;
    const int box_min_len = 3;
    const int box_x = 0, box_y = global.height - 3, box_pad_x = 1, box_mar_x = 1, box_bwith = 1, box_height = 3;
    const int prompt_x = box_x + box_pad_x + box_mar_x + box_bwith + input_len;

    // the minimum height required is the hight for the box prompt
    // the minimum width required is that one character should fit in the box prompt
    if (global.height < box_height || global.width < (box_x + box_mar_x * 2 + box_pad_x * 2 + box_bwith * 2 + 1)) {
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
        Message *messages = msgsArena->memory;
        assert(messages != NULL);
        // on what line to print the current message, used for scrolling
        u32 msg_y = 0;

        u32 freesp = global.height - (global.height - box_height);
        if (freesp <= 0)
            goto draw_prompt;

        u32 nmessages = (msgsArena->pos / sizeof(Message));
        u32 offs = (nmessages > freesp) ? nmessages - freesp : 0;

        for (int i = offs; i < nmessages; i++) {
            // Color user's own messages
            u32 fg = 0;
            if (strncmp(username, (char *)messages[i].author, AUTHOR_LEN) == 0) {
                fg = TB_CYAN;
            } else {
                fg = TB_WHITE;
            }

            u32 ty = 0;
            char pfx[AUTHOR_LEN + TIMESTAMP_LEN - 2 + 5] = {0};
            sprintf(pfx, "%s [%s] ", messages[i].timestamp, messages[i].author);
            ty = tb_printf_wrap(0, msg_y, fg, 0, messages[i].text, messages[i].text_len - 1, pfx, global.width);
            msg_y += ty;
        }

        // Draw prompt box which is a box made out of
        // should look like this: ╭───────╮
        //                        │ text█ │
        //                        ╰───────╯
        // the text is padded to the left and right by box_pad_x
        // the middle/inner part is opaque
        // TODO: wrapping when the text is bigger & alternated with scrolling when there is not
        // enough space.
    draw_prompt: {

        int box_len = 0;
        if (global.width >= box_mar_x * 2 + box_min_len) {
            // whole screen, but max out at box_max_len
            box_len = (global.width >= box_max_len + 2) ? box_max_len : global.width - box_mar_x * 2;
        } else {
            box_len = box_mar_x * 2 + box_min_len; // left + right side
        }

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
    }
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
                exit = 1;
                break;
            case TB_KEY_CTRL_M: // send message
                if (input_len == 0)
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
