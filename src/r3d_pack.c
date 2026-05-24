/* r3d_pack.c -- R3D Pack Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_pack.h>
#include <math.h>

#include "./common/r3d_half.h"

// ========================================
// PUBLIC API
// ========================================

uint16_t R3D_PackFloat16(float x)
{
    return r3d_half_from_float(x);
}

uint16_t R3D_PackUnorm16(float x)
{
    x = fminf(fmaxf(x, 0.0f), 1.0f);
    return (uint16_t)roundf(x * 65535.0f);
}

int16_t R3D_PackSnorm16(float x)
{
    x = fminf(fmaxf(x, -1.0f), 1.0f);
    return (int16_t)roundf(x * 32767.0f);
}

uint8_t R3D_PackUnorm8(float x)
{
    x = fminf(fmaxf(x, 0.0f), 1.0f);
    return (uint8_t)roundf(x * 255.0f);
}

int8_t R3D_PackSnorm8(float x)
{
    x = fminf(fmaxf(x, -1.0f), 1.0f);
    return (int8_t)roundf(x * 127.0f);
}

float R3D_UnpackFloat16(uint16_t x)
{
    return r3d_half_to_float(x);
}

float R3D_UnpackUnorm16(uint16_t x)
{
    return (float)x / 65535.0f;
}

float R3D_UnpackSnorm16(int16_t x)
{
    return fmaxf((float)x / 32767.0f, -1.0f);
}

float R3D_UnpackUnorm8(uint8_t x)
{
    return (float)x / 255.0f;
}

float R3D_UnpackSnorm8(int8_t x)
{
    return fmaxf((float)x / 127.0f, -1.0f);
}
