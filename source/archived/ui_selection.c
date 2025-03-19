#define TB_IMPL
#include "../chatty/external/termbox2.h"

#include <signal.h>

#define Assert(expr) if (!(expr)) raise(SIGTRAP)

typedef struct {
    int X, Y;
} Position;

int
main(void)
{
    int Selected = 0;
    int NumChoices = 3;
    int Y = 0;

    Position Positions[NumChoices];
    Positions[0] = (Position){1, Y};
    Positions[1] = (Position){5, Y};
    Positions[2] = (Position){9, Y};
    Y += 2;

    struct tb_event ev = {0};
    int Color = TB_GREEN;

    tb_init();

    int Quit = 0;
    while (!Quit)
    {
        tb_clear();

        int BaseY = Y;
        tb_printf(0, BaseY, TB_BOLD, 0, "j"); tb_printf(2, BaseY++, 0, 0, "select next");
        tb_printf(0, BaseY, TB_BOLD, 0, "k"); tb_printf(2, BaseY++, 0, 0, "select previous");
        tb_printf(0, BaseY, TB_BOLD, 0, "s"); tb_printf(2, BaseY++, 0, 0, "change color");
        tb_printf(0, BaseY, TB_BOLD, 0, "q"); tb_printf(2, BaseY++, 0, 0, "quit");

        // Draw a box at position
        for (int PositionsIndex = 0; PositionsIndex < NumChoices; PositionsIndex++)
        {
            Assert(Positions[PositionsIndex].X > 0);
            tb_printf(Positions[PositionsIndex].X - 1, Positions[PositionsIndex].Y, Color, 0, "[ ]");
        }
        // Draw mark in selected box
        tb_printf(Positions[Selected].X, Positions[Selected].Y, Color, 0, "x");

        tb_present();
        tb_poll_event(&ev);

        if (ev.ch == 'q') Quit = 1;
        else if (ev.ch == 'j' && Selected < NumChoices - 1) Selected++;
        else if (ev.ch == 'k' && Selected)                  Selected--;
        else if (ev.ch == 's')
        {
            switch (Selected)
            {
            case 0: Color = TB_GREEN; break;
            case 1: Color = TB_BLUE; break;
            case 2: Color = TB_RED; break;
            default: Assert(0); break;
            }
        }
    }

    tb_shutdown();

    return 0;
}
