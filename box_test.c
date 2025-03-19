#include <stdio.h>
#include <stdbool.h>

#define Assert(expr) if (!(expr)) return false;

char
DrawingTest()
{
    Assert(2 == 3);

    return true;
}

char
FooTest()
{
    return false;
}


int
main(int Argc, char* Argv[])
{
    char *i(void);
    i = &DrawingTest;

}
