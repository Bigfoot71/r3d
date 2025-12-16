/* r3d_material.h -- R3D Material Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MATERIAL_H
#define R3D_MATERIAL_H

#include "./r3d_api.h"
#include <raylib.h>

/**
 * @defgroup Material
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Blend modes for rendering.
 *
 * Defines common blending modes used in 3D rendering to combine source and destination colors.
 * @note The blend mode is applied only if you are in forward rendering mode or auto-detect mode.
 */
typedef enum R3D_BlendMode {
    R3D_BLEND_OPAQUE,               ///< No blending, the source color fully replaces the destination color.
    R3D_BLEND_ALPHA,                ///< Alpha blending: source color is blended with the destination based on alpha value.
    R3D_BLEND_ADDITIVE,             ///< Additive blending: source color is added to the destination, making bright effects.
    R3D_BLEND_MULTIPLY,             ///< Multiply blending: source color is multiplied with the destination, darkening the image.
    R3D_BLEND_PREMULTIPLIED_ALPHA   ///< Premultiplied alpha blending: source color is blended with the destination assuming the source color is already multiplied by its alpha.
} R3D_BlendMode;

/**
 * @brief Defines the depth mode used to render the mesh.
 */
typedef enum R3D_DepthMode {
    R3D_DEPTH_READ_WRITE,       ///< Enable depth testing and writing to the depth buffer.
    R3D_DEPTH_READ_ONLY,        ///< Enable depth testing but disable writing to the depth buffer.
    R3D_DEPTH_DISABLED          ///< Disable depth testing and writing to the depth buffer.
} R3D_DepthMode;

/**
 * @brief Face culling modes for a mesh.
 *
 * Specifies which faces of a geometry are discarded during rendering based on their winding order.
 */
typedef enum R3D_CullMode {
    R3D_CULL_NONE,   ///< No culling; all faces are rendered.
    R3D_CULL_BACK,   ///< Cull back-facing polygons (faces with clockwise winding order).
    R3D_CULL_FRONT   ///< Cull front-facing polygons (faces with counter-clockwise winding order).
} R3D_CullMode;

/**
 * @brief Defines billboard modes for 3D objects.
 *
 * This enumeration defines how a 3D object aligns itself relative to the camera.
 * It provides options to disable billboarding or to enable specific modes of alignment.
 */
typedef enum R3D_BillboardMode {
    R3D_BILLBOARD_DISABLED,     ///< Billboarding is disabled; the object retains its original orientation.
    R3D_BILLBOARD_FRONT,        ///< Full billboarding; the object fully faces the camera, rotating on all axes.
    R3D_BILLBOARD_Y_AXIS        /**< Y-axis constrained billboarding; the object rotates only around the Y-axis,
                                     keeping its "up" orientation fixed. This is suitable for upright objects like characters or signs. */
} R3D_BillboardMode;

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Represents a material with textures, parameters, and rendering modes.
 *
 * Combines multiple texture maps and settings used during shading.
 */
typedef struct R3D_Material {

    struct R3D_MapAlbedo {
        Texture2D texture;      ///< Albedo (base color) texture.
        Color color;            ///< Albedo color multiplier.
    } albedo;

    struct R3D_MapEmission {
        Texture2D texture;      ///< Emission texture.
        Color color;            ///< Emission color.
        float energy;           ///< Emission energy multiplier.
    } emission;

    struct R3D_MapNormal {
        Texture2D texture;      ///< Normal map texture.
        float scale;            ///< Normal scale.
    } normal;

    struct R3D_MapORM {
        Texture2D texture;      ///< Combined Occlusion-Roughness-Metalness texture.
        float occlusion;        ///< Occlusion multiplier.
        float roughness;        ///< Roughness multiplier.
        float metalness;        ///< Metalness multiplier.
    } orm;

    R3D_BlendMode blendMode;              ///< Blend mode used for rendering the material.
    R3D_DepthMode depthMode;              ///< Depth mode used for rendering the material. Not taken into account for decals.
    R3D_CullMode cullMode;                ///< Face culling mode used for the material.

    R3D_BillboardMode billboardMode;      ///< Billboard mode applied to the object.

    Vector2 uvOffset;                     /**< UV offset applied to the texture coordinates.
                                           *  For models, this can be set manually.
                                           *  For sprites, this value is overridden automatically.
                                           */

    Vector2 uvScale;                      /**< UV scale factor applied to the texture coordinates.
                                           *  For models, this can be set manually.
                                           *  For sprites, this value is overridden automatically.
                                           */

    float alphaCutoff;          ///< Alpha threshold below which fragments are discarded.

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
 * Returns a default material with standard properties and default textures.
 * This material can be used as a fallback or starting point for custom materials.
 *
 * @return Default material structure with standard properties.
 */
R3DAPI R3D_Material R3D_GetDefaultMaterial(void);

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
R3DAPI void R3D_UnloadMaterial(const R3D_Material* material);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Material

#endif // R3D_MATERIAL_H
