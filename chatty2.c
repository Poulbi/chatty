/* Macro's */

#define TB_IMPL
#include "external/termbox2.h"
#undef TB_IMPL

#define DEBUG
#define MAX_INPUT_LEN 255

#define CHATTY_IMPL
#include "ui.h"
#undef CHATTY_IMPL
#include "protocol.h"

#include <locale.h>

#ifdef DEBUG
#define Assert(expr) \
    if (!(expr)) \
    { \
        tb_shutdown(); \
        raise(SIGTRAP); \
    }
#else
#define Assert(expr) ;
#endif

int
main(int Argc, char *Argv[])
{
    struct tb_event ev;
    rect TextBox = {0, 0, 24, 4};
    rect TextR = {
        TextBox.X + TEXTBOX_BORDER_WIDTH + TEXTBOX_PADDING_X, 
        TextBox.Y + TEXTBOX_BORDER_WIDTH,
        TextBox.W - TEXTBOX_BORDER_WIDTH * 2 - TEXTBOX_PADDING_X * 2, 
        TextBox.H - TEXTBOX_BORDER_WIDTH * 2
    };
    wchar_t Input[MAX_INPUT_LEN] = {0};
    u32 InputLen = 0;
    u32 InputOffset = 0;
    u32 InputPos = 0;
    u32 DidParseKey = 0;

    Assert(setlocale(LC_ALL, ""));
    tb_init();
    global.cursor_x = TextR.X;
    global.cursor_y = TextR.Y;
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);

    while (ev.key != TB_KEY_CTRL_C)
    {
        tb_clear();

        DrawBox(TextBox, 0);
        TextBoxDraw(TextR, Input + InputOffset, InputLen);

        InputPos = InputOffset + (global.cursor_x - TextR.X) + (global.cursor_y - TextR.Y) * TextR.W; 
        Assert(InputPos <= InputLen);

        tb_present();

        tb_poll_event(&ev);

        // TODO: Handle resize event

        // Intercept keys
        if (ev.key == TB_KEY_CTRL_M)
        {
            tb_printf(26, 0, 0, 0, "sent.");
            continue;
        }
        else
        {
            DidParseKey = TextBoxKeypress(ev, TextR,
                                          Input, &InputLen, InputPos, &InputOffset);
        }

        u32 ShouldInsert = (!DidParseKey) && (ev.ch && InputLen < MAX_INPUT_LEN);
        if (ShouldInsert)
        {
            TextBoxInsert(Input, InputPos, InputLen++, ev.ch);
            ScrollRight(TextR, &InputOffset);
        }

        // tb_clear();
    }

    tb_shutdown();
}
