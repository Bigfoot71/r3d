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
