/* r3d_material.h -- R3D Material Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MATERIAL_H
#define R3D_MATERIAL_H

#include "./r3d_surface_shader.h"
#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Material
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

/**
 * @brief Default material configuration.
 *
 * Initializes an R3D_Material structure with sensible default values for all
 * rendering parameters. Use this as a starting point for custom configurations.
 */
#define R3D_MATERIAL_BASE                               \
    R3D_LITERAL(R3D_Material) {                         \
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
        .depth = {                                      \
            .mode = R3D_COMPARE_LESS,                   \
            .offsetFactor = 0.0f,                       \
            .offsetUnits = 0.0f,                        \
            .rangeNear = 0.0f,                          \
            .rangeFar = 1.0f,                           \
        },                                              \
        .stencil = {                                    \
            .mode = R3D_COMPARE_ALWAYS,                 \
            .ref = 0x00,                                \
            .mask = 0xFF,                               \
            .opFail = R3D_STENCIL_KEEP,                 \
            .opZFail = R3D_STENCIL_KEEP,                \
            .opPass = R3D_STENCIL_REPLACE,              \
        },                                              \
        .transparencyMode = R3D_TRANSPARENCY_DISABLED,  \
        .billboardMode = R3D_BILLBOARD_DISABLED,        \
        .blendMode = R3D_BLEND_MIX,                     \
        .cullMode = R3D_CULL_BACK,                      \
        .unlit = false,                                 \
        .shader = 0,                                    \
    }

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Transparency modes.
 *
 * This enumeration defines how a material handles transparency during rendering.
 * It controls whether transparency is disabled, rendered using a depth pre-pass,
 * or rendered with standard alpha blending.
 */
typedef enum R3D_TransparencyMode {
    R3D_TRANSPARENCY_DISABLED,      ///< No transparency, supports alpha cutoff.
    R3D_TRANSPARENCY_PREPASS,       ///< Supports transparency with shadows. Writes shadows for alpha > 0.1 and depth for alpha > 0.99.
    R3D_TRANSPARENCY_ALPHA,         ///< Standard transparency without shadows or depth writes.
} R3D_TransparencyMode;

/**
 * @brief Billboard modes.
 *
 * This enumeration defines how a 3D object aligns itself relative to the camera.
 * It provides options to disable billboarding or to enable specific modes of alignment.
 */
typedef enum R3D_BillboardMode {
    R3D_BILLBOARD_DISABLED,         ///< Billboarding is disabled; the object retains its original orientation.
    R3D_BILLBOARD_FRONT,            ///< Full billboarding; the object fully faces the camera, rotating on all axes.
    R3D_BILLBOARD_Y_AXIS            /**< Y-axis constrained billboarding; the object rotates only around the Y-axis,
                                         keeping its "up" orientation fixed. This is suitable for upright objects like characters or signs. */
} R3D_BillboardMode;

/**
 * @brief Blend modes.
 *
 * Defines common blending modes used in 3D rendering to combine source and destination colors.
 * @note The blend mode is applied only if you are in forward rendering mode or auto-detect mode.
 */
typedef enum R3D_BlendMode {
    R3D_BLEND_MIX,                  ///< Default mode: the result will be opaque or alpha blended depending on the transparency mode.
    R3D_BLEND_ADDITIVE,             ///< Additive blending: source color is added to the destination, making bright effects.
    R3D_BLEND_MULTIPLY,             ///< Multiply blending: source color is multiplied with the destination, darkening the image.
    R3D_BLEND_PREMULTIPLIED_ALPHA   ///< Premultiplied alpha blending: source color is blended with the destination assuming the source color is already multiplied by its alpha.
} R3D_BlendMode;

/**
 * @brief Comparison modes.
 *
 * Defines how fragments are tested against the depth/stencil buffer during rendering.
 * @note The depth/stencil comparison mode affects both forward and deferred rendering passes.
 */
typedef enum R3D_CompareMode {
    R3D_COMPARE_LESS,       ///< Passes if 'value' <  'buffer' (default)
    R3D_COMPARE_LEQUAL,     ///< Passes if 'value' <= 'buffer'
    R3D_COMPARE_EQUAL,      ///< Passes if 'value' == 'buffer'
    R3D_COMPARE_GREATER,    ///< Passes if 'value' >  'buffer'
    R3D_COMPARE_GEQUAL,     ///< Passes if 'value' >= 'buffer'
    R3D_COMPARE_NOTEQUAL,   ///< Passes if 'value' != 'buffer'
    R3D_COMPARE_ALWAYS,     ///< Always passes
    R3D_COMPARE_NEVER       ///< Never passes
} R3D_CompareMode;

/**
 * @brief Stencil buffer operations.
 *
 * Defines how the stencil buffer value is modified based on test results.
 */
typedef enum R3D_StencilOp {
    R3D_STENCIL_KEEP,      ///< Keep the current stencil value
    R3D_STENCIL_ZERO,      ///< Set stencil value to 0
    R3D_STENCIL_REPLACE,   ///< Replace with reference value
    R3D_STENCIL_INCR,      ///< Increment stencil value (clamped)
    R3D_STENCIL_DECR       ///< Decrement stencil value (clamped)
} R3D_StencilOp;

/**
 * @brief Face culling modes.
 *
 * Specifies which faces of a geometry are discarded during rendering based on their winding order.
 */
typedef enum R3D_CullMode {
    R3D_CULL_NONE,          ///< No culling; all faces are rendered.
    R3D_CULL_BACK,          ///< Cull back-facing polygons (faces with clockwise winding order).
    R3D_CULL_FRONT          ///< Cull front-facing polygons (faces with counter-clockwise winding order).
} R3D_CullMode;

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Albedo (base color) map.
 *
 * Provides the base color texture and a color multiplier.
 */
typedef struct R3D_AlbedoMap {
    Texture2D texture;  ///< Base color texture (default: WHITE)
    Color color;        ///< Color multiplier (default: WHITE)
} R3D_AlbedoMap;

/**
 * @brief Emission map.
 *
 * Provides emission texture, color, and energy multiplier.
 */
typedef struct R3D_EmissionMap {
    Texture2D texture;  ///< Emission texture (default: WHITE)
    Color color;        ///< Emission color (default: WHITE)
    float energy;       ///< Emission strength (default: 0.0f)
} R3D_EmissionMap;

/**
 * @brief Normal map.
 *
 * Provides normal map texture and scale factor.
 */
typedef struct R3D_NormalMap {
    Texture2D texture;  ///< Normal map texture (default: Front Facing)
    float scale;        ///< Normal scale (default: 1.0f)
} R3D_NormalMap;

/**
 * @brief Combined Occlusion-Roughness-Metalness (ORM) map.
 *
 * Provides texture and individual multipliers for occlusion, roughness, and metalness.
 */
typedef struct R3D_OrmMap {
    Texture2D texture;  ///< ORM texture (default: WHITE)
    float occlusion;    ///< Occlusion multiplier (default: 1.0f)
    float roughness;    ///< Roughness multiplier (default: 1.0f)
    float metalness;    ///< Metalness multiplier (default: 0.0f)
} R3D_OrmMap;

/**
 * @brief Depth buffer state configuration.
 *
 * Controls how fragments interact with the depth buffer during rendering..
 *
 * @note This structure does not directly control depth buffer writes for technical reasons.
 *       To render objects without writing to the depth buffer, use alpha blending mode instead.
 */
typedef struct R3D_DepthState {
    R3D_CompareMode mode;   ///< Comparison function for depth test (default: LESS)
    float offsetFactor;     ///< Scales the maximum depth slope for polygon offset (default: 0.0f)
    float offsetUnits;      ///< Constant depth offset value (default: 0.0f)
    float rangeNear;        ///< Near clipping plane for depth range mapping (default: 0.0f)
    float rangeFar;         ///< Far clipping plane for depth range mapping (default: 1.0f)
} R3D_DepthState;

/**
 * @brief Stencil buffer state configuration.
 *
 * Controls how fragments interact with the stencil buffer during rendering.
 * The stencil buffer can be used for effects like x-ray vision, outlines,
 * portals, and masking.
 */
typedef struct R3D_StencilState {
    R3D_CompareMode mode;   ///< Comparison function for stencil test (default: ALWAYS)
    uint8_t ref;            ///< Reference value (0-255) for comparison and replace operations (default: 0x00)
    uint8_t mask;           ///< Bit mask applied to both reference and stencil values during comparison (default: 0xFF)
    R3D_StencilOp opFail;   ///< Operation when stencil test fails (default: KEEP)
    R3D_StencilOp opZFail;  ///< Operation when stencil test passes but depth test fails (default: KEEP)
    R3D_StencilOp opPass;   ///< Operation when both stencil and depth tests pass (default: REPLACE)
} R3D_StencilState;

/**
 * @brief Material definition.
 *
 * Combines multiple texture maps and rendering parameters for shading.
 */
typedef struct R3D_Material {

    R3D_AlbedoMap albedo;                   ///< Albedo map
    R3D_EmissionMap emission;               ///< Emission map
    R3D_NormalMap normal;                   ///< Normal map
    R3D_OrmMap orm;                         ///< Occlusion-Roughness-Metalness map

    Vector2 uvOffset;                       ///< UV offset (default: {0.0f, 0.0f})
    Vector2 uvScale;                        ///< UV scale (default: {1.0f, 1.0f})
    float alphaCutoff;                      ///< Alpha cutoff threshold (default: 0.01f)

    R3D_DepthState depth;                   ///< Depth test configuration (default: standard)
    R3D_StencilState stencil;               ///< Stencil test configuration (default: disabled)

    R3D_TransparencyMode transparencyMode;  ///< Transparency mode (default: DISABLED)
    R3D_BillboardMode billboardMode;        ///< Billboard mode (default: DISABLED)
    R3D_BlendMode blendMode;                ///< Blend mode (default: MIX)
    R3D_CullMode cullMode;                  ///< Face culling mode (default: BACK)

    bool unlit;                             ///< If true, material does not participate in lighting (default: false)

    R3D_SurfaceShader* shader;              ///< Custom shader applied to the material (default: NULL)

} R3D_Material;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the default material configuration.
 *
 * Returns `R3D_MATERIAL_BASE` by default,
 * or the material defined via `R3D_SetDefaultMaterial()`.
 *
 * @return Default material structure with standard properties.
 */
R3DAPI R3D_Material R3D_GetDefaultMaterial(void);

/**
 * @brief Set the default material configuration.
 *
 * Allows you to override the default material.
 * The default material will be used as the basis for loading 3D models.
 *
 * @param material Default material to define.
 */
R3DAPI void R3D_SetDefaultMaterial(R3D_Material material);

/**
 * @brief Unload a material and its associated textures.
 *
 * Frees all memory associated with a material, including its textures.
 * This function will unload all textures that are not default textures.
 *
 * @warning Only call this function if you are certain that the textures
 * are not shared with other materials or objects, as this will permanently
 * free the texture data.
 *
 * @param material Pointer to the material structure to be unloaded.
 */
R3DAPI void R3D_UnloadMaterial(R3D_Material material);

/**
 * @brief Load an albedo (base color) map from file.
 *
 * Loads an image, uploads it as an sRGB texture (if enabled),
 * and applies the provided tint color.
 *
 * @param fileName Path to the texture file.
 * @param color Multiplicative tint applied in the shader.
 * @return Albedo map structure. Returns an empty map on failure.
 */
R3DAPI R3D_AlbedoMap R3D_LoadAlbedoMap(const char* fileName, Color color);

/**
 * @brief Load an albedo (base color) map from memory.
 *
 * Same behavior as R3D_LoadAlbedoMap(), but reads from memory instead of disk.
 *
 * @param fileType Image format hint (e.g. "png", "jpg").
 * @param fileData Pointer to image data.
 * @param dataSize Size of image data in bytes.
 * @param color Multiplicative tint applied in the shader.
 * @return Albedo map structure. Returns an empty map on failure.
 */
R3DAPI R3D_AlbedoMap R3D_LoadAlbedoMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color);

/**
 * @brief Unload an albedo map texture.
 *
 * Frees the underlying texture unless it is a default texture.
 *
 * @param map Albedo map to unload.
 */
R3DAPI void R3D_UnloadAlbedoMap(R3D_AlbedoMap map);

/**
 * @brief Load an emission map from file.
 *
 * Loads an emissive texture (sRGB if enabled) and sets color + energy.
 *
 * @param fileName Path to the texture file.
 * @param color Emission color.
 * @param energy Emission intensity multiplier.
 * @return Emission map. Returns an empty map on failure.
 */
R3DAPI R3D_EmissionMap R3D_LoadEmissionMap(const char* fileName, Color color, float energy);

/**
 * @brief Load an emission map from memory.
 *
 * Same behavior as R3D_LoadEmissionMap(), but reads from memory.
 *
 * @param fileType Image format hint.
 * @param fileData Pointer to image data.
 * @param dataSize Size of image data in bytes.
 * @param color Emission color.
 * @param energy Emission intensity multiplier.
 * @return Emission map. Returns an empty map on failure.
 */
R3DAPI R3D_EmissionMap R3D_LoadEmissionMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color, float energy);

/**
 * @brief Unload an emission map texture.
 *
 * Frees the texture unless it is a default texture.
 *
 * @param map Emission map to unload.
 */
R3DAPI void R3D_UnloadEmissionMap(R3D_EmissionMap map);

/**
 * @brief Load a normal map from file.
 *
 * Uploads the texture in linear space and stores the normal scale factor.
 *
 * @param fileName Path to the texture file.
 * @param scale Normal intensity multiplier.
 * @return Normal map. Returns an empty map on failure.
 */
R3DAPI R3D_NormalMap R3D_LoadNormalMap(const char* fileName, float scale);

/**
 * @brief Load a normal map from memory.
 *
 * Same behavior as R3D_LoadNormalMap(), but reads from memory.
 *
 * @param fileType Image format hint.
 * @param fileData Pointer to image data.
 * @param dataSize Size of image data in bytes.
 * @param scale Normal intensity multiplier.
 * @return Normal map. Returns an empty map on failure.
 */
R3DAPI R3D_NormalMap R3D_LoadNormalMapFromMemory(const char* fileType, const void* fileData, int dataSize, float scale);

/**
 * @brief Unload a normal map texture.
 *
 * Frees the texture unless it is a default texture.
 *
 * @param map Normal map to unload.
 */
R3DAPI void R3D_UnloadNormalMap(R3D_NormalMap map);

/**
 * @brief Load a combined ORM (Occlusion-Roughness-Metalness) map from file.
 *
 * Uploads the texture in linear space and applies the provided multipliers.
 *
 * @param fileName Path to the ORM texture.
 * @param occlusion Occlusion multiplier.
 * @param roughness Roughness multiplier.
 * @param metalness Metalness multiplier.
 * @return ORM map. Returns an empty map on failure.
 */
R3DAPI R3D_OrmMap R3D_LoadOrmMap(const char* fileName, float occlusion, float roughness, float metalness);

/**
 * @brief Load a combined ORM (Occlusion-Roughness-Metalness) map from memory.
 *
 * Same behavior as R3D_LoadOrmMap(), but reads from memory.
 *
 * @param fileType Image format hint.
 * @param fileData Pointer to image data.
 * @param dataSize Size of image data in bytes.
 * @param occlusion Occlusion multiplier.
 * @param roughness Roughness multiplier.
 * @param metalness Metalness multiplier.
 * @return ORM map. Returns an empty map on failure.
 */
R3DAPI R3D_OrmMap R3D_LoadOrmMapFromMemory(const char* fileType, const void* fileData, int dataSize,
                                           float occlusion, float roughness, float metalness);

/**
 * @brief Unload an ORM map texture.
 *
 * Frees the texture unless it is a default texture.
 *
 * @param map ORM map to unload.
 */
R3DAPI void R3D_UnloadOrmMap(R3D_OrmMap map);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Material

#endif // R3D_MATERIAL_H
