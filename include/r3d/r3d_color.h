/* r3d_color.h -- R3D Color Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COLOR_H
#define R3D_COLOR_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup Color
 * @{
 */

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts an sRGB color to linear space.
 *
 * @param color The sRGB color to convert.
 * @return The converted linear color as a Vector4.
 */
R3DAPI Vector4 R3D_ColorSrgbToLinear(Color color);

/**
 * @brief Converts an sRGB color to linear space.
 *
 * @param color The sRGB color to convert.
 * @return The converted linear color as a Vector3.
 */
R3DAPI Vector3 R3D_ColorSrgbToLinearVector3(Color color);

/**
 * @brief Converts a linear color to sRGB space.
 *
 * @param color The linear color to convert.
 * @return The converted sRGB color.
 */
R3DAPI Color R3D_ColorLinearToSrgb(Vector4 color);

/**
 * @brief Converts a color from the current color space to linear space.
 *
 * @param color The color to convert.
 * @return The converted linear color as a Vector4.
 */
R3DAPI Vector4 R3D_ColorFromCurrentSpace(Color color);

/**
 * @brief Converts a color from the current color space to linear space.
 *
 * @param color The color to convert.
 * @return The converted linear color as a Vector3.
 */
R3DAPI Vector3 R3D_ColorFromCurrentSpaceVector3(Color color);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Color

#endif // R3D_COLOR_H
