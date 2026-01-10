/* r3d_ambient_map.h -- R3D Ambient Map Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_AMBIENT_MAP_H
#define R3D_AMBIENT_MAP_H

#include "./r3d_cubemap.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup EnvironmentMap
 * @{
 */

// ========================================
// FLAG TYPES
// ========================================

/**
 * @brief Bit-flags controlling what components are generated.
 *
 * - R3D_AMBIENT_ILLUMINATION -> generate diffuse irradiance
 * - R3D_AMBIENT_REFLECTION   -> generate specular prefiltered map
 */
typedef uint32_t R3D_AmbientFlag;

#define R3D_AMBIENT_ILLUMINATION    (1 << 0)
#define R3D_AMBIENT_REFLECTION      (1 << 1)

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Global environment lighting data.
 *
 * An ambient map is built from a cubemap (like a skybox)
 * and preprocessed into two specialized textures:
 *
 *  - irradiance:
 *      Low-frequency lighting used for diffuse IBL.
 *      Captures soft ambient light from all directions.
 *
 *  - prefilter:
 *      Mipmapped environment used for specular reflections.
 *      Higher mip levels simulate rougher surfaces.
 *
 * Both textures are derived from the same source cubemap,
 * but serve different shading purposes.
 */
typedef struct R3D_AmbientMap {
    R3D_AmbientFlag flags;  ///< Components generated for this map
    uint32_t irradiance;    ///< Diffuse IBL cubemap (may be 0 if not generated)
    uint32_t prefilter;     ///< Specular prefiltered cubemap (may be 0 if not generated)
} R3D_AmbientMap;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a ambient map from an image file.
 *
 * The layout parameter tells how faces are arranged inside the source image.
 */
R3DAPI R3D_AmbientMap R3D_LoadAmbientMap(const char* fileName, R3D_CubemapLayout layout, R3D_AmbientFlag flags);

/**
 * @brief Builds a ambient map from an existing Image.
 *
 * Same behavior as R3D_LoadAmbientMap(), but without loading from disk.
 */
R3DAPI R3D_AmbientMap R3D_LoadAmbientMapFromImage(Image image, R3D_CubemapLayout layout, R3D_AmbientFlag flags);

/**
 * @brief Generates an ambient map from a cubemap.
 *
 * The source cubemap should usually be an HDR sky/environment.
 *
 * Depending on the provided flags, this function:
 *  - convolves the cubemap into diffuse irradiance
 *  - builds a mipmapped prefiltered cubemap for reflections
 *
 * @param cubemap Source cubemap (environment / sky).
 * @param flags   Which components to generate (irradiance, reflection, or both).
 * @return A fully initialized ambient map.
 */
R3DAPI R3D_AmbientMap R3D_GenAmbientMap(R3D_Cubemap cubemap, R3D_AmbientFlag flags);

/**
 * @brief Frees the textures used by an ambient map.
 *
 * After this call, the ambient map is no longer valid.
 */
R3DAPI void R3D_UnloadAmbientMap(R3D_AmbientMap ambientMap);

/**
 * @brief Rebuilds an existing ambient map from a new cubemap.
 *
 * Use this when the environment changes dynamically (time of day,
 * weather, interior/exterior transitions, etc).
 *
 * Only the components enabled in `ambientMap.flags` are regenerated.
 *
 * @param ambientMap Existing ambient map to update.
 * @param cubemap    New cubemap source.
 */
R3DAPI void R3D_UpdateAmbientMap(R3D_AmbientMap ambientMap, R3D_Cubemap cubemap);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of EnvironmentMap

#endif // R3D_AMBIENT_MAP_H
