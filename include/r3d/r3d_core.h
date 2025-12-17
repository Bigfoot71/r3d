/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CORE_H
#define R3D_CORE_H

#include "./r3d_api.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Core Core Functions
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Flags to configure the rendering engine behavior.
 *
 * These flags control various aspects of the rendering pipeline.
 */
typedef uint32_t R3D_Flags;

#define R3D_FLAG_NONE                   0           /**< No special rendering flags */
#define R3D_FLAG_FXAA                   (1 << 0)    /**< Enables Fast Approximate Anti-Aliasing (FXAA) */
#define R3D_FLAG_BLIT_LINEAR            (1 << 1)    /**< Uses linear filtering when blitting the final image */
#define R3D_FLAG_ASPECT_KEEP            (1 << 2)    /**< Maintains the aspect ratio of the internal resolution when blitting the final image */
#define R3D_FLAG_8_BIT_NORMALS          (1 << 3)    /**< Use 8-bit precision for the normals buffer (deferred); default is 16-bit float */
#define R3D_FLAG_NO_FRUSTUM_CULLING     (1 << 4)    /**< Disables internal frustum culling. Manual culling is allowed, but may break shadow visibility if objects casting shadows are skipped. */
#define R3D_FLAG_TRANSPARENT_SORTING    (1 << 5)    /**< Back-to-front sorting of transparent objects for correct blending of non-discarded fragments. Be careful, in 'force forward' mode this flag will also sort opaque objects in 'near-to-far' but in the same sorting pass. */
#define R3D_FLAG_OPAQUE_SORTING         (1 << 6)    /**< Front-to-back sorting of opaque objects to optimize depth testing at the cost of additional sorting. Please note, in 'force forward' mode this flag has no effect, see transparent sorting. */
#define R3D_FLAG_LOW_PRECISION_BUFFERS  (1 << 7)    /**< Use 32-bit HDR formats like R11G11B10F for intermediate color buffers instead of full 16-bit floats. Saves memory and bandwidth. */

/**
 * @brief Bitfield type used to specify rendering layers for 3D objects.
 *
 * This type is used by `R3D_Mesh` and `R3D_Sprite` objects to indicate
 * which rendering layer(s) they belong to. Active layers are controlled
 * globally via the functions:
 * 
 * - void R3D_EnableLayers(R3D_Layer bitfield);
 * - void R3D_DisableLayers(R3D_Layer bitfield);
 *
 * A mesh or sprite will be rendered if at least one of its assigned layers is active.
 *
 * For simplicity, 16 layers are defined in this header, but the maximum number
 * of layers is 32 for an uint32_t.
 */
typedef uint32_t R3D_Layer;

#define R3D_LAYER_01    (1 << 0)
#define R3D_LAYER_02    (1 << 1)
#define R3D_LAYER_03    (1 << 2)
#define R3D_LAYER_04    (1 << 3)
#define R3D_LAYER_05    (1 << 4)
#define R3D_LAYER_06    (1 << 5)
#define R3D_LAYER_07    (1 << 6)
#define R3D_LAYER_08    (1 << 7)
#define R3D_LAYER_09    (1 << 8)
#define R3D_LAYER_10    (1 << 9)
#define R3D_LAYER_11    (1 << 10)
#define R3D_LAYER_12    (1 << 11)
#define R3D_LAYER_13    (1 << 12)
#define R3D_LAYER_14    (1 << 13)
#define R3D_LAYER_15    (1 << 14)
#define R3D_LAYER_16    (1 << 15)

#define R3D_LAYER_ALL   0xFFFFFFFF

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the rendering engine.
 * 
 * This function sets up the internal rendering system with the provided resolution 
 * and state flags, which define the internal behavior. These flags can be modified
 * later via R3D_SetState.
 * 
 * @param resWidth Width of the internal resolution.
 * @param resHeight Height of the internal resolution.
 * @param flags Flags indicating internal behavior (modifiable via R3D_SetState).
 */
R3DAPI void R3D_Init(int resWidth, int resHeight, unsigned int flags);

/**
 * @brief Closes the rendering engine and deallocates all resources.
 * 
 * This function shuts down the rendering system and frees all allocated memory, 
 * including the resources associated with the created lights.
 */
R3DAPI void R3D_Close(void);

/**
 * @brief Checks if a specific internal state flag is set.
 * 
 * @param flag The state flag to check.
 * @return True if the flag is set, false otherwise.
 */
R3DAPI bool R3D_HasState(unsigned int flag);

/**
 * @brief Sets internal state flags for the rendering engine.
 * 
 * This function modifies the behavior of the rendering engine by setting one or more 
 * state flags. Flags can be later cleared with R3D_ClearState.
 * 
 * @param flags The flags to set.
 */
R3DAPI void R3D_SetState(unsigned int flags);

/**
 * @brief Clears specific internal state flags.
 * 
 * This function clears one or more previously set state flags, modifying the 
 * behavior of the rendering engine accordingly.
 * 
 * @param flags The flags to clear.
 */
R3DAPI void R3D_ClearState(unsigned int flags);

/**
 * @brief Gets the current internal resolution.
 * 
 * This function retrieves the current internal resolution being used by the 
 * rendering engine.
 * 
 * @param width Pointer to store the width of the internal resolution.
 * @param height Pointer to store the height of the internal resolution.
 */
R3DAPI void R3D_GetResolution(int* width, int* height);

/**
 * @brief Updates the internal resolution.
 * 
 * This function changes the internal resolution of the rendering engine. Note that 
 * this process destroys and recreates all framebuffers, which may be a slow operation.
 * 
 * @param width The new width for the internal resolution.
 * @param height The new height for the internal resolution.
 * 
 * @warning This function may be slow due to the destruction and recreation of framebuffers.
 */
R3DAPI void R3D_UpdateResolution(int width, int height);

/**
 * @brief Defines the bounds of the scene for directional light calculations.
 * 
 * This function sets the scene bounds used to determine which areas should be illuminated 
 * by directional lights. It is the user's responsibility to calculate and provide the 
 * correct bounds.
 * 
 * @param sceneBounds The bounding box defining the scene's dimensions.
 */
R3DAPI void R3D_SetSceneBounds(BoundingBox sceneBounds);

/**
 * @brief Sets the default texture filtering mode.
 * 
 * This function defines the default texture filter that will be applied to all subsequently 
 * loaded textures, including those used in materials, sprites, and other resources.
 * 
 * If a trilinear or anisotropic filter is selected, mipmaps will be automatically generated 
 * for the textures, but they will not be generated when using nearest or bilinear filtering.
 *
 * The default texture filter mode is `TEXTURE_FILTER_TRILINEAR`.
 * 
 * @param filter The texture filtering mode to be applied by default.
 */
R3DAPI void R3D_SetTextureFilter(TextureFilter filter);

/**
 * @brief Get the currently active global rendering layers.
 *
 * Returns the bitfield representing the currently active layers in the renderer.
 * By default, the internal active layers are set to 0, which means that any
 * non-zero layer assigned to an object will NOT be rendered unless explicitly
 * activated.
 *
 * @return R3D_Layer Bitfield of active layers.
 */
R3DAPI R3D_Layer R3D_GetActiveLayers(void);

/**
 * @brief Set the active global rendering layers.
 *
 * Replaces the current set of active layers with the given bitfield.
 *
 * @param layers Bitfield representing the layers to activate.
 */
R3DAPI void R3D_SetActiveLayers(R3D_Layer layers);

/**
 * @brief Enable one or more layers without affecting other active layers.
 *
 * This function sets the bits in the global active layers corresponding to
 * the bits in the provided bitfield. Layers already active remain active.
 *
 * @param bitfield Bitfield representing one or more layers to enable.
 */
R3DAPI void R3D_EnableLayers(R3D_Layer bitfield);

/**
 * @brief Disable one or more layers without affecting other active layers.
 *
 * This function clears the bits in the global active layers corresponding to
 * the bits in the provided bitfield. Layers not included in the bitfield
 * remain unchanged.
 *
 * @param bitfield Bitfield representing one or more layers to disable.
 */
R3DAPI void R3D_DisableLayers(R3D_Layer bitfield);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Core

#endif // R3D_CORE_H
