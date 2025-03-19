#define TB_IMPL
#include "external/termbox2.h"
#undef TB_IMPL

#define MAX_INPUT_LEN 255
#define DEBUG

#define CHATTY_IMPL
#include "ui.h"

#undef Assert
#define Assert(expr) \
    if (!(expr)) \
    { \
        tb_shutdown(); \
        raise(SIGTRAP); \
    }

void
Right(rect TextR, s32 *TextOffset)
{
    if (global.cursor_x == TextR.X + TextR.W - 1)
    {
        if (global.cursor_y == TextR.Y + TextR.H - 1)
        {
            *TextOffset += TextR.W;
        }
        else
        {
            global.cursor_y++;
        }
        global.cursor_x = TextR.X;
    }
    else
    {
        global.cursor_x++;
    }
}

void
Left(rect TextR, s32 *TextOffset)
{
    if (global.cursor_x == TextR.X)
    {
        if (global.cursor_y == TextR.Y)
        {
            *TextOffset -= TextR.W;
        }
        else
        {
            global.cursor_y--;
        }
        global.cursor_x = TextR.X + TextR.W - 1;
    }
    else
    {
        global.cursor_x--;
    }

}

void
PrintString(s32 X, s32 Y, s32 FG, s32 BG, wchar_t *Text, s32 TextLen)
{
    for (s32 At = 0;
         At < TextLen;
         At++)
    {
#ifdef DEBUG
        tb_set_cell(X + At, Y, Text[At], TB_BLACK, TB_CYAN);
        tb_present();
#else
        tb_set_cell(X + At, Y, Text[At], FG, BG);
#endif
    }
}

int
main(int Argc, char* Argv[])
{
    wchar_t Text[MAX_INPUT_LEN] = {0};
    s32 TextLen = 0;
    s32 TextPos = 0;
    s32 TextOffset = 0;
    rect BoxR = {0, 0, 24, 4};
    rect TextR = TEXTBOXFROMBOX(BoxR);
    struct tb_event ev = {0};

    Assert(setlocale(LC_ALL, ""));
    tb_init();

    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);
    global.cursor_x = TextR.X;
    global.cursor_y = TextR.Y;

#if 0
    {
        wchar_t DummyText[] = L"This is some dummy text m"
                                "eant to be wrapped sever"
                                "al times so I can try ou"
                                "t how scrolling works";
        s32 Len = wcslen(DummyText);
        wcsncpy(Text, DummyText, Len);
        TextLen = Len;
    }
#endif

    while (ev.key != TB_KEY_CTRL_C)
    {
        tb_clear();
        DrawBox(BoxR, 0);

#if 1
        {
            s32 SearchIndex = TextR.W;
            s32 PrevIndex = 0;
            s32 BreakOffset = 0;
            s32 YOffset = 0;

            if (TextLen - TextOffset <= TextR.W)
            {
                PrintString(TextR.X, TextR.Y, 0, 0, Text + TextOffset, TextLen);
            }
            else
            {
                while (SearchIndex < TextLen - TextOffset)
                {
                    while (Text[SearchIndex] != ' ')
                    {
                        SearchIndex--;
                        if (SearchIndex == PrevIndex)
                        {
                            SearchIndex += TextR.W;
                            break;
                        }
                    }

                    BreakOffset = (PrevIndex == SearchIndex) ? TextR.W : SearchIndex - PrevIndex;
                    PrintString(TextR.X, TextR.Y + YOffset, 0, 0,
                                Text + TextOffset + PrevIndex, BreakOffset);

                    YOffset++;
                    if (YOffset == TextR.H)
                    {
                        break;
                    }

                    PrevIndex = SearchIndex;
                    SearchIndex += TextR.W;
                }

                if (YOffset < TextR.H)
                {
                    PrintString(TextR.X, TextR.Y + YOffset, 0, 0,
                                Text + TextOffset + PrevIndex,
                                TextLen - TextOffset - PrevIndex);
                }
            }
            

        }
#endif

        tb_printf(BoxR.X, BoxR.Y, 0, 0,
                  "#%d/%d +%d",
                  TextPos, TextLen,
                  TextOffset);
        tb_printf(BoxR.X, BoxR.Y + BoxR.H, 0, 0,
                  "'%lc'/'%lc'",
                  ev.ch ? ev.ch : L'|', Text[TextPos] ? ev.ch : L'|');

        tb_present();
        tb_poll_event(&ev);
        switch (ev.key)
        {
            case TB_KEY_ARROW_RIGHT:
            {
                if (TextPos == TextLen) break;
                TextPos++;
                Right(TextR, &TextOffset);
                continue;
            } break;
            case TB_KEY_ARROW_LEFT:
            {
                if (TextPos == 0) break;
                TextPos--;
                Left(TextR, &TextOffset);
                continue;
            } break;
            case TB_KEY_CTRL_8:
            {
                if (TextPos == 0) break;
                TextPos--;
                TextLen--;
                memmove(Text + TextPos,
                        Text + TextPos + 1,
                        (MAX_INPUT_LEN - TextPos - 1)*sizeof(*Text));
                Left(TextR, &TextOffset);
                continue;
            } break;
        }

        if (!ev.mod && ev.ch && TextLen < MAX_INPUT_LEN)
        {
            if (TextPos < TextLen)
            {
                memmove(Text + TextPos + 1,
                        Text + TextPos,
                        (MAX_INPUT_LEN - TextPos - 1)*sizeof(*Text));
            }
            Text[TextPos] = ev.ch;
            TextPos++;
            TextLen++;

            // Advance more if wrapping will happen
            if (global.cursor_x == TextR.X + TextR.W - 1 &&
                Text)

            Right(TextR, &TextOffset);
        }
    }

    tb_shutdown();
    return 0;
}
