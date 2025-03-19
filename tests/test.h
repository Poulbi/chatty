// Library used for writing tests, must be included once for definitions and
// once for the `main()` function.
//
// DOCUMENTATION
//  Expect(expr)
//      Macro that will return early if the expression is false, calling
//      `tb_shutdown()` to save the terminal.  It will also print the expression
//      and line on which it occurred for convenience
//
//  test_functions TestFunctions
//      Global variable containing the function (test).  This array is
//      0-terminated.
//
//  TESTFUNC()
//      Macro for adding functions to TestFunctions array conveniently.
//
//  tb_get_cell(x, y)
//      Function
//
// EXAMPLE
//  #define TEST_IMPL
//  #include "test.h"
//
//  bool FooTest()
//  {
//      Expect(0 == 1);
//      return true;
//  }
//
//  test_functions TestFunctions[] = {
//      TESTFUNC(FooTest),
//      {0}
//  }
//
// int main(int Argc, int Argv)
// {
//      Test(Argc, Argv);
// }

#ifndef TEST_H
#define TEST_H

#include <stdio.h>
#include <stdbool.h>

#include "../source/chatty.h"

/* Macro's */
#define Expect(expr) if (!(expr)) \
    {  \
        tb_shutdown(); \
        printf("\n    %d: %s\n", __LINE__, #expr); \
        return false; \
    }

#define TESTFUNC(func) { func, #func }

/* Types */
typedef struct {
    bool (*Func)(void);
    const char * Name;
} test_functions;

typedef struct {
    s32 X, Y, W, H;
} rect;

/* Declarations */
struct tb_cell tb_get_cell(s32 x, s32 y);
void RectCopy(rect Dest, rect Source);
bool RectCmp(rect Dest, rect Source);
bool TextCmp(s32 X, s32 Y, wchar_t *Text, s32 TextLen);
bool TextCmpWithColor(s32 X, s32 Y, wchar_t *Text, s32 TextLen, s32 fg, s32 bg);

/* Global variables */
void (*Before)(void);
void (*After)(void);

/* Functions*/
struct tb_cell
tb_get_cell(s32 x, s32 y)
{
    return global.back.cells[global.width * y + x];
}

void
RectCopy(rect Dest, rect Source)
{
    for (u32 Y = 0; Y < Source.H; Y++)
    {
        for (u32 X = 0; X < Source.W; X++)
        {
            struct tb_cell Cell = tb_get_cell(Source.X + X, Source.Y + Y);
            tb_set_cell(Dest.X + X, Dest.Y + Y, Cell.ch, Cell.fg, Cell.bg);
        }
    }
}

bool
RectCmp(rect Dest, rect Source)
{
    for (u32 Y = 0; Y < Source.H; Y++)
    {
        for (u32 X = 0; X < Source.W; X++)
        {
            struct tb_cell SourceCell = tb_get_cell(Source.X + X, Source.Y + Y);
            struct tb_cell DestCell = tb_get_cell(Dest.X + X, Dest.Y + Y);
            if (!(SourceCell.fg == DestCell.fg &&
                  SourceCell.bg == DestCell.bg &&
                  SourceCell.ch == DestCell.ch))
            {
                return false;
            }
        }
    }

    return true;
}

bool
TextCmp(s32 X, s32 Y, wchar_t *Text, s32 TextLen)
{
    for (u32 At = 0;
         At < TextLen;
         At++)
    {
        struct tb_cell DestCell = tb_get_cell(X++, Y);
        if (DestCell.ch != Text[At]) return false;
    }

    return true;
}

bool
TextCmpWithColor(s32 X, s32 Y, wchar_t *Text, s32 TextLen, s32 fg, s32 bg)
{
    for (u32 At = 0;
         At < TextLen;
         At++)
    {
        struct tb_cell DestCell = tb_get_cell(X++, Y);
        if (DestCell.ch != Text[At] ||
            DestCell.fg != fg ||
            DestCell.bg != bg)
        {
            return false;
        }
    }

    return true;
}

int
Test(test_functions *TestFunctions, int Argc, char *Argv[])
{
    u32 TestFunctionsLen = 0;
    while (TestFunctions[TestFunctionsLen].Func) TestFunctionsLen++;

    if (Argc > 1)
    {
        for (u32 Arg = 0;
             Arg < Argc;
             Arg++)
        {
            char *Function = Argv[Arg];

            for (u32 TestFunc = 0;
                 TestFunc < TestFunctionsLen;
                 TestFunc++)
            {
                if(!strcmp(Function, TestFunctions[TestFunc].Name))
                {
                    printf("%s ", TestFunctions[TestFunc].Name);

                    if (Before) Before();
                    bool Ret = TestFunctions[TestFunc].Func();
                    if (After) After();

                    if (Ret)
                    {
                        printf("\033[32mPASSED\033[0m\n");
                    }
                    else
                    {
                        printf("\033[31mFAILED\033[0m\n"); \
                    }
                }
            }
        }
    }
    else
    {
        for (int At = 0;
             TestFunctions[At].Func;
             At++)
        {
            printf("%s ", TestFunctions[At].Name);

            if (Before) Before();
            bool Ret = TestFunctions[At].Func();
            if (After) After();

            if (Ret)
            {
                printf("\033[32mPASSED\033[0m\n");
            }
            else
            {
                printf("\033[31mFAILED\033[0m\n"); \
            }
        }
    }

    return 0;
}

#endif // TEST_H
