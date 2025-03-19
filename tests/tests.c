#define MAX_INPUT_LEN 255

#define TB_IMPL
#include "../source/termbox2.h"
#include "../source/chatty.h"

#define TEST_IMPL
#include "test.h"

#define Assert(expr) if (!(expr)) *(u8*)0 = 0

bool
DrawingTest(void)
{
    struct tb_event ev = {0};

    return true;
}

int
main(int Argc, char* Argv[])
{
    test_functions TestFunctions[] = {
        TESTFUNC(DrawingTest),
        { 0 }
    };

    Assert(setlocale(LC_ALL, ""));

    Before = (void(*)(void))tb_init;
    After = (void(*)(void))tb_shutdown;

    return(Test(TestFunctions, Argc, Argv));
}
