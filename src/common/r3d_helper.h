/* r3d_helper.h -- Common R3D Helpers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_HELPER_H
#define R3D_COMMON_HELPER_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

// ========================================
// HELPER MACROS
// ========================================

#ifndef MIN
#   define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#   define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef CLAMP
#   define CLAMP(v, min, max) ((v) < (min) ? (min) : ((v) > (max) ? (max) : (v)))
#endif

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#endif

// ========================================
// HELPER FUNCTIONS
// ========================================

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

static inline int r3d_lsb_index(uint32_t value)
{
    if (value == 0) return -1;

#if defined(__clang__)
#  if __has_builtin(__builtin_ctz)
    return __builtin_ctz(value);
#  endif
#elif defined(__GNUC__)
    return __builtin_ctz(value);
#endif

    int index = 0;
    while ((value & 1) == 0) {
        value >>= 1;
        index++;
    }
    return index;
}

#endif // R3D_COMMON_HELPER_H
