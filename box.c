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

Bool
IsInRect(rect Rect, u32 X, u32 Y)
{
    if ((X >= Rect.X && X <= Rect.X + Rect.W) &&
        (Y >= Rect.Y && Y <= Rect.Y + Rect.H)) return True;
    return False;
}

int
main(void)
{
    assert(setlocale(LC_ALL, ""));
    struct tb_event ev = {0};

    tb_init();

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
#undef BOX_PADDING_X
#undef BOX_BORDER_WIDTH
    u32 TextSurface = Text.W * Text.H;

    global.cursor_x = Text.X;
    global.cursor_y = Text.Y;

    u32 TextOffset = 0;
    u32 InputPos = 0;

    while (ev.key != TB_KEY_CTRL_C)
    {

        tb_clear();

        DrawBox(TextBox, 0);

        // Draw the text right of the cursor
        // NOTE: the cursor is assumed to be in the box
        // assert(IsInRect(Text, global.cursor_x, global.cursor_y));
        u32 AtX = Text.X, AtY = Text.Y;
        u32 At = TextOffset;
        while (AtY < Text.Y + Text.H)
        {
            if (At < InputLen) 
            {
                tb_printf(AtX++, AtY, 0, 0, "%lc", Input[At++]);
            }
            else
            {
                // Erase previous text
                tb_printf(AtX++, AtY, 0, 0, " ");
            }

            if (AtX == Text.X + Text.W) { AtY++; AtX = Text.X; }
        }

        // Position in input based on cursor position
        InputPos = TextOffset + (global.cursor_x - Text.X) + (global.cursor_y - Text.Y) * Text.W; 

#ifdef DEBUG
        tb_printf(TextBox.X, TextBox.Y, 0, 0,
                  "%2d,%-2d +%d #%-2d %d/%d",
                  global.cursor_x, global.cursor_y,
                  TextOffset, InputPos,
                  InputLen, MAX_INPUT_LEN);
#endif

        tb_present();

        tb_poll_event(&ev);

        if (ev.mod & TB_MOD_CTRL)
        {
            switch (ev.key)
            {
            // Delete character forwards
            case TB_KEY_CTRL_D:
            {
                if (InputPos == InputLen) break;
                Delete(Input, InputPos);
                InputLen--;
                // Delete(Input, Position)
            } break;
            // Delete Word backwards
            case TB_KEY_CTRL_W:
            {
                u32 At = InputPos;
                // Find character to stop on
                while (At && is_whitespace(Input[At - 1])) At--;
                while (At && !is_whitespace(Input[At - 1])) At--;

                u32 NDelete = InputPos - At;
                memmove(Input + At, Input + InputPos, (InputLen - InputPos) * sizeof(Input[At]));
                InputLen -= NDelete;

                if (NDelete > global.cursor_x)
                {

                }
                global.cursor_x -= NDelete;
                if (global.cursor_x < Text.X) {
                    global.cursor_y--;
                    global.cursor_x += Text.W;
                    if (global.cursor_y < Text.Y)
                    {
                        global.cursor_y = Text.Y + Text.H - 1;
                        TextOffset -= TextSurface;
                    }
                }
                assert(IsInRect(Text, global.cursor_x, global.cursor_y));

            } break;
            // Delete until start of line
            case TB_KEY_CTRL_U:
            {
                // memmove until first character
            } break;
            // Delete until end of input
            case TB_KEY_CTRL_K: break;
            // Move to start
            case TB_KEY_CTRL_A: break;
            // Move to end
            case TB_KEY_CTRL_E: break;
            // Move backwards by one character
            case TB_KEY_CTRL_B: break;
            // Move forwards by one character
            case TB_KEY_CTRL_F: break;
            // Move backwards by word
            case TB_KEY_ARROW_LEFT: { } break;
            // Move forwards by word
            case TB_KEY_ARROW_RIGHT: { } break;
            }

        }

        // Insert new character in Input at InputPos
        else if (ev.ch && InputLen < MAX_INPUT_LEN)
        {
        }
        else
        {
            switch (ev.key)
            {
                // Delete character backwards
                case TB_KEY_BACKSPACE2:
                {
                } break;
                case TB_KEY_ARROW_UP:
                {
                    if (global.cursor_y == Text.Y)
                    {
                        if (TextOffset == 0)
                        {
                            global.cursor_x = Text.X;

                            break;
                        }

                        TextOffset -= TextSurface;
                        global.cursor_y = Text.Y + Text.H - 1;
                    }
                    else
                    {
                        global.cursor_y--;
                    }
                } break;
                case TB_KEY_ARROW_DOWN:
                {
                    if (InputPos + Text.W > InputLen) 
                    {
                        // Put the cursor on the last character
                        global.cursor_x = Text.X + (InputLen - TextOffset) % (Text.W);
                        global.cursor_y = Text.Y + (InputLen - TextOffset) / Text.W;

                        break;
                    }

                    if (global.cursor_y == Text.Y + Text.H - 1)
                    {
                        TextOffset += TextSurface;
                        global.cursor_y = Text.Y;
                    }
                    else
                    {
                        global.cursor_y++;
                    }
                } break;

                // Move character left or scroll
                case TB_KEY_ARROW_LEFT:
                {
                    if (InputPos == 0) break;

                    // If text is on the first character of the box scroll backwards
                    if (global.cursor_x == Text.X &&
                        global.cursor_y == Text.Y)
                    {

                        global.cursor_x = Text.X + Text.W - 1;
                        global.cursor_y = Text.Y + Text.H - 1;

                        // Scroll
                        TextOffset -= TextSurface;

                        break;
                    }

                    global.cursor_x--;
                    if (global.cursor_x < Text.X)
                    {
                        global.cursor_x = Text.X + Text.W - 1;
                        global.cursor_y--;
                    }

                } break;
                // Move character right or scroll
                case TB_KEY_ARROW_RIGHT:
                {
                    if (InputPos == InputLen) break;

                    // If cursor is on the last character scroll forwards
                    if (global.cursor_x == Text.X + Text.W - 1 &&
                        global.cursor_y == Text.Y + Text.H - 1)
                    {
                        global.cursor_x = Text.X;
                        global.cursor_y = Text.Y;

                        // Scroll
                        TextOffset += TextSurface;

                        break;
                    }

                    global.cursor_x++;
                    if (global.cursor_x == Text.X + Text.W)
                    {
                        global.cursor_x = Text.X;
                        global.cursor_y++;
                    }
                } break;
            }
        }

    }

    tb_shutdown();
    return 0;
}
