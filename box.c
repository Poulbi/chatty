#define MAX_INPUT_LEN 64
#define DEBUG

#define TB_IMPL
#include "external/termbox2.h"
#undef TB_IMPL

#define CHATTY_IMPL
#include "chatty.h"
#undef CHATTY_IMPL

#include "ui.h"

#include <locale.h>
#include <assert.h>

typedef struct {
    u32 X, Y, W, H;
} rect;

typedef struct {
    wchar_t ur, ru, rd, dr, lr, ud;
} box_characters;

void
DrawBox(rect Rect, box_characters *Chars)
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

    Rect.H--;
    Rect.W--;

    tb_printf(Rect.X, Rect.Y, 0, 0, "%lc", ur);
    for (u32 X = 1; X < Rect.W; X++)
    {
        tb_printf(Rect.X + X, Rect.Y, 0, 0, "%lc", lr);
    }
    tb_printf(Rect.X + Rect.W, Rect.Y, 0, 0, "%lc", rd);

    // Draw vertical bars
    for (u32 Y = 1; Y < Rect.H; Y++)
    {
        tb_printf(Rect.X, Rect.Y + Y, 0, 0, "%lc", ud);
        tb_printf(Rect.X + Rect.W, Rect.Y + Y, 0, 0, "%lc", ud);
    }

    tb_printf(Rect.X, Rect.Y + Rect.H, 0, 0, "%lc", dr);
    for (u32 X = 1; X < Rect.W; X++)
    {
        tb_printf(Rect.X + X, Rect.Y + Rect.H, 0, 0, "%lc", lr);
    }
    tb_printf(Rect.X + Rect.W, Rect.Y + Rect.H, 0, 0, "%lc", ru);
}


void
Delete(wchar_t* Text, u64 Pos)
{
    memmove((u8*)(Text + Pos),
            (u8*)(Text + Pos + 1),
            (MAX_INPUT_LEN - Pos - 1) * sizeof(*Text));
}

int
main(void)
{
    assert(setlocale(LC_ALL, ""));
    struct tb_event ev = {0};

    tb_init();

    u32 InputPos = 0;
    u32 InputLen = 0;

#if 1
    wchar_t Input[MAX_INPUT_LEN] = {0};
    wchar_t *t = L"This is some text that no one would want to know, but you could.";
    u32 Len = wcslen(t);
    for (u32 At = 0; At < Len; At++)
    {
        Input[At] = t[At];
    }
    InputLen = Len;
#endif

    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);

    rect TextBox = {0, 0, 24, 4};

#define BOX_PADDING_X 1
#define BOX_BORDER_WIDTH 1
    rect Text = {
        2, 1,
        TextBox.W - 2*BOX_PADDING_X - 2*BOX_BORDER_WIDTH,
        TextBox.H - 2*BOX_BORDER_WIDTH
    };
    u32 TextSurface = Text.W * Text.H;
#undef BOX_PADDING_X
#undef BOX_BORDER_WIDTH

    DrawBox(TextBox, 0);

    // Draw the text
    u32 XOffset = 0, YOffset = 0;
    for (u32 At = 0;
         (At < InputLen) && (At < TextSurface) ;
         At++)
    {
        tb_printf(Text.X + XOffset,
                  Text.Y + YOffset,
                  0, 0,
                  "%lc", Input[At]);
        XOffset++;
        if (XOffset == Text.W) { YOffset++; XOffset = 0; }
    }

    global.cursor_x = Text.X;
    global.cursor_y = Text.Y;

    while (ev.key != TB_KEY_CTRL_C)
    {
        tb_present();

        tb_poll_event(&ev);

        if (ev.mod & TB_MOD_CTRL)
        {
            switch (ev.key)
            {
            case TB_KEY_CTRL_D:
            {
                if (InputPos == InputLen) break;
                Delete(Input, InputPos);
                Input[MAX_INPUT_LEN - 1] = 0;
                InputLen--;
            } break;
            case TB_KEY_CTRL_W:
            {
                // TODO: this could be one memmove call
                while (InputPos && is_whitespace(Input[InputPos - 1]))
                {
                    InputPos--;
                    InputLen--;
                    Delete(Input, InputPos);
                    Input[MAX_INPUT_LEN - 1] = 0;
                }
                while (InputPos && !is_whitespace(Input[InputPos - 1]))
                {
                    InputPos--;
                    InputLen--;
                    Delete(Input, InputPos);
                    Input[MAX_INPUT_LEN - 1] = 0;
                }

            } break;
            case TB_KEY_CTRL_U:
            {
                if (!InputPos) break;
                memmove(Input,
                        Input + InputPos,
                        (InputLen - InputPos) * sizeof(wchar_t));
                InputLen -= InputPos;
                InputPos = 0;
            } break;
            // Delete until end of input
            case TB_KEY_CTRL_K: InputLen = InputPos; break;
            // Move to start
            case TB_KEY_CTRL_A: InputPos = 0; break;
            // Move to end
            case TB_KEY_CTRL_E: InputPos = InputLen; break;
            // Move backwards by one character
            case TB_KEY_CTRL_B: if (InputPos) InputPos--; break;
            // Move forwards by one character
            case TB_KEY_CTRL_F: if (InputPos < InputLen) InputPos++; break;
            // Move backwards by word
            case TB_KEY_ARROW_LEFT:
            {
                while (InputPos && is_whitespace(Input[--InputPos]));
                while (InputPos && !is_whitespace(Input[--InputPos]));
            } break;
            // Move forwards by word
            case TB_KEY_ARROW_RIGHT:
            {
                while (InputPos < InputLen && is_whitespace(Input[InputPos])) InputPos++;
                while (InputPos < InputLen && !is_whitespace(Input[++InputPos]));
            } break;
            }

        }
        // Insert new character in Input at InputPos
        else if (ev.ch && InputLen < MAX_INPUT_LEN)
        {
            memmove(Input + InputPos + 1,
                    Input + InputPos,
                    (MAX_INPUT_LEN - InputPos - 1) * sizeof(*Input));
            Input[InputPos++] = ev.ch;
            InputLen++;
        }
        else
        {
            switch (ev.key)
            {
                case TB_KEY_BACKSPACE2:
                {
                    if (!InputLen) break;
                    Input[InputPos--] = 0;
                    InputLen--;
                } break;
                case TB_KEY_ARROW_LEFT:
                {
                    if (InputPos) InputPos--;

                    // Text is assumed to start on (Text.X, Text.Y)
                    if (global.cursor_x == Text.X &&
                        global.cursor_y == Text.Y)
                    {
                        if (InputPos == 0) break;

                        global.cursor_x = Text.X + Text.W - 1;
                        global.cursor_y = Text.Y + Text.H - 1;
                        // TODO: scroll
                        break;
                    }

                    global.cursor_x--;
                    if (global.cursor_x < Text.X)
                    {
                        global.cursor_x = Text.X + Text.W - 1;
                        global.cursor_y--;
                    }

                } break;
                case TB_KEY_ARROW_RIGHT:
                {
                    if (InputPos == InputLen) break;

                    InputPos++;

                    if (global.cursor_x == Text.X + Text.W - 1 &&
                        global.cursor_y == Text.Y + Text.H - 1)
                    {

                        global.cursor_x = Text.X;
                        global.cursor_y = Text.Y;

                        break;
                    }
                    // TODO: scroll

                    global.cursor_x++;
                    if (global.cursor_x == Text.X + Text.W)
                    {
                        global.cursor_x = Text.X;
                        global.cursor_y++;
                    }
                } break;
            }
        }

#ifdef DEBUG
            tb_printf(TextBox.X, TextBox.Y, 0, 0,
                      "%2d,%-2d #%-2d %d/%d",
                      global.cursor_x, global.cursor_y,
                      InputPos,
                      InputLen, MAX_INPUT_LEN);
#endif


    }

    tb_shutdown();
    return 0;
}
