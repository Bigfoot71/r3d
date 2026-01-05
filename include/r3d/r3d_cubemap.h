/* r3d_cubemap.h -- R3D Cubemap Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CUBEMAP_H
#define R3D_CUBEMAP_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Cubemap
 * @{
 */

// ========================================
// ENUM TYPES
// ========================================

/**
 * @brief Supported cubemap source layouts.
 *
 * Used when converting an image into a cubemap. AUTO_DETECT tries to guess
 * the layout based on image dimensions.
 */
typedef enum R3D_CubemapLayout {
    R3D_CUBEMAP_LAYOUT_AUTO_DETECT = 0,         // Automatically detect layout type
    R3D_CUBEMAP_LAYOUT_LINE_VERTICAL,           // Layout is defined by a vertical line with faces
    R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL,         // Layout is defined by a horizontal line with faces
    R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,     // Layout is defined by a 3x4 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE,     // Layout is defined by a 4x3 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_PANORAMA                 // Layout is defined by an equirectangular panorama
} R3D_CubemapLayout;

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief GPU cubemap texture.
 *
 * Holds the OpenGL texture handle and its base resolution (per face).
 */
typedef struct R3D_Cubemap {
    uint32_t texture;
    int size;
} R3D_Cubemap;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a cubemap from an image file.
 *
 * The layout parameter tells how faces are arranged inside the source image.
 */
R3DAPI R3D_Cubemap R3D_LoadCubemap(const char* fileName, R3D_CubemapLayout layout);

/**
 * @brief Builds a cubemap from an existing Image.
 *
 * Same behavior as R3D_LoadCubemap(), but without loading from disk.
 */
R3DAPI R3D_Cubemap R3D_LoadCubemapFromImage(Image image, R3D_CubemapLayout layout);

/**
 * @brief Releases GPU resources associated with a cubemap.
 */
R3DAPI void R3D_UnloadCubemap(R3D_Cubemap cubemap);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Cubemap

#endif // R3D_CUBEMAP_H
