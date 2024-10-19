#include "config.h"
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>

// wrapper for write
void writef(char *format, ...)
{
    va_list args;
    char buf[BUF_MAX + 1];
    va_start(args, format);

    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    int n = 0;
    while (*(buf + n) != 0)
        n++;
    write(0, buf, n);
}
