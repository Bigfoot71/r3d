/* r3d_skybox.h -- R3D Skybox Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SKYBOX_H
#define R3D_SKYBOX_H

#include "./r3d_api.h"
#include <raylib.h>

/**
 * @defgroup Skybox
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Structure representing a skybox and its related textures for lighting.
 *
 * This structure contains textures used for rendering a skybox, as well as
 * precomputed lighting textures used for image-based lighting (IBL).
 */
typedef struct R3D_Skybox {
    TextureCubemap cubemap;  ///< The skybox cubemap texture for the background and reflections.
    Texture2D irradiance;    ///< The irradiance cubemap for diffuse ambient lighting.
    Texture2D prefilter;     ///< The prefiltered cubemap for specular reflections with mipmaps.
} R3D_Skybox;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// SKYBOX: Skybox Loading Functions
// ----------------------------------------

/**
 * @brief Loads a skybox from a texture file.
 *
 * This function loads a skybox cubemap from a texture file using a specified cubemap layout.
 * The layout defines how the six faces of the cubemap are arranged within the texture.
 *
 * @param filePath The path to the texture file.
 * @param layout The cubemap layout format.
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkybox(const char* filePath, CubemapLayout layout);

/**
 * @brief Loads a skybox from an image in memory.
 *
 * This function loads a skybox cubemap from an image already loaded in memory,
 * using a specified cubemap layout to map the six faces.
 *
 * @param image The source image in memory.
 * @param layout The cubemap layout format.
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxFromMemory(Image image, CubemapLayout layout);

/**
 * @brief Loads a skybox from a panorama texture file.
 *
 * This function loads a skybox from a panorama (equirectangular) texture file,
 * and converts it into a cubemap with the specified resolution.
 *
 * @param filePath The path to the panorama texture file.
 * @param size The resolution of the generated cubemap (e.g., 512, 1024).
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxPanorama(const char* filePath, int size);

/**
 * @brief Loads a skybox from a panorama image in memory.
 *
 * This function loads a skybox from a panorama (equirectangular) image already loaded in memory,
 * and converts it into a cubemap with the specified resolution.
 *
 * @param image The panorama image in memory.
 * @param size The resolution of the generated cubemap (e.g., 512, 1024).
 * @return The loaded skybox object.
 */
R3DAPI R3D_Skybox R3D_LoadSkyboxPanoramaFromMemory(Image image, int size);

/**
 * @brief Unloads a skybox and frees its resources.
 *
 * This function removes a previously loaded skybox from memory.
 * It should be called when the skybox is no longer needed to prevent memory leaks.
 *
 * @param sky The skybox to unload.
 */
R3DAPI void R3D_UnloadSkybox(R3D_Skybox sky);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Skybox

#endif // R3D_SKYBOX_H
