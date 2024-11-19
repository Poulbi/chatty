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
        if (MDFormat.Len &&
            TextIndex == MDFormat.Options[MDFormatOptionsIndex].Position)
        {
            fg ^= MDFormat.Options[MDFormatOptionsIndex].Color;
            MDFormatOptionsIndex++;
        }
        if (WrapPositionsLen &&
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
markdown_to_raw(Arena* ScratchArena, u32* Text, u32 Len)
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
preprocess_markdown(Arena* ScratchArena, u32* Text, u32 Len)
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
