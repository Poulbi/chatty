#define Assert(expr) if (!(expr)) { \
    tb_shutdown(); \
    raise(SIGTRAP); \
}

#define TB_IMPL
#include "termbox2.h"
#undef TB_IMPL

#define TEXTBOX_MAX_INPUT 255

#define UI_IMPL
#include "ui.h"

#define ARENA_IMPL
#include "arena.h"

#include <locale.h>

int
main(void)
{
    struct tb_event ev = {0};

    u32 InputLen = 0;
    wchar_t Input[TEXTBOX_MAX_INPUT] = {0};
    rect TextBox = {0, 0, 32, 5};
    rect TextR = {
        2, 1,
        TextBox.W - 2*TEXTBOX_PADDING_X - 2*TEXTBOX_BORDER_WIDTH,
        TextBox.H - 2*TEXTBOX_BORDER_WIDTH
    };
    // Used for scrolling the text. Text before TextOffset will not be printed.
    u32 TextOffset = 0;
    // Position in input based on cursor position.
    u32 InputPos = 0;

    Assert(setlocale(LC_ALL, ""));
    tb_init();
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    global.cursor_x = TextR.X;
    global.cursor_y = TextR.Y;

    DrawBox(TextBox, 0);

    while (ev.key != TB_KEY_CTRL_C)
    {
        DrawTextBoxWrapped(TextR, Input + TextOffset, InputLen - TextOffset);

        InputPos = TextOffset + (global.cursor_x - TextR.X) + (global.cursor_y - TextR.Y) * TextR.W; 
        Assert(InputPos <= InputLen);

        tb_present();
        tb_poll_event(&ev);

        u32 Ret = TextBoxKeypress(ev, TextR, Input, &InputLen, InputPos, &TextOffset);

        u32 ShouldInsert = (!Ret) && (ev.ch && InputLen < TEXTBOX_MAX_INPUT);
        // Insert new character in Input at InputPos
        if (ShouldInsert)
        {
            TextBoxInsert(Input, InputPos, InputLen++, ev.ch);
            TextBoxScrollRight(TextR, &TextOffset);
        }
    }


    tb_shutdown();
    return 0;
}
