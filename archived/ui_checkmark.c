#define TB_IMPL
#include "../chatty/external/termbox2.h"

int
main(void)
{
    struct tb_event ev = {0};

    typedef struct {
        int X, Y;
        int Checked;
    } Checkmark;

#define NUM_CHECKMARKS 4
    int Y = 0;
    Checkmark Marks[NUM_CHECKMARKS] = {
        {0, Y++, 0},
        {0, Y++, 0},
        {0, Y++, 1},
        {0, Y++, 0}
    };
    Y++;

    int Selected = 0;

    tb_init();

    int Quit = 0;
    while (!Quit)
    {
        tb_clear();

        for (int CheckmarkIndex = 0;
             CheckmarkIndex < NUM_CHECKMARKS;
             CheckmarkIndex++)
        {
            Checkmark Mark = Marks[CheckmarkIndex];
            if (Mark.Checked)
            {
                tb_printf(Mark.X, Mark.Y, 0, 0, "[x]");
            }
            else
            {
                tb_printf(Mark.X, Mark.Y, 0, 0, "[ ]");
            }
        }
        Checkmark Mark = Marks[Selected];
        if (Mark.Checked)
        {
            tb_set_cell(Mark.X + 1, Mark.Y, L'x', TB_UNDERLINE, 0);
        }
        else
        {
            tb_set_cell(Mark.X + 1, Mark.Y, L' ', TB_UNDERLINE, 0);
        }

        int BaseY = Y;
        tb_printf(0, BaseY, TB_BOLD, 0, "j"); tb_printf(2, BaseY++, 0, 0, "next");
        tb_printf(0, BaseY, TB_BOLD, 0, "k"); tb_printf(2, BaseY++, 0, 0, "previous");
        tb_printf(0, BaseY, TB_BOLD, 0, "c"); tb_printf(2, BaseY++, 0, 0, "toggle");
        tb_printf(0, BaseY, TB_BOLD, 0, "q"); tb_printf(2, BaseY++, 0, 0, "quit");

        tb_present();

        tb_poll_event(&ev);
        if (ev.ch == 'q')
        {
            Quit = 1;
        }
        else if (ev.ch == 'j')
        {
            if (Selected == NUM_CHECKMARKS - 1) Selected = 0;
            else Selected++; 
        }
        else if (ev.ch == 'k')
        {
            if (Selected) Selected--;
            else Selected = NUM_CHECKMARKS - 1;
        }
        else if (ev.ch == 'c')
        {
            Marks[Selected].Checked = !Marks[Selected].Checked;
        }
    }

    tb_shutdown();
}
