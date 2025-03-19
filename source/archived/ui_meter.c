#define TB_IMPL
#include <termbox2.h>

#include <locale.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

#define Assert(expr) \
    if (!(expr)) \
    { \
        tb_shutdown(); \
        raise(SIGTRAP); \
    }

#define METER_WIDTH 4

static char ***EnvironmentPath = 0;

void
draw_meter(int X, int Y, int Height, int MaxLevel, int Level)
{
    int FillColor = 0;

    Height -= 2; // substract top and bottom border

    tb_printf(X, Y,              0, 0, "%ls", L"┌──┐"); 
    tb_printf(X, Y + Height + 1, 0, 0, "%ls", L"└  ┘");

    if (Level == MaxLevel)
    {
        FillColor = TB_GREEN;
        tb_printf(X + 1, Y + Height + 1, 0, 0, "00");
    }
    else
    {
        FillColor = TB_CYAN;
        tb_printf(X + 1, Y + Height + 1, 0, 0, "%02d", Level);
    }

    int FilledLevel = (Level * Height)/MaxLevel;

    for (int LevelIndex = 0; LevelIndex < FilledLevel; LevelIndex++)
    {
        int YLevel = Y + (Height - LevelIndex);
        tb_set_cell(X + 1, YLevel, L'▒', FillColor, FillColor);
        tb_set_cell(X + 2, YLevel, L'▒', FillColor, FillColor);
    }

    for (int YOffset = 0; YOffset < Height; YOffset++)
    {
        tb_set_cell(X + 0, Y + YOffset + 1, L'│', 0, 0);
        tb_set_cell(X + 3, Y + YOffset + 1, L'│', 0, 0);
    }

}

int
main(int Argc, char *Args[], char *Envp[])
{
    EnvironmentPath = &Envp;

    struct tb_event ev = {0};
    Assert(setlocale(LC_ALL, ""));
    int Quit = 0;
    int Level = 0;
    int LevelStep = 10;
    int MeterMaxLevel = 100;
    int MeterHeight = 10 + 2;

    tb_init();
    tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE);

    while (!Quit)
    {
        tb_clear();

        int Y = 0;
        draw_meter(1, Y, MeterHeight, MeterMaxLevel, Level);
        Y += MeterHeight + 1;;

        tb_print(0, Y, TB_BOLD, 0, "j"); tb_print(2, Y++, 0, 0, "decrease");
        tb_print(0, Y, TB_BOLD, 0, "k"); tb_print(2, Y++, 0, 0, "increase");
        tb_print(0, Y, TB_BOLD, 0, "q"); tb_print(2, Y++, 0, 0, "to quit");

        tb_present();

        tb_poll_event(&ev);

        if (ev.ch == 'q' || ev.key == TB_KEY_CTRL_C) Quit = 1;
        else if ((ev.ch == 'j' || ev.key == TB_KEY_MOUSE_WHEEL_DOWN) &&
                 Level > 0)
        {
            if (LevelStep > Level) Level = 0;
            else Level -= LevelStep;
        }
        else if ((ev.ch == 'k' || ev.key == TB_KEY_MOUSE_WHEEL_UP) &&
                 Level < MeterMaxLevel)
        {
            if (Level + LevelStep > MeterMaxLevel) Level = MeterMaxLevel;
            else Level += LevelStep;
        }

    }

    tb_shutdown();
    return 0;
}
