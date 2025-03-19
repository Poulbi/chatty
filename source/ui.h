#ifndef UI_H
#define UI_H

/* Macro's */

#include <stdint.h>
#include <stdbool.h>
#include "termbox2.h"
#include "arena.h"
#include "chatty.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef u32 b32;

/* Types */

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

// Rectangle
typedef struct {
    s32 X, Y, W, H;
} rect;

// Characters to use for drawing a box
// See DrawBox() for an example
typedef struct {
    wchar_t ur, ru, rd, dr, lr, ud;
} box_characters;

/* Functions */

bool IsInRect(rect Rect, s32 X, s32 Y);
bool is_whitespace(u32 ch);
bool is_markdown(u32 ch);
void tb_print_wrapped(u32 X, u32 Y, u32 XLimit, u32 YLimit, u32* Text, u32 Len);
void tb_print_markdown(u32 X, u32 Y, u32 fg, u32 bg, u32* Text, u32 Len);
u32 tb_print_wrapped_with_markdown(u32 XOffset, u32 YOffset, u32 fg, u32 bg,
                                   u32* Text, u32 Len,
                                   u32 XLimit, u32 YLimit,
                                   markdown_formatoptions MDFormat);
raw_result markdown_to_raw(Arena* ScratchArena, wchar_t* Text, u32 Len);
markdown_formatoptions preprocess_markdown(Arena* ScratchArena, wchar_t* Text, u32 Len);

/* Input Box UI */

// assumes TEXTBOX_MAX_INPUT to be set
//
// #define TEXTBOX_MAX_INPUT 128

#define TEXTBOX_PADDING_X 1
#define TEXTBOX_BORDER_WIDTH 1
#define TEXTBOX_MIN_WIDTH TEXTBOX_PADDING_X * 2 + TEXTBOX_BORDER_WIDTH * 2 + 1;
#define TEXTBOXFROMBOX(Box) \
    { \
        .X = Box.X + TEXTBOX_BORDER_WIDTH + TEXTBOX_PADDING_X, \
        .Y = Box.Y + TEXTBOX_BORDER_WIDTH, \
        .W = Box.W - TEXTBOX_BORDER_WIDTH * 2 - TEXTBOX_PADDING_X * 2, \
        .H = Box.H - TEXTBOX_BORDER_WIDTH * 2 \
    }

void DrawBox(rect Rect, box_characters *Chars);
void DrawTextBox(rect TextR, wchar_t *Text, u32 TextLen);
void DrawTextBoxWrapped(rect TextR, wchar_t *Text, u32 TextLen);
void TextBoxScrollLeft(rect Text, u32 *TextOffset);
void TextBoxScrollRight(rect Text, u32 *TextOffset);
void TextBoxDelete(wchar_t* Text, u64 Pos);
void TextBoxInsert(wchar_t *Input, u32 InputPos, u32 InputLen, wchar_t ch);
u32  TextBoxKeypress(struct tb_event ev,
                     rect TextR, wchar_t *Text, u32 *TextLenPtr, u32 TextPos, u32 *TextOffsetPtr);

// Draw box along boundaries in Rect with optional Chars.
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
    for (s32 X = 1; X < Rect.W; X++)
    {
        tb_printf(Rect.X + X, Rect.Y, 0, 0, "%lc", lr);
    }
    tb_printf(Rect.X + Rect.W, Rect.Y, 0, 0, "%lc", rd);

    // Draw vertical bars
    for (s32 Y = 1; Y < Rect.H; Y++)
    {
        tb_printf(Rect.X, Rect.Y + Y, 0, 0, "%lc", ud);
        tb_printf(Rect.X + Rect.W, Rect.Y + Y, 0, 0, "%lc", ud);
    }

    tb_printf(Rect.X, Rect.Y + Rect.H, 0, 0, "%lc", dr);
    for (s32 X = 1; X < Rect.W; X++)
    {
        tb_printf(Rect.X + X, Rect.Y + Rect.H, 0, 0, "%lc", lr);
    }
    tb_printf(Rect.X + Rect.W, Rect.Y + Rect.H, 0, 0, "%lc", ru);
}

// SCROLLING
// ╭──────────╮    ╭──────────╮ Going Left on the first character scrolls up.
// │ █3     4 │ => │ 1     2█ │ Cursor on end of the top line.
// │  5     6 │    │  3     4 │ 
// ╰──────────╯    ╰──────────╯
//
// ╭──────────╮    ╭──────────╮ Going Right on the last character scrolls down.
// │ 1      3 │ => │ 2     4  │ Puts cursor on start of the bottom line.
// │ 2     4█ │    │ █        │ 
// ╰──────────╯    ╰──────────╯
//
// ╭──────────╮    ╭──────────╮ Going Down on bottom line scrolls down.
// │ 1      3 │ => │ 2      4 │ Cursor stays on bottom line.
// │ 2  █   4 │    │    █     │ 
// ╰──────────╯    ╰──────────╯
//
// ╭──────────╮    ╭──────────╮ Going Up on top line scrolls up.
// │ 3  █   4 │ => │ 1  █   2 │ Cursor stays on top line.
// │ 5      6 │    │ 3      5 │ 
// ╰──────────╯    ╰──────────╯
//
// In code this translates to changing global.cursor_{x,y} and TextOffset accordingly.

// Scroll one character to the left
void
TextBoxScrollLeft(rect Text, u32 *TextOffset)
{
    // If text is on the first character of the box scroll up
    if (global.cursor_x == Text.X &&
        global.cursor_y == Text.Y)
    {
        global.cursor_x = Text.X + Text.W - 1;
        global.cursor_y = Text.Y;

        *TextOffset -= Text.W;
    }
    else
    {
        if (global.cursor_x == Text.X)
        {
            // Got to previous line
            global.cursor_x = Text.X + Text.W - 1;
            global.cursor_y--;
        }
        else
        {
            global.cursor_x--;
        }
    }
}

// Scroll one character to the right
void
TextBoxScrollRight(rect Text, u32 *TextOffset)
{
    // If cursor is on the last character scroll forwards
    if (global.cursor_x == Text.X + Text.W - 1 &&
        global.cursor_y == Text.Y + Text.H - 1)
    {
        global.cursor_x = Text.X;
        global.cursor_y = Text.Y + Text.H - 1;

        *TextOffset += Text.W;
    }
    else
    {
        global.cursor_x++;
        if (global.cursor_x == Text.X + Text.W)
        {
            global.cursor_x = Text.X;
            global.cursor_y++;
        }
    }
}


// Delete a character in Text at Pos
void
TextBoxDelete(wchar_t* Text, u64 Pos)
{
    memmove(Text + Pos,
            Text + Pos + 1,
            (TEXTBOX_MAX_INPUT - Pos - 1) * sizeof(*Text));
}

// Insert a ev.ch in Input at InputPos
void
TextBoxInsert(wchar_t *Input, u32 InputPos, u32 InputLen, wchar_t ch)
{
    if (InputPos < InputLen)
    {
        memmove(Input + InputPos,
                Input + InputPos - 1,
                (InputLen - InputPos + 1) * sizeof(*Input));
    }
    Input[InputPos] = ch;
}

// Handle the key event ev changing Text, TextLenPtr, and TextOffsetPtr accordingly.
// InputPos is the position in the Input relating to the cursor position.
// TextR is the bounding box for the text.
//
// Returns non-zero when a key event was handled.
//
// TODO: pass by value and return struct with updated values
u32
TextBoxKeypress(struct tb_event ev, rect TextR,
                wchar_t *Text, u32 *TextLenPtr, u32 TextPos, u32 *TextOffsetPtr)
{
    u32 Result = 1;

    u32 TextLen = *TextLenPtr;
    u32 TextOffset = *TextOffsetPtr;

    switch (ev.key)
    {

    // Delete character backwards
    case TB_KEY_CTRL_8:
    // case TB_KEY_BACKSPACE2:
    {
        if (TextPos == 0) break;

        TextBoxDelete(Text, TextPos - 1);
        TextLen--;

        TextBoxScrollLeft(TextR, &TextOffset);

    } break;

    // Delete character forwards
    case TB_KEY_CTRL_D:
    {
        if (TextPos == TextLen) break;
        TextBoxDelete(Text, TextPos);
        TextLen--;
        // Delete(Text, Position)
    } break;

    // Delete word backwards
    case TB_KEY_CTRL_W:
    {
        u32 At = TextPos;
        // Find character to stop on
        while (At && is_whitespace(Text[At - 1])) At--;
        while (At && !is_whitespace(Text[At - 1])) At--;

        s32 NDelete = TextPos - At;
        memmove(Text + At, Text + TextPos, (TextLen - TextPos) * sizeof(Text[At]));
        TextLen -= NDelete;
#ifdef DEBUG
        Text[TextLen] = 0;
#endif
        // NOTE: this could be calculated at once instead
        while(NDelete--) TextBoxScrollLeft(TextR, &TextOffset);

        Assert(IsInRect(TextR, global.cursor_x, global.cursor_y));

    } break;

    // Delete until start of Text
    case TB_KEY_CTRL_U:
    {
        memmove(Text, Text + TextPos, (TextLen - TextPos) * sizeof(*Text));
        TextLen -= TextPos;
#ifdef DEBUG
        Text[TextLen] = 0;
#endif
        global.cursor_x = TextR.X;
        global.cursor_y = TextR.Y;
        TextOffset = 0;
    } break;

    // Delete until end of Text
    case TB_KEY_CTRL_K:
    {
        TextLen = TextPos;
        Text[TextPos] = 0;
    } break;

    // Move to start of line
    case TB_KEY_CTRL_A: global.cursor_x = TextR.X; break;

    // Move to end of line
    case TB_KEY_CTRL_E:
    {
        if (global.cursor_x == TextR.X + TextR.W - 1) break;

        if (TextPos + TextR.W > TextLen) 
        {
            // Put the cursor on the last character
            global.cursor_x = TextR.X + (TextLen - TextOffset) % TextR.W;
        }
        else
        {
            global.cursor_x = TextR.X + TextR.W - 1;
        }
    } break;

    // Move backwards
    case TB_KEY_CTRL_B: 
    case TB_KEY_ARROW_LEFT:
    {
        // Move forward by word
        if (ev.mod == TB_MOD_CTRL)
        {
            u32 At = TextPos;
            while(At && is_whitespace(Text[At])) At--;
            while(At && !is_whitespace(Text[At])) At--;
            while(TextPos - At++) TextBoxScrollLeft(TextR, &TextOffset);
        }
        // Move forward by character
        else
        {
            if (TextPos == 0) break;
            TextBoxScrollLeft(TextR, &TextOffset);
        }
    } break;

    // Move forwards
    case TB_KEY_CTRL_F:
    case TB_KEY_ARROW_RIGHT:
    {
        // Move forward by word
        if (ev.mod == TB_MOD_CTRL)
        {
            u32 At = TextPos;
            while(At < TextLen && is_whitespace(Text[At])) At++;
            while(At < TextLen && !is_whitespace(Text[At])) At++;
            while(At-- - TextPos) TextBoxScrollRight(TextR, &TextOffset);
        }
        // Move forward by character
        else
        {
            if (TextPos == TextLen) break;
            TextBoxScrollRight(TextR, &TextOffset);
        }
    } break;

    // Move up
    case TB_KEY_CTRL_P:
    case TB_KEY_ARROW_UP:
    {
        if (global.cursor_y == TextR.Y)
        {
            if (TextOffset == 0)
            {
                global.cursor_x = TextR.X;

                break;
            }

            TextOffset -= TextR.W;
            global.cursor_y = TextR.Y;
        }
        else
        {
            global.cursor_y--;
        }
    } break;

    // Move down
    case TB_KEY_CTRL_N:
    case TB_KEY_ARROW_DOWN:
    {
        if (TextPos + TextR.W > TextLen) 
        {
            // Put the cursor on the last character
            global.cursor_x = TextR.X + (TextLen - TextOffset) % (TextR.W);
            global.cursor_y = TextR.Y + (TextLen - TextOffset) / TextR.W;

            // If cursor ended 1 line under the bottom line this means that the text
            // needs to be scrolled.
            if (global.cursor_y == TextR.Y + TextR.H)
            {
                TextOffset += TextR.W;
                global.cursor_y--;
            }

            break;
        }

        if (global.cursor_y == TextR.Y + TextR.H - 1)
        {
            TextOffset += TextR.W;
        }
        else
        {
            global.cursor_y++;
        }
    } break;
    default:
    {
        Result = 0;
    }
    }

    *TextLenPtr = TextLen;
    *TextOffsetPtr = TextOffset;

    return Result;
}

// Draws characters from Text fitting in the TextR rectangle.
// InputLen is the amount of characters in Text.
//
// NOTE: TextR is always filled, when not enough characters in Input it will uses spaces instead.
// This makes it easy to update the textbox by recalling this function.
void
DrawTextBox(rect TextR, wchar_t *Text, u32 TextLen)
{
    // Draw the text right of the cursor
    // NOTE: the cursor is assumed to be in the box
    Assert(IsInRect(TextR, global.cursor_x, global.cursor_y));
    s32 AtX = TextR.X, AtY = TextR.Y;
    u32 At = 0;
    while (AtY < TextR.Y + TextR.H)
    {
        if (At < TextLen) 
        {
            tb_printf(AtX++, AtY, 0, 0, "%lc", Text[At++]);
            global.cursor_x = AtX;
        }
        else
        {
            tb_printf(AtX++, AtY, 0, 0, " ");
        }

        if (AtX == TextR.X + TextR.W)
        {
            AtY++;
            AtX = TextR.X;
            global.cursor_y = AtY - 1;
        }
    }

}

// NOTE: To ensure that the text looks the same even when scrolling it you must provide the whole text,
// wrap the whole text and the only show the portion that can fit in the Text Rectangle.

// When line will exceed width break the word on the next line. This is done by looking backwards
// for whitespace from TextR.W width. When a whitespace is found text is wrapped on the next line.
// TODO: this does not work yet.
void
DrawTextBoxWrapped(rect TextR, wchar_t *Text, u32 TextLen)
{
    if (TextLen <= TextR.W)
    {
        tb_printf(TextR.X, TextR.Y, 0, 0, "%ls", Text);
        tb_present();
        global.cursor_x = TextR.X + TextLen;
        global.cursor_y = TextR.Y;
        return;
    }

    u32 SearchIndex = TextR.W; 
    u32 PrevIndex = 0;
    u32 Y = TextR.Y;

    while (SearchIndex < TextLen)
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

        // Wrap
        wchar_t BreakChar = Text[SearchIndex];
        Text[SearchIndex] = 0;
        tb_printf(TextR.X, Y, 0, 0, "%ls", Text + PrevIndex);
        tb_present();
        Text[SearchIndex] = BreakChar;

        if (Y + 1 == TextR.Y + TextR.H)
        {
            global.cursor_y = Y;
            global.cursor_x = TextR.X + (SearchIndex - PrevIndex);
            return;
        }
        Y++;

        if (BreakChar == L' ')
        {
            SearchIndex++;
        }

        PrevIndex = SearchIndex;
        SearchIndex += TextR.W;
    }

    // This happens when SearchIndex exceeds TextLen but there is still some 
    // text left to print.  We can assume that the text will fit because otherwise it would have
    // been wrapped a second time and the loop would have returned.
    tb_printf(TextR.X, Y, 0, 0, "%ls", Text + PrevIndex);
    // NOTE: this sets the cursor position correctly

    global.cursor_y = Y;
    global.cursor_x = TextR.X + TextLen - PrevIndex;
}

#endif // UI_H

#ifdef UI_IMPL
// Check if coordinate (X,Y) is in rect boundaries
bool
IsInRect(rect Rect, s32 X, s32 Y)
{
    if ((X >= Rect.X && X <= Rect.X + Rect.W) &&
        (Y >= Rect.Y && Y <= Rect.Y + Rect.H)) return true;
    return false;
}

// Return True if ch is whitespace
bool
is_whitespace(u32 ch)
{
    if (ch == L' ')
        return true;
    return false;
}

// Return True if ch is a supported markdown markup character
// TODO: tilde
bool
is_markdown(u32 ch)
{
    if (ch == L'_' ||
        ch == L'*')
        return true;
    return false;
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
    Assert(XLimit > 0);
    Assert(YLimit > 0);
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
    Assert(YLimit > 0);
    Assert(XLimit > 0);

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
    Assert(WrapPositionsIndex == WrapPositionsLen);
    Assert(MDFormat.Len == MDFormatOptionsIndex);

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

#endif
