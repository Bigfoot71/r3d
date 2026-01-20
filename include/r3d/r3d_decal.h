/* r3d_decal.h -- R3D Decal Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_DECAL_H
#define R3D_DECAL_H

#include "./r3d_platform.h"
#include "./r3d_material.h"

/**
 * @defgroup Decal
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

/**
 * @brief Default decal configuration.
 *
 * Contains a R3D_Decal structure with sensible default values for all rendering parameters.
 */
#define R3D_DECAL_BASE                                  \
    R3D_LITERAL(R3D_Decal) {                            \
        .albedo = {                                     \
            .texture = {0},                             \
            .color = {255, 255, 255, 255},              \
        },                                              \
        .emission = {                                   \
            .texture = {0},                             \
            .color = {255, 255, 255, 255},              \
            .energy = 0.0f,                             \
        },                                              \
        .normal = {                                     \
            .texture = {0},                             \
            .scale = 1.0f,                              \
        },                                              \
        .orm = {                                        \
            .texture = {0},                             \
            .occlusion = 1.0f,                          \
            .roughness = 1.0f,                          \
            .metalness = 0.0f,                          \
        },                                              \
        .uvOffset = {0.0f, 0.0f},                       \
        .uvScale = {1.0f, 1.0f},                        \
        .alphaCutoff = 0.01f,                           \
        .normalThreshold = 0,                           \
        .fadeWidth = 0,                                 \
        .applyColor = true                              \
    }

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a decal and its properties.
 *
 * This structure defines a decal that can be projected onto geometry that has already been rendered.
 *
 * @note Decals are drawn using deferred screen space rendering and do not interact with any
 * forward rendered or non-opaque objects.
 */
typedef struct R3D_Decal {
    R3D_AlbedoMap albedo;       ///< Albedo map
    R3D_EmissionMap emission;   ///< Emission map
    R3D_NormalMap normal;       ///< Normal map
    R3D_OrmMap orm;             ///< Occlusion-Roughness-Metalness map
    Vector2 uvOffset;           ///< UV offset (default: {0.0f, 0.0f})
    Vector2 uvScale;            ///< UV scale (default: {1.0f, 1.0f})
    float alphaCutoff;          ///< Alpha cutoff threshold (default: 0.01f)
    float normalThreshold;      ///< Maximum angle against the surface normal to draw decal. 0.0f disables threshold. (default: 0.0f)
    float fadeWidth;            ///< The width of fading along the normal threshold (default: 0.0f)
    bool applyColor;            ///< Indicates that the albedo color will not be rendered, only the alpha component of the albedo will be used as a mask. (default: true)
} R3D_Decal;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Unload all map textures assigned to a R3D_Decal.
 * 
 * Frees all underlying textures in a R3D_Decal that are not a default texture.
 *
 * @param decal to unload maps from.
 */
R3DAPI void R3D_UnloadDecalMaps(R3D_Decal decal);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Decal

#endif // R3D_DECAL_H
