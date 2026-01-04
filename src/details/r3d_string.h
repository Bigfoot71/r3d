#ifndef R3D_DETAILS_STRING_H
#define R3D_DETAILS_STRING_H

#include <stdarg.h>
#include <stdio.h>

static inline void r3d_string_format(char *dst, size_t dstSize, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(dst, dstSize, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= dstSize) {
        dst[dstSize - 1] = '\0';
    }
}

#endif // R3D_DETAILS_STRING_H
