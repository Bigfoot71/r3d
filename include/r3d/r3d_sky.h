/* r3d_sky.h -- R3D Sky Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SKY_H
#define R3D_SKY_H

#include "./r3d_sky_shader.h"
#include "./r3d_platform.h"
#include "./r3d_cubemap.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Sky
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

#define R3D_PROCEDURAL_SKY_BASE                                 \
    R3D_LITERAL(R3D_ProceduralSky) {                            \
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
        .sunCurve = 0.15f,                                      \
        .sunEnergy = 1.0f,                                      \
    }

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Parameters for procedural sky generation.
 *
 * Curves control gradient falloff (lower = sharper transition at horizon).
 */
typedef struct R3D_ProceduralSky {

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

} R3D_ProceduralSky;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generates a procedural sky cubemap.
 *
 * Creates a GPU cubemap with procedural gradient sky and sun rendering.
 * The cubemap is ready for use as environment map or IBL source.
 */
R3DAPI R3D_Cubemap R3D_GenProceduralSky(int size, R3D_ProceduralSky params);

/**
 * @brief Generates a custom sky cubemap.
 *
 * Creates a GPU cubemap rendered using the provided sky shader.
 * The cubemap is ready for use as environment map or IBL source.
 */
R3DAPI R3D_Cubemap R3D_GenCustomSky(int size, R3D_SkyShader* shader);

/**
 * @brief Updates an existing procedural sky cubemap.
 *
 * Re-renders the cubemap with new parameters. Faster than unload + generate
 * when animating sky conditions (time of day, weather, etc.).
 */
R3DAPI void R3D_UpdateProceduralSky(R3D_Cubemap* cubemap, R3D_ProceduralSky params);

/**
 * @brief Updates an existing custom sky cubemap.
 *
 * Re-renders the cubemap using the provided sky shader. Faster than unload + generate
 * when animating sky conditions or updating shader uniforms (time, clouds, stars, etc.).
 */
R3DAPI void R3D_UpdateCustomSky(R3D_Cubemap* cubemap, R3D_SkyShader* shader);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Sky

#endif // R3D_SKY_H
