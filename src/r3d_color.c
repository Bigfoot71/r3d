/* r3d_color.c -- R3D Color Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_color.h>

#include "./common/r3d_math.h"
#include "./r3d_core_state.h"

// ========================================
// PUBLIC API
// ========================================

Vector4 R3D_ColorSrgbToLinear(Color color)
{
    return r3d_color_srgb_to_linear_vec4(color);
}

Vector3 R3D_ColorSrgbToLinearVector3(Color color)
{
    return r3d_color_srgb_to_linear_vec3(color);
}

Color R3D_ColorLinearToSrgb(Vector4 color)
{
    return r3d_color_linear_to_srgb_vec4(color);
}

Vector4 R3D_ColorFromCurrentSpace(Color color)
{
    return r3d_color_to_linear_vec4(color, R3D.colorSpace);
}

Vector3 R3D_ColorFromCurrentSpaceVector3(Color color)
{
    return r3d_color_to_linear_vec3(color, R3D.colorSpace);
}

Color R3D_ColorFromTemperature(float kelvin)
{
    // Tanner Helland approximation, valid between 1000K and 40000K
    // https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html

    float temp = kelvin / 100.0f;
    float r, g, b;

    if (temp <= 66.0f) r = 255.0f;
    else r = 329.698727446f * powf(temp - 60.0f, -0.1332047592f);

    if (temp <= 66.0f) g = 99.4708025861f * logf(temp) - 161.1195681661f;
    else g = 288.1221695283f * powf(temp - 60.0f, -0.0755148492f);

    if (temp >= 66.0f) b = 255.0f;
    else if (temp <= 19.0f) b = 0.0f;
    else b = 138.5177312231f * logf(temp - 10.0f) - 305.0447927307f;

    return (Color) {
        (uint8_t)fmaxf(0.0f, fminf(255.0f, r)),
        (uint8_t)fmaxf(0.0f, fminf(255.0f, g)),
        (uint8_t)fmaxf(0.0f, fminf(255.0f, b)),
        255
    };
}
