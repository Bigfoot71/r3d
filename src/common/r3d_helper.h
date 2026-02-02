/* r3d_helper.h -- Common R3D Helpers
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_HELPER_H
#define R3D_COMMON_HELPER_H

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#if defined(_MSC_VER)
#   include <intrin.h>
#endif

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

#ifndef SATURATE
#   define SATURATE(x) (CLAMP(x, 0.0f, 1.0f))
#endif

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))
#endif

#ifndef SWAP
#   define SWAP(type, a, b) do { type _tmp = (a); (a) = (b); (b) = _tmp; } while (0)
#endif

#ifndef BIT_SET
#   define BIT_SET(v, m) ((v) |= (m))
#endif

#ifndef BIT_CLEAR
#   define BIT_CLEAR(v, m) ((v) &= ~(m))
#endif

#ifndef BIT_TOGGLE
#   define BIT_TOGGLE(v, m) ((v) ^= (m))
#endif

#ifndef BIT_TEST_ALL
#   define BIT_TEST_ALL(v, m) (((v) & (m)) == (m))
#endif

#ifndef BIT_TEST_ANY
#   define BIT_TEST_ANY(v, m) (((v) & (m)) != 0)
#endif

#ifndef BIT_TEST
#   define BIT_TEST(v, b) BIT_TEST_ANY(v, b)
#endif

// ========================================
// HELPER FUNCTIONS
// ========================================

/*
 * Returns the number of logical CPUs available to the system.
 * The value is detected once and cached.
 */
int r3d_get_cpu_count(void);

// ========================================
// INLINED FUNCTIONS
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

static inline int32_t r3d_get_mip_levels_1d(int32_t s)
{
    if (s <= 0) return 0;

#if defined(__GNUC__) || defined(__clang__)
    return 32 - __builtin_clz((unsigned)s);
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanReverse(&index, (unsigned long)s);
    return (int32_t)index + 1;
#else
    int32_t levels = 0;
    while (s > 0) {
        levels++;
        s >>= 1;
    }
    return levels;
#endif
}

static inline int32_t r3d_get_mip_levels_2d(int32_t w, int32_t h)
{
    return r3d_get_mip_levels_1d((w > h) ? w : h);
}

static inline int32_t r3d_lsb_index(uint32_t value)
{
    if (value == 0) return -1;

#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctz(value);
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, (unsigned long)value);
    return (int32_t)index;
#else
    int32_t index = 0;
    while ((value & 1) == 0) {
        value >>= 1;
        index++;
    }
    return index;
#endif
}

static inline int r3d_align_offset(int offset, int align)
{
    return (offset + align - 1) & ~(align - 1);
}

#endif // R3D_COMMON_HELPER_H
