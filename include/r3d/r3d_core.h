/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CORE_H
#define R3D_CORE_H

#include "./r3d_platform.h"
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

/**
 * @brief Anti-aliasing modes for rendering.
 */
typedef enum R3D_AntiAliasing {
    R3D_ANTI_ALIASING_DISABLED, ///< Anti-aliasing is disabled. Edges may appear jagged.
    R3D_ANTI_ALIASING_FXAA,     ///< FXAA is applied. Smooths edges efficiently but may appear blurry.
} R3D_AntiAliasing;

/**
 * @brief Aspect ratio handling modes for rendering.
 */
typedef enum R3D_AspectMode {
    R3D_ASPECT_EXPAND,      ///< Expands the rendered output to fully fill the target (render texture or window).
    R3D_ASPECT_KEEP         ///< Preserves the target's aspect ratio without distortion, adding empty gaps if necessary.
} R3D_AspectMode;

/**
 * @brief Upscaling/filtering methods for rendering output.
 *
 * Upscale mode to apply when the output window is larger than the internal render resolution.
 */
typedef enum R3D_UpscaleMode {
    R3D_UPSCALE_NEAREST,    ///< Nearest-neighbor upscaling: very fast, but produces blocky pixels.
    R3D_UPSCALE_LINEAR,     ///< Bilinear upscaling: very fast, smoother than nearest, but can appear blurry.
    R3D_UPSCALE_BICUBIC,    ///< Bicubic (Catmull-Rom) upscaling: slower, smoother, and less blurry than linear.
    R3D_UPSCALE_LANCZOS     ///< Lanczos-2 upscaling: preserves more fine details, but is the most expensive.
} R3D_UpscaleMode;

/**
 * @brief Downscaling/filtering methods for rendering output.
 *
 * Downscale mode to apply when the output window is smaller than the internal render resolution.
 */
typedef enum R3D_DownscaleMode {
    R3D_DOWNSCALE_NEAREST,  ///< Nearest-neighbor downscaling: very fast, but produces aliasing.
    R3D_DOWNSCALE_LINEAR,   ///< Bilinear downscaling: very fast, can serve as a basic form of anti-aliasing (SSAA).
    R3D_DOWNSCALE_BOX       ///< Box-blur downscaling: uses a simple but effective box blur, slightly more expensive than linear, smooths moiré better.
} R3D_DownscaleMode;

/**
 * @brief Defines the buffer to output (render texture or window).
 * @note Nothing will be output if the requested target has not been created / used.
 */
typedef enum R3D_OutputMode {
    R3D_OUTPUT_SCENE,
    R3D_OUTPUT_ALBEDO,
    R3D_OUTPUT_NORMAL,
    R3D_OUTPUT_TANGENT,
    R3D_OUTPUT_ORM,
    R3D_OUTPUT_DIFFUSE,
    R3D_OUTPUT_SPECULAR,
    R3D_OUTPUT_SSAO,
    R3D_OUTPUT_SSIL,
    R3D_OUTPUT_SSR,
    R3D_OUTPUT_BLOOM
} R3D_OutputMode;

/**
 * @brief Specifies the color space for user-provided colors and color textures.
 *
 * This enum defines how colors are interpreted for material inputs:
 * - Surface colors (e.g., albedo or emission tint)
 * - Color textures (albedo, emission maps)
 *
 * Lighting values (direct or indirect light) are always linear and
 * are not affected by this setting.
 *
 * Used with `R3D_SetColorSpace()` to control whether input colors
 * should be treated as linear or sRGB.
 */
typedef enum R3D_ColorSpace {
    R3D_COLORSPACE_LINEAR,  ///< Linear color space: values are used as-is.
    R3D_COLORSPACE_SRGB     ///< sRGB color space: values are converted to linear on load.
} R3D_ColorSpace;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the rendering engine.
 * 
 * This function sets up the internal rendering system with the provided resolution.
 * 
 * @param resWidth Width of the internal resolution.
 * @param resHeight Height of the internal resolution.
 */
R3DAPI void R3D_Init(int resWidth, int resHeight);

/**
 * @brief Closes the rendering engine and deallocates all resources.
 * 
 * This function shuts down the rendering system and frees all allocated memory, 
 * including the resources associated with the created lights.
 */
R3DAPI void R3D_Close(void);

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
 * @brief Retrieves the current anti-aliasing mode used for rendering.
 * @return The currently active R3D_AntiAliasing mode.
 */
R3DAPI R3D_AntiAliasing R3D_GetAntiAliasing(void);

/**
 * @brief Sets the anti-aliasing mode for rendering.
 * @param mode The desired R3D_AntiAliasing mode.
 */
R3DAPI void R3D_SetAntiAliasing(R3D_AntiAliasing mode);

/**
 * @brief Retrieves the current aspect ratio handling mode.
 * @return The currently active R3D_AspectMode.
 */
R3DAPI R3D_AspectMode R3D_GetAspectMode(void);

/**
 * @brief Sets the aspect ratio handling mode for rendering.
 * @param mode The desired R3D_AspectMode.
 */
R3DAPI void R3D_SetAspectMode(R3D_AspectMode mode);

/**
 * @brief Retrieves the current upscaling/filtering method.
 * @return The currently active R3D_UpscaleMode.
 */
R3DAPI R3D_UpscaleMode R3D_GetUpscaleMode(void);

/**
 * @brief Sets the upscaling/filtering method for rendering output.
 * @param mode The desired R3D_UpscaleMode.
 */
R3DAPI void R3D_SetUpscaleMode(R3D_UpscaleMode mode);

/**
 * @brief Retrieves the current downscaling mode used for rendering.
 * @return The currently active R3D_DownscaleMode.
 */
R3DAPI R3D_DownscaleMode R3D_GetDownscaleMode(void);

/**
 * @brief Sets the downscaling mode for rendering output.
 * @param mode The desired R3D_DownscaleMode.
 */
R3DAPI void R3D_SetDownscaleMode(R3D_DownscaleMode mode);

/**
 * @brief Gets the current output mode.
 * @return The currently active R3D_OutputMode.
 */
R3DAPI R3D_OutputMode R3D_GetOutputMode(void);

/**
 * @brief Sets the output mode for rendering.
 * @param mode The R3D_OutputMode to use.
 * @note Nothing will be output if the requested target has not been created / used.
 */
R3DAPI void R3D_SetOutputMode(R3D_OutputMode mode);

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
 * @brief Set the working color space for user-provided surface colors and color textures.
 *
 * Defines how all *color inputs* should be interpreted:
 * - surface colors provided in materials (e.g. albedo/emission tints)
 * - color textures such as albedo and emission maps
 *
 * When set to sRGB, these values are converted to linear before shading.
 * When set to linear, values are used as-is.
 *
 * This does NOT affect lighting inputs (direct or indirect light),
 * which are always expected to be provided in linear space.
 *
 * The default color space is `R3D_COLORSPACE_SRGB`.
 *
 * @param space Color space to use for color inputs (linear or sRGB).
 */
R3DAPI void R3D_SetColorSpace(R3D_ColorSpace space);

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
 * @param bitfield Bitfield representing the layers to activate.
 */
R3DAPI void R3D_SetActiveLayers(R3D_Layer bitfield);

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
