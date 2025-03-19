// 1. Search backwards for whitespace
// - found?
//   y) wrap
//   n) break at limit
// - end?
//   y) terminate
//   n) goto 1. with offset += limit
void
wrap(u8* Text, u32 Len, u32 XLimit, u32 YLimit)
{
    u32 SearchingOffset = XLimit;
    u32 X = SearchingOffset;
    u32 Y = 0;
    u8 t;
    u32 PrevX = 0;

    while (X < Len)
    {
        // Search for whitespace to break on
        while (1)
        {
            if (is_whitespace(Text[X])) break;

            X--;

            // if we got back to the previous position break on Text[SearchingOffset]
            if (X == PrevX)
            {
                X = XLimit;
                break;
            }
        }

        // break
        t = Text[X];
        Text[X] = '\0';
        tb_printf(0, Y, 0, 0, "%s", Text + PrevX);
        Text[X] = t;
        Y++;
        if (Y >= YLimit) break;

        // consume leading whitespace
        while (is_whitespace(Text[X])) X++;

        PrevX = X;
        X += XLimit;
    }

    tb_printf(0, Y, 0, 0, "%s", Text + PrevX);

    return;
}
