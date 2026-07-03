/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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
 * @defgroup Core
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Configuration hints used to customize R3D before initialization.
 *
 * Hints must be set via R3D_SetHint() before calling R3D_Init().
 * Any hint not explicitly set falls back to its default value.
 *
 * @note Some hints may be clamped internally.
 */
typedef enum R3D_Hint {
    R3D_HINT_MAX_LIGHT_FORWARD_PER_MESH,    ///< Max lights per mesh in forward pass. Default: 8
    R3D_HINT_MAX_PROBE_ON_SCREEN,           ///< Max probes rendered simultaneously. Default: 8
    R3D_HINT_PROBE_CAPTURE_SIZE,            ///< Probe capture cubemap face size (px). Default: 256
    R3D_HINT_SHADOW_MAP_DIRECTIONAL_SIZE,   ///< Directional light shadow map size (px). Default: 4096
    R3D_HINT_SHADOW_MAP_SPOT_SIZE,          ///< Spot light shadow map size (px). Default: 2048
    R3D_HINT_SHADOW_MAP_OMNI_SIZE,          ///< Omni light shadow map size (px). Default: 2048
    R3D_HINT_CUBEMAP_IRRADIANCE_SIZE,       ///< Irradiance cubemap face size (px). Default: 32
    R3D_HINT_CUBEMAP_PREFILTER_SIZE,        ///< Prefiltered cubemap face size (px). Default: 128
    R3D_HINT_COUNT,                         ///< Sentinel, not a valid hint
} R3D_Hint;

/**
 * @brief Anti-aliasing modes used during rendering.
 *
 * Anti-aliasing reduces visible jagged edges (aliasing artifacts)
 * in the final rendered image.
 */
typedef enum R3D_AntiAliasingMode {
    R3D_ANTI_ALIASING_MODE_NONE,  ///< No anti-aliasing. Best performance, visible jagged edges.
    R3D_ANTI_ALIASING_MODE_FXAA,  ///< Fast Approximate AA. Smooths edges efficiently but may appear blurry.
    R3D_ANTI_ALIASING_MODE_SMAA,  ///< Subpixel Morphological AA. Sharper than FXAA, moderate performance cost.
} R3D_AntiAliasingMode;

/**
 * @brief Quality presets for anti-aliasing.
 *
 * Presets adjust internal algorithm parameters (e.g. edge detection,
 * search steps, thresholds). Higher presets increase quality and GPU cost.
 */
typedef enum R3D_AntiAliasingPreset {
    R3D_ANTI_ALIASING_PRESET_LOW,     ///< Performance-oriented preset with reduced quality.
    R3D_ANTI_ALIASING_PRESET_MEDIUM,  ///< Balanced quality/performance preset.
    R3D_ANTI_ALIASING_PRESET_HIGH,    ///< High quality preset with increased GPU cost.
    R3D_ANTI_ALIASING_PRESET_ULTRA,   ///< Maximum quality preset, highest performance cost.
    R3D_ANTI_ALIASING_PRESET_COUNT,   ///< Number of presets (not a valid preset value).
} R3D_AntiAliasingPreset;

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
    R3D_UPSCALE_BICUBIC,    ///< Bicubic upscaling: slower, smoother, and less blurry than linear.
    R3D_UPSCALE_LANCZOS     ///< Lanczos-2 upscaling: preserves more fine details, but is the most expensive.
} R3D_UpscaleMode;

/**
 * @brief Downscaling/filtering methods for rendering output.
 *
 * Downscale mode to apply when the output window is smaller than the internal render resolution.
 */
typedef enum R3D_DownscaleMode {
    R3D_DOWNSCALE_NEAREST,  ///< Nearest-neighbor downscaling: very fast, but produces aliasing.
    R3D_DOWNSCALE_LINEAR,   ///< Bilinear filtering. Fast, may show moire on high-frequency content.
    R3D_DOWNSCALE_RGSS,     ///< 4-sample supersampling. Reduces aliasing and moire, low cost. Recommended default.
    R3D_DOWNSCALE_PDSS      ///< 16-sample supersampling. Better color accuracy than RGSS, higher cost.
} R3D_DownscaleMode;

/**
 * @brief Defines the buffer to output (render texture or window).
 * @note Nothing will be output if the requested target has not been created / used.
 */
typedef enum R3D_OutputMode {
    R3D_OUTPUT_SCENE,
    R3D_OUTPUT_ALBEDO,
    R3D_OUTPUT_NORMAL,
    R3D_OUTPUT_ORM,
    R3D_OUTPUT_DIFFUSE,
    R3D_OUTPUT_SPECULAR,
    R3D_OUTPUT_SSAO,
    R3D_OUTPUT_SSIL,
    R3D_OUTPUT_SSGI,
    R3D_OUTPUT_SSR,
    R3D_OUTPUT_BLOOM,
    R3D_OUTPUT_DOF,
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
 * @brief Sets a configuration hint.
 * 
 * Must be called before R3D_Init().
 */
R3DAPI void R3D_SetHint(R3D_Hint hint, int value);

/**
 * @brief Returns the effective value of a hint.
 */
R3DAPI int R3D_GetHint(R3D_Hint hint);

/**
 * @brief Initializes the rendering engine.
 * 
 * This function sets up the internal rendering system with the provided resolution.
 * 
 * @param resWidth Width of the internal resolution.
 * @param resHeight Height of the internal resolution.
 *
 * @return True if the initialization is successful.
 */
R3DAPI bool R3D_Init(int resWidth, int resHeight);

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
 * @brief Sets the internal rendering resolution.
 *
 * Reallocates all internal render targets to the new resolution.
 * This operation may cause a stall, this is acceptable when called
 * infrequently (like window resize events), but should never be called per-frame.
 *
 * @param width New internal width in pixels.
 * @param height New internal height in pixels.
 */
R3DAPI void R3D_SetResolution(int width, int height);

/**
 * @brief Retrieves the current anti-aliasing mode used for rendering.
 *
 * @return The currently active R3D_AntiAliasingMode.
 */
R3DAPI R3D_AntiAliasingMode R3D_GetAntiAliasingMode(void);

/**
 * @brief Sets the anti-aliasing mode for rendering.
 *
 * The new mode takes effect on subsequent frames.
 *
 * @param mode The desired R3D_AntiAliasingMode.
 * @note If the mode is invalid, no AA will be applied.
 */
R3DAPI void R3D_SetAntiAliasingMode(R3D_AntiAliasingMode mode);

/**
 * @brief Retrieves the current anti-aliasing quality preset.
 *
 * @return The currently active R3D_AntiAliasingPreset.
 */
R3DAPI R3D_AntiAliasingPreset R3D_GetAntiAliasingPreset(void);

/**
 * @brief Sets the anti-aliasing quality preset.
 *
 * Changing the preset triggers an internal shader recompilation.
 * Compiled variants are cached and reused if the preset is set again.
 *
 * @param preset The desired R3D_AntiAliasingPreset.
 * @note The preset will be a clamp between low and ultra.
 */
R3DAPI void R3D_SetAntiAliasingPreset(R3D_AntiAliasingPreset preset);

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
 * @brief Sets the default texture wrap mode.
 *
 * This function only affects textures that are loaded manually for material maps.
 * Textures loaded automatically during model import will use the wrap mode
 * defined in the model file itself.
 *
 * The default texture wrap mode is `TEXTURE_WRAP_CLAMP`.
 *
 * @param wrap The texture wrap mode to apply by default.
 */
R3DAPI void R3D_SetTextureWrap(TextureWrap wrap);

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

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Core

#endif // R3D_CORE_H
