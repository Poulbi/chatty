#include "chatty.h"

// Format option at a position in raw text, used when iterating to know when to toggle a color
// option.
typedef struct {
    u32 Position;
    u32 Color;
} format_option;

// Array of format options and length of said array
typedef struct {
    format_option* Options;
    u32 Len;
} markdown_formatoptions;

typedef struct {
    u32* Text;
    u32 Len;
} raw_result;

// Return True if ch is whitespace
Bool
is_whitespace(u32 ch)
{
    if (ch == L' ')
        return True;
    return False;
}

// Return True if ch is a supported markdown markup character
// TODO: tilde
Bool
is_markdown(u32 ch)
{
    if (ch == L'_' ||
        ch == L'*')
        return True;
    return False;
}

// Print `Text`, `Len` characters long with markdown
// NOTE: This function has no wrapping support
void
tb_print_markdown(u32 X, u32 Y, u32 fg, u32 bg, u32* Text, u32 Len)
{
    for (u32 ch = 0; ch < Len; ch++)
    {
        if (Text[ch] == L'_')
        {
            if (ch < Len - 1 && Text[ch + 1] == L'_')
            {
                fg ^= TB_UNDERLINE;
                ch++;
            }
            else
            {
                fg ^= TB_ITALIC;
            }
        }
        else if (Text[ch] == L'*')
        {
            if (ch < Len - 1 && Text[ch + 1] == L'*')
            {
                fg ^= TB_BOLD;
                ch++;
            }
            else
            {
                fg ^= TB_ITALIC;
            }
        }
        else
        {
            tb_printf(X, Y, fg, bg, "%lc", Text[ch]);
#ifdef DEBUG
            tb_present();
#endif
            X++;
        }
    }
}

// Print `Text`, `Len` characters long as a string wrapped at `XLimit` width and `YLimit` height.
void
tb_print_wrapped(u32 X, u32 Y, u32 XLimit, u32 YLimit, u32* Text, u32 Len)
{
    // Iterator in text
    assert(XLimit > 0);
    assert(YLimit > 0);
    u32 i = XLimit;

    u32 PrevI = 0;

    // For printing
    u32 t = 0;

    while(i < Len)
    {
        // Search backwards for whitespace
        while (!is_whitespace(Text[i]))
        {
            i--;

            // Failed to find whitespace, break on limit at character
            if (i == PrevI)
            {
                i += XLimit;
                break;
            }
        }

        t = Text[i];
        Text[i] = 0;
        tb_printf(X, Y++, 0, 0, "%ls", Text + PrevI);
#ifdef DEBUG
        tb_present();
#endif

        Text[i] = t;

        if (is_whitespace(Text[i])) i++;

        PrevI = i;
        i += XLimit;

        if (Y >= YLimit - 1)
        {
            break;
        }
    }
    tb_printf(X, Y++, 0, 0, "%ls", Text + PrevI);
}

// Print raw string with markdown format options in `MDFormat`, wrapped at
// `XLimit` and `YLimit`.  The string is offset by `XOffset` and `YOffset`.
// `fg` and `bg` are passed to `tb_printf`.
// `Len` is the length of the string not including a null terminator
// The wrapping algorithm searches for a whitespace backwards and if none are found it wraps at
// `XLimit`.
// This function first builds an array of positions where to wrap and then prints `Text` by
// character using the array in `MDFormat.Options` and `WrapPositions` to know when to act.
// Returns how many times wrapped
u32
tb_print_wrapped_with_markdown(u32 XOffset, u32 YOffset, u32 fg, u32 bg,
                               u32* Text, u32 Len,
                               u32 XLimit, u32 YLimit,
                               markdown_formatoptions MDFormat)
{
    XLimit -= XOffset;
    YLimit -= YOffset;
    assert(YLimit > 0);
    assert(XLimit > 0);

    u32 TextIndex = XLimit;
    u32 PrevTextIndex = 0;

    u32 WrapPositions[Len/XLimit + 1];
    u32 WrapPositionsLen = 0;

    // Get wrap positions
    while (TextIndex < Len)
    {
        while (!is_whitespace(Text[TextIndex]))
        {
            TextIndex--;

            if (TextIndex == PrevTextIndex)
            {
                TextIndex += XLimit;
                break;
            }
        }

        WrapPositions[WrapPositionsLen] = TextIndex;
        WrapPositionsLen++;

        PrevTextIndex = TextIndex;
        TextIndex += XLimit;
    }

    u32 MDFormatOptionsIndex = 0;
    u32 WrapPositionsIndex = 0;
    u32 X = XOffset, Y = YOffset;

    for (u32 TextIndex = 0; TextIndex < Len; TextIndex++)
    {
        if (MDFormatOptionsIndex < MDFormat.Len &&
            TextIndex == MDFormat.Options[MDFormatOptionsIndex].Position)
        {
            fg ^= MDFormat.Options[MDFormatOptionsIndex].Color;
            MDFormatOptionsIndex++;
        }
        if (WrapPositionsIndex < WrapPositionsLen &&
            TextIndex == WrapPositions[WrapPositionsIndex])
        {
            Y++;
            if (Y == YLimit) return WrapPositionsIndex + 1;
            WrapPositionsIndex++;
            X = XOffset;
            if (is_whitespace(Text[TextIndex])) continue;
        }
        tb_printf(X++, Y, fg, bg, "%lc", Text[TextIndex]);
    }
    assert(WrapPositionsIndex == WrapPositionsLen);
    assert(MDFormat.Len == MDFormatOptionsIndex);

    return WrapPositionsLen + 1;
}

// Return string without markdown markup characters using `is_markdown()`
// ScratchArena is used to allocate space for the raw text
// If ScratchArena is null then it will only return then length of the raw string
// Len should be characters + null terminator
// Copies the null terminator as well
raw_result
markdown_to_raw(Arena* ScratchArena, wchar_t* Text, u32 Len)
{
    raw_result Result = {0};
    if (ScratchArena)
    {
        Result.Text = ScratchArena->addr;
    }

    for (u32 i = 0; i < Len; i++)
    {
        if (!is_markdown(Text[i]))
        {
            if (ScratchArena)
            {
                u32* ch = ArenaPush(ScratchArena, sizeof(*ch));
                *ch = Text[i];
            }
            Result.Len++;
        }
    }

    return Result;
}

// Get a string with markdown in it and fill array in makrdown_formtoptions with position and colors
// Use Scratcharena to make allocations on that buffer, The Maximimum space needed is Len, eg. when
// the string is only markup characters.
markdown_formatoptions
preprocess_markdown(Arena* ScratchArena, wchar_t* Text, u32 Len)
{
    markdown_formatoptions Result = {0};
    Result.Options = (format_option*)((u8*)ScratchArena->addr + ScratchArena->pos);

    format_option* FormatOpt;

    // raw char iterator
    u32 rawch = 0;

    for (u32 i = 0; i < Len; i++)
    {
        switch (Text[i])
        {
        case L'_':
        {
            FormatOpt = ArenaPush(ScratchArena, sizeof(*FormatOpt));
            Result.Len++;

            FormatOpt->Position = rawch;
            if (i < Len - 1 && Text[i + 1] == '_')
            {
                FormatOpt->Color = TB_UNDERLINE;
                i++;
            }
            else
            {
                FormatOpt->Color = TB_ITALIC;
            }
        } break;
        case L'*':
        {
            FormatOpt = ArenaPush(ScratchArena, sizeof(*FormatOpt));
            Result.Len++;

            FormatOpt->Position = rawch;
            if (i < Len - 1 && Text[i + 1] == '*')
            {
                FormatOpt->Color = TB_BOLD;
                i++;
            }
            else
            {
                FormatOpt->Color = TB_ITALIC;
            }
        } break;
        default:
        {
            rawch++;
        } break;
        }
    }

    return Result;
}

u32 InputBoxMarginX = 1;
u32 InputBoxPaddingX = 1;
#define INPUT_BOX_BORDER_WIDTH 1
#define INPUT_BOX_MIN_TEXT_WIDTH 1
#define INPUT_BOX_MIN_TEXT_HEIGHT 1

u32
GetInputBoxMinimumWidth()
{
    return InputBoxPaddingX * 2 +
           InputBoxMarginX * 2 +
           INPUT_BOX_BORDER_WIDTH * 2 +
           INPUT_BOX_MIN_TEXT_WIDTH;
}

u32
GetInputBoxMinimumHeight()
{
    return INPUT_BOX_BORDER_WIDTH * 2 +
           INPUT_BOX_MIN_TEXT_HEIGHT;
}

void
InputBox(u32 BoxX, u32 BoxY, u32 BoxWidth, u32 BoxHeight,
              wchar_t *Text, u32 TextLen,
              Bool Focused)
{
    //  ╭───────╮
    // M│P....█P│M
    //  ╰───────╯
    // . -> text
    // █ -> cursor
    // P -> padding (symmetric)
    // M -> margin (symmetric)

    u32 MarginX = InputBoxMarginX;
    u32 PaddingX = InputBoxPaddingX;
    u32 BorderWidth = INPUT_BOX_BORDER_WIDTH;

    // Get 0-based coordinate
    BoxWidth -= 2* MarginX;
    BoxHeight--;

    wchar_t ur = L'╭';
    wchar_t ru = L'╯';
    wchar_t rd = L'╮';
    wchar_t dr = L'╰';
    wchar_t lr = L'─';
    wchar_t ud = L'│';

    // Draw Borders
    {
        // Draw the top bar
        tb_printf(BoxX + MarginX, BoxY, 0, 0, "%lc", ur);
        for (u32 X = 1;
             X < BoxWidth;
             X++)
        {
            tb_printf(BoxX + X + MarginX, BoxY, 0, 0, "%lc", lr);
        }
        tb_printf(BoxX + BoxWidth + MarginX, BoxY, 0, 0, "%lc", rd);

        // Draw vertical bars
        for (u32 Y = 1;
             Y < BoxHeight;
             Y++)
        {
            tb_printf(BoxX + MarginX, BoxY + Y, 0, 0, "%lc", ud);
            tb_printf(BoxX + BoxWidth + MarginX, BoxY + Y, 0, 0, "%lc", ud);
        }

        // Draw the bottom bar
        tb_printf(BoxWidth + MarginX, BoxY + BoxHeight, 0, 0, "%lc", ru);
        for (u32 X = 1;
             X < BoxWidth;
             X++)
        {
            tb_printf(BoxX + X + MarginX, BoxY + BoxHeight, 0, 0, "%lc", lr);
        }
        tb_printf(BoxX + MarginX, BoxY + BoxHeight, 0, 0, "%lc", dr);
    }

    // Draw the text
    u32 TextX = BoxX + MarginX + PaddingX + 1;
    u32 TextY = BoxY + 1;
    u32 TextWidth = BoxWidth - TextX - MarginX + 1;
    u32 TextHeight = BoxHeight - BorderWidth * 2 + 1;
    u32 TextDisplaySize = TextWidth * TextHeight;

    // XOffset and YOffset are needed for setting the cursor position
    u32 XOffset = 0, YOffset = 0;
    u32 TextOffset = 0;

    // If there is more than one line, implement vertical wrapping otherwise scroll the text
    // horizontally.
    if (TextHeight > 1)
    {
        // If there is not enough space to fit the text scroll one line by advancing by textwidth.
        if (TextLen >= TextDisplaySize)
        {
            // TextHeight - 1 : scroll by one line
            TextOffset = (TextLen / TextWidth - (TextHeight - 1))  * TextWidth;
        }

        // Print the text
        while (TextOffset < TextLen)
        {
            for (YOffset = 0;
                 YOffset < TextHeight && TextOffset < TextLen;
                 YOffset++)
            {
                for (XOffset = 0;
                     XOffset < TextWidth && TextOffset < TextLen;
                     XOffset++)
                {
                    tb_printf(TextX + XOffset, TextY + YOffset, 0, 0, "%lc", Text[TextOffset]);
                    TextOffset++;
                }
            }
        }
    }
    else
    {
        // Scrooll the text horizontally
        if (TextLen >= TextDisplaySize)
        {
            TextOffset = TextLen - TextWidth;
            XOffset = TextWidth;
        }
        else
        {
            XOffset = TextLen;
        }
        YOffset = 1;
        tb_printf(TextX, TextY, 0, 0, "%ls", Text + TextOffset);
    }

#ifdef DEBUG
#ifdef MAX_INPUT_LEN
    tb_printf(BoxX + 1, BoxY, 0, 0, "%d/%d [%dx%d] %dx%d %d (%d,%d)+(%d,%d)",
            TextLen, MAX_INPUT_LEN,
            BoxWidth, BoxHeight,
            TextWidth, TextHeight,
            TextOffset,
            TextX, TextY,
            XOffset, YOffset);
#else
    tb_printf(BoxX + 1, BoxY, 0, 0, "%d/%d [%dx%d] %dx%d %d (%d,%d)+(%d,%d)",
            TextLen, TextLen,
            BoxWidth, BoxHeight,
            TextWidth, TextHeight,
            TextOffset,
            TextX, TextY,
            XOffset, YOffset);

#endif
#endif

    // Set the cursor
    if (Focused)
    {
        if (TextLen == 0)
        {
            // When there is no text
            global.cursor_x = TextX;
            global.cursor_y = TextY;
        }
        else if (TextLen % TextWidth == 0 && TextHeight > 1)
        {
            // When at the end of width put the cursor on the next line
            global.cursor_x = TextX;
            global.cursor_y = TextY + YOffset;
        }
        else
        {
            // Put cursor after the text
            // Minus one because of the for loop
            global.cursor_x = (TextX-1) + XOffset + 1;
            global.cursor_y = (TextY-1) + YOffset;
        }
    }
}
