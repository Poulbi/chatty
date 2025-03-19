#define MAX_INPUT_LEN 255

#define TB_IMPL
#include "external/termbox2.h"
#undef TB_IMPL
#define CHATTY_IMPL
#include "ui.h"
#undef CHATTY_IMPL

void
DrawBoxTest(rect Box, wchar_t *DummyText)
{
    u32 TextLen = wcslen(DummyText);

    wchar_t Text[TextLen];
    bzero(Text, sizeof(Text));
    wcsncpy(Text, DummyText, TextLen + 1); // copy n*ull terminator

    rect TextR = TEXTBOXFROMBOX(Box);
    // fill the cursor for reference
    for (s32 Y = 0; Y < TextR.H; Y++)
        for (s32 X = 0; X < TextR.W; X++)
            tb_printf(TextR.X + X, TextR.Y + Y, 0, TB_BLUE, " ");

    DrawBox(Box, 0);
    tb_present();
    TextBoxDrawWrapped(TextR, Text, TextLen);
    tb_printf(0, TextR.Y - 1, 0, 0,
              "%d (%d, %d) ~(%d, %d)",
              TextLen,
              global.cursor_x, global.cursor_y,
              global.cursor_x - TextR.X, global.cursor_y - TextR.Y);
    tb_printf(global.cursor_x, global.cursor_y, 0, TB_RED, " ");
}

struct tb_cell
tb_get_cell(s32 X, s32 Y)
{
    return global.back.cells[global.width * Y + X];
}


int
main(int Argc, char* Argv[])
{
    struct tb_event ev;
    rect Box = {0, 0, 24, 4};

    setlocale(LC_ALL, "");

    tb_init();
    bytebuf_puts(&global.out, global.caps[TB_CAP_SHOW_CURSOR]);

    /* Text that does not fit */
    DrawBoxTest(Box, L"This is some dummy text meant to be wrapped multiple times.");
    Box.Y += Box.H;

    /* Text that fits */
    DrawBoxTest(Box, L"That is some.");
    Box.Y += Box.H;

    /* Text ending on a space */
    DrawBoxTest(Box, L"There is something. ");
    Box.Y += Box.H;

    /* Text that fits the surface */
    DrawBoxTest(Box, L"This is something. This is something.");
    Box.Y += Box.H;

    /* Text that wraps once */
    DrawBoxTest(Box, L"This is something I do not.");

    rect NewBox = Box;
    NewBox.X += Box.W + 2;
    DrawBox(NewBox, 0);

    tb_present();
    tb_poll_event(&ev);

    tb_shutdown();
    return 0;
}
