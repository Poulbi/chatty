#ifndef BOX_H
#define BOX_H

// assumes MAX_INPUT_LEN to be set
//
// #define MAX_INPUT_LEN 128

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
void ScrollLeft(rect Text, u32 *TextOffset);
void ScrollRight(rect Text, u32 *TextOffset);
void TextBoxDelete(wchar_t* Text, u64 Pos);
void TextBoxInsert(wchar_t *Input, u32 InputPos, u32 InputLen, wchar_t ch);
u32 TextBoxKeypress(struct tb_event ev, rect TextR, wchar_t *Text, u32 *TextLenPtr, u32 TextPos, u32 *TextOffsetPtr);
void TextBoxDrawText(rect TextR, wchar_t *Input, u32 InputLen, u32 TextOffset);

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
            (MAX_INPUT_LEN - Pos - 1) * sizeof(*Text));
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

// Handle the key event and edit Input and updates TextLenPtr and TextOffsetPtr accordingly.
// InputPos is the position in the Input relating to the cursor position.
// TextR is the bounding box for the text.
// Returns 0 no key event was handled
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

// Draws characters in Input in the TextR rectangle.
// Skip printing TextOffset amount of characters.
// InputLen is the amount of characters in Input.
//
// NOTE: TextR is always filled, when not enough characters in Input it will uses spaces instead.
// This makes it easy to update the textbox by recalling this function.
void
TextBoxDraw(rect TextR, wchar_t *Text, u32 TextLen)
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
        }
        else
        {
            tb_printf(AtX++, AtY, 0, 0, " ");
        }

        if (AtX == TextR.X + TextR.W) { AtY++; AtX = TextR.X; }
    }
}

// NOTE: To ensure that the text looks the same even when scrolling it you must provide the whole text,
// wrap the whole text and the only show the portion that can fit in the Text Rectangle.

// When line will exceed width break the word on the next line. This is done by looking backwards
// for whitespace from TextR.W width. When a whitespace is found text is wrapped on the next line.
void
TextBoxDrawWrapped(rect TextR, wchar_t *Text, u32 TextLen)
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

#endif // BOX_H
