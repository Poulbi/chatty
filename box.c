#define TB_IMPL
#include "external/termbox2.h"

#include <locale.h>
#include <assert.h>

#define MAX_INPUT_LEN 70
#define DEBUG

#include "ui.h"

u32
DeleteWordBackwards(u32 Pos, wchar_t* Input)
{
    while (Pos && is_whitespace(Input[Pos - 1]))
    {
        Input[--Pos] = 0;
    }

    while (Pos && !is_whitespace(Input[Pos - 1]))
    {
        Input[--Pos] = 0;
    }

    return Pos;
}

typedef struct {
    wchar_t ur, ru, rd, dr, lr, ud;
} box_characters;

void
Box(u32 BoxX, u32 BoxY, u32 Width, u32 Height, box_characters *Chars)
{
    wchar_t ur, ru, rd, dr, lr, ud;
    if (!Chars)
    {
        ur = L'╭';
        ru = L'╯';
        rd = L'╮';
        dr = L'╰';
        lr = L'─';
        ud = L'│';
    }
    else
    {
        ur = Chars->ur;
        ru = Chars->ru;
        rd = Chars->rd;
        dr = Chars->dr;
        lr = Chars->lr;
        ud = Chars->ud;
    }

    Height--;
    Width--;

    tb_printf(BoxX, BoxY, 0, 0, "%lc", ur);
    for (u32 X = 1; X < Width; X++)
    {
        tb_printf(BoxX + X, BoxY, 0, 0, "%lc", lr);
    }
    tb_printf(BoxX + Width, BoxY, 0, 0, "%lc", rd);

    // Draw vertical bars
    for (u32 Y = 1; Y < Height; Y++)
    {
        tb_printf(BoxX, BoxY + Y, 0, 0, "%lc", ud);
        tb_printf(BoxX + Width, BoxY + Y, 0, 0, "%lc", ud);
    }

    tb_printf(BoxX, BoxY + Height, 0, 0, "%lc", dr);
    for (u32 X = 1; X < Width; X++)
    {
        tb_printf(BoxX + X, BoxY + Height, 0, 0, "%lc", lr);
    }
    tb_printf(BoxX + Width, BoxY + Height, 0, 0, "%lc", ru);
}

int
main(void)
{
    assert(setlocale(LC_ALL, ""));
    struct tb_event ev = {0};

    tb_init();

    Box(0, 0, 32, 4, 0);
    tb_present();
    tb_poll_event(&ev);
    tb_shutdown();
    return 1;

    wchar_t Input[MAX_INPUT_LEN] = {0};
    u32 InputIndex = 0;
    u32 InputLen = 0;
    // Input[InputIndex++] = 1;

    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);


    while (ev.key != TB_KEY_CTRL_C)
    {
        tb_clear();

        InputBox(0, 0, 32, 4, Input, InputLen, True);


        if (InputIndex) tb_printf(0, 3, 0, 0, "'%lc'", Input[InputIndex - 1]);
        tb_printf(4, 3, 0, 0, "%d#%d", InputIndex, InputLen);

        tb_present();
        tb_poll_event(&ev);

        if (ev.ch && InputLen < MAX_INPUT_LEN)
        {
            // Add new character to input
            Input[InputIndex++] = ev.ch;
            InputLen++;
            continue;
        }

        switch (ev.key)
        {
            case TB_KEY_BACKSPACE2:
            {
                if (!InputLen) break;
                Input[InputIndex--] = 0;
                InputLen--;
            } break;
            case TB_KEY_CTRL_W:
            {
                while (InputIndex && is_whitespace(Input[InputIndex - 1]))
                {
                    Input[--InputIndex] = 0;
                    InputLen--;
                }
                while (InputIndex && !is_whitespace(Input[InputIndex - 1]))
                {
                    Input[--InputIndex] = 0;
                    InputLen--;
                }

            } break;
            case TB_KEY_CTRL_U:
            {
                InputIndex = InputLen = 0;
                Input[0] = 0;
            }
            case TB_KEY_ARROW_LEFT:
            {
                if (InputIndex) InputIndex--;
            } break;
            case TB_KEY_ARROW_RIGHT:
            {
                if (InputIndex < InputLen) InputIndex++;
            } break;
        }

    }

    tb_shutdown();
    return 0;
}
