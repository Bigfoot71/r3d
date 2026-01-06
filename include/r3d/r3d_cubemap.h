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
// CONSTANTS
// ========================================

#define R3D_CUBEMAP_SKY_BASE                                    \
    R3D_LITERAL(R3D_CubemapSky) {                               \
        .skyTopColor = (Color) {98, 116, 140, 255},             \
        .skyHorizonColor = (Color) {165, 167, 171, 255},        \
        .skyHorizonCurve = 0.15f,                               \
        .skyEnergy = 1.0f,                                      \
        .groundBottomColor = (Color) {51, 43, 34, 255},         \
        .groundHorizonColor = (Color) {165, 167, 171, 255},     \
        .groundHorizonCurve = 0.02f,                            \
        .groundEnergy = 1.0f,                                   \
        .sunDirection = (Vector3) {-1.0f, -1.0f, -1.0f},        \
        .sunColor = (Color) {255, 255, 255, 255},               \
        .sunSize = 1.5f * DEG2RAD,                              \
        .sunCurve = 0.15,                                       \
        .sunEnergy = 1.0f,                                      \
    }

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
    R3D_CUBEMAP_LAYOUT_AUTO_DETECT = 0,         ///< Automatically detect layout type
    R3D_CUBEMAP_LAYOUT_LINE_VERTICAL,           ///< Layout is defined by a vertical line with faces
    R3D_CUBEMAP_LAYOUT_LINE_HORIZONTAL,         ///< Layout is defined by a horizontal line with faces
    R3D_CUBEMAP_LAYOUT_CROSS_THREE_BY_FOUR,     ///< Layout is defined by a 3x4 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE,     ///< Layout is defined by a 4x3 cross with cubemap faces
    R3D_CUBEMAP_LAYOUT_PANORAMA                 ///< Layout is defined by an equirectangular panorama
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
    uint32_t fbo;
    int size;
} R3D_Cubemap;

/**
 * @brief Parameters for procedural sky generation.
 *
 * Curves control gradient falloff (lower = sharper transition at horizon).
 */
typedef struct R3D_CubemapSky {

    Color skyTopColor;          // Sky color at zenith
    Color skyHorizonColor;      // Sky color at horizon
    float skyHorizonCurve;      // Gradient curve exponent (0.01 - 1.0, typical: 0.15)
    float skyEnergy;            // Sky brightness multiplier

    Color groundBottomColor;    // Ground color at nadir
    Color groundHorizonColor;   // Ground color at horizon
    float groundHorizonCurve;   // Gradient curve exponent (typical: 0.02)
    float groundEnergy;         // Ground brightness multiplier

    Vector3 sunDirection;       // Direction from which light comes (can take not normalized)
    Color sunColor;             // Sun disk color
    float sunSize;              // Sun angular size in radians (real sun: ~0.0087 rad = 0.5°)
    float sunCurve;             // Sun edge softness exponent (typical: 0.15)
    float sunEnergy;            // Sun brightness multiplier

} R3D_CubemapSky;

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
 * @brief Generates a procedural sky cubemap.
 *
 * Creates a GPU cubemap with procedural gradient sky and sun rendering.
 * The cubemap is ready for use as environment map or IBL source.
 */
R3DAPI R3D_Cubemap R3D_GenCubemapSky(int size, R3D_CubemapSky params);

/**
 * @brief Releases GPU resources associated with a cubemap.
 */
R3DAPI void R3D_UnloadCubemap(R3D_Cubemap cubemap);

/**
 * @brief Updates an existing procedural sky cubemap.
 *
 * Re-renders the cubemap with new parameters. Faster than unload + generate
 * when animating sky conditions (time of day, weather, etc.).
 */
R3DAPI void R3D_UpdateCubemapSky(R3D_Cubemap* cubemap, R3D_CubemapSky params);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Cubemap

#endif // R3D_CUBEMAP_H
