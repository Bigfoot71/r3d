/* r3d_environment.h -- R3D Environment Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ENVIRONMENT_H
#define R3D_ENVIRONMENT_H

#include "./r3d_ambient_map.h"
#include "./r3d_platform.h"
#include "./r3d_cubemap.h"

/**
 * @defgroup Environment
 * @brief Scene rendering and post-processing configuration.
 *
 * Controls background, lighting, and visual effects through the R3D_Environment structure.
 * Includes skybox, ambient light, SSAO, bloom, SSR, fog, depth of field, tone mapping,
 * and color grading.
 *
 * ## Usage
 *
 * ```c
 * // Direct access
 * R3D_Environment* env = R3D_GetEnvironment();
 * env->bloom.intensity = 0.8f;
 *
 * // Quick macro update
 * R3D_ENVIRONMENT_SET(bloom.intensity, 0.8f);
 *
 * // Batch configuration
 * R3D_Environment custom = R3D_ENVIRONMENT_BASE;
 * custom.bloom.mode = R3D_BLOOM_ADDITIVE;
 * custom.ssao.enabled = true;
 * R3D_SetEnvironment(&custom);
 * ```
 *
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

/**
 * @brief Default environment configuration.
 *
 * Initializes an R3D_Environment structure with sensible default values for all
 * rendering parameters. Use this as a starting point for custom configurations.
 */
#define R3D_ENVIRONMENT_BASE                            \
    R3D_LITERAL(R3D_Environment) {                      \
        .background = {                                 \
            .color = GRAY,                              \
            .energy = 1.0f,                             \
            .skyBlur = 0.0f,                            \
            .sky = {0},                                 \
            .rotation = {0.0f, 0.0f, 0.0f, 1.0f},       \
        },                                              \
        .ambient = {                                    \
            .color = BLACK,                             \
            .energy = 1.0f,                             \
            .map = (R3D_AmbientMap) {0},                \
        },                                              \
        .ssao = {                                       \
            .sampleCount = 16,                          \
            .intensity = 0.5f,                          \
            .power = 1.5f,                              \
            .radius = 0.5f,                             \
            .bias = 0.02f,                              \
            .enabled = false,                           \
        },                                              \
        .ssil = {                                       \
            .sampleCount = 2,                           \
            .sliceCount = 4,                            \
            .radius = 2.0f,                             \
            .thickness = 1.0f,                          \
            .intensity = 1.0f,                          \
            .aoPower = 1.0f,                            \
            .denoiseSteps = 4,                          \
            .enabled = false,                           \
        },                                              \
        .ssgi = {                                       \
            .sampleCount = 2,                           \
            .maxRaySteps = 32,                          \
            .stepSize = 0.125f,                         \
            .thickness = 1.0f,                          \
            .maxDistance = 4.0f,                        \
            .fadeStart = 8.0f,                          \
            .fadeEnd = 16.0f,                           \
            .denoiseSteps = 5,                          \
            .enabled = false,                           \
        },                                              \
        .ssr = {                                        \
            .maxRaySteps = 32,                          \
            .binarySteps = 4,                           \
            .stepSize = 0.125f,                         \
            .thickness = 0.2f,                          \
            .maxDistance = 4.0f,                        \
            .edgeFade = 0.25f,                          \
            .enabled = false,                           \
        },                                              \
        .bloom = {                                      \
            .mode = R3D_BLOOM_DISABLED,                 \
            .levels = 0.5f,                             \
            .intensity = 0.05f,                         \
            .threshold = 0.0f,                          \
            .softThreshold = 0.5f,                      \
            .filterRadius = 1.0f,                       \
        },                                              \
        .fog = {                                        \
            .mode = R3D_FOG_DISABLED,                   \
            .color = {255, 255, 255, 255},              \
            .start = 1.0f,                              \
            .end = 50.0f,                               \
            .density = 0.05f,                           \
            .skyAffect = 0.5f,                          \
        },                                              \
        .dof = {                                        \
            .mode = R3D_DOF_DISABLED,                   \
            .focusPoint = 10.0f,                        \
            .focusScale = 1.0f,                         \
            .maxBlurSize = 20.0f,                       \
        },                                              \
        .tonemap = {                                    \
            .mode = R3D_TONEMAP_LINEAR,                 \
            .exposure = 1.0f,                           \
            .white = 1.0f,                              \
        },                                              \
        .color = {                                      \
            .brightness = 1.0f,                         \
            .contrast = 1.0f,                           \
            .saturation = 1.0f,                         \
        },                                              \
    }

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Bloom effect modes.
 *
 * Different blending methods for the bloom glow effect.
 */
typedef enum R3D_Bloom {
    R3D_BLOOM_DISABLED,     ///< No bloom effect applied
    R3D_BLOOM_MIX,          ///< Linear interpolation blend between scene and bloom
    R3D_BLOOM_ADDITIVE,     ///< Additive blending, intensifying bright regions
    R3D_BLOOM_SCREEN        ///< Screen blending for softer highlight enhancement
} R3D_Bloom;

/**
 * @brief Fog effect modes.
 *
 * Distance-based fog density distribution methods.
 */
typedef enum R3D_Fog {
    R3D_FOG_DISABLED,       ///< No fog effect
    R3D_FOG_LINEAR,         ///< Linear density increase between start and end distances
    R3D_FOG_EXP2,           ///< Exponential squared density (exp2), more realistic
    R3D_FOG_EXP             ///< Simple exponential density increase
} R3D_Fog;

/**
 * @brief Depth of field modes.
 */
typedef enum R3D_DoF {
    R3D_DOF_DISABLED,       ///< No depth of field effect
    R3D_DOF_ENABLED         ///< Depth of field enabled with focus point and blur
} R3D_DoF;

/**
 * @brief Tone mapping algorithms.
 *
 * HDR to LDR color compression methods.
 */
typedef enum R3D_Tonemap {
    R3D_TONEMAP_LINEAR,     ///< Direct linear mapping (no compression)
    R3D_TONEMAP_REINHARD,   ///< Reinhard operator, balanced HDR compression
    R3D_TONEMAP_FILMIC,     ///< Film-like response curve
    R3D_TONEMAP_ACES,       ///< Academy Color Encoding System (cinematic standard)
    R3D_TONEMAP_AGX,        ///< Modern algorithm preserving highlights and shadows
    R3D_TONEMAP_COUNT       ///< Internal: number of tonemap modes
} R3D_Tonemap;

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Background and skybox configuration.
 */
typedef struct R3D_EnvBackground {
    Color color;            ///< Background color when there is no skybox
    float energy;           ///< Energy multiplier applied to background (skybox or color)
    float skyBlur;          ///< Sky blur factor [0,1], based on mipmaps, very fast
    R3D_Cubemap sky;        ///< Skybox asset (used if ID is non-zero)
    Quaternion rotation;    ///< Skybox rotation (pitch, yaw, roll as quaternion)
} R3D_EnvBackground;

/**
 * @brief Ambient lighting configuration.
 */
typedef struct R3D_EnvAmbient {
    Color color;            ///< Ambient light color when there is no ambient map
    float energy;           ///< Energy multiplier for ambient light (map or color)
    R3D_AmbientMap map;     ///< IBL environment map, can be generated from skybox
} R3D_EnvAmbient;

/**
 * @brief Screen Space Ambient Occlusion (SSAO) settings.
 *
 * Darkens areas where surfaces are close together, such as corners and crevices.
 */
typedef struct R3D_EnvSSAO {
    int sampleCount;        ///< Number of samples to compute SSAO (default: 16)
    float intensity;        ///< Base occlusion strength multiplier (default: 1.0)
    float power;            ///< Exponential falloff for sharper darkening (default: 1.5)
    float radius;           ///< Sampling radius in world space (default: 0.25)
    float bias;             ///< Depth bias to prevent self-shadowing, good value is ~2% of the radius (default: 0.007)
    bool enabled;           ///< Enable/disable SSAO effect (default: false)
} R3D_EnvSSAO;

/**
 * @brief Screen Space Indirect Lighting (SSIL) settings.
 *
 * Approximates indirect lighting by gathering light from nearby visible
 * surfaces in screen space.
 *
 * With a small radius, SSIL behaves like an extension of SSAO,
 * producing a very subtle local blending of light and surface hues.
 * With a larger radius, it becomes a better complement to SSGI,
 * reinforcing indirect lighting over a wider area.
 */
typedef struct R3D_EnvSSIL {
    int sampleCount;        ///< Number of samples to compute indirect lighting (default: 2)
    int sliceCount;         ///< Number of depth slices for accumulation (default: 4)
    float radius;           ///< Maximum distance to gather light from (default: 2.0)
    float thickness;        ///< Thickness threshold for occluders (default: 1.0)
    float intensity;        ///< IL intensity multiplier (default: 1.0)
    float aoPower;          ///< AO exponent/power (default: 1.0)
    int denoiseSteps;       ///< Number of denoiser iterations (default: 4)
    bool enabled;           ///< Enable/disable SSIL effect (default: false)
} R3D_EnvSSIL;

/**
 * @brief Screen Space Global Illumination (SSGI) settings.
 *
 * Real-time global illlumination calculated in screen space.
 * @note Best suited for enclosed/indoor environments.
 */
typedef struct R3D_EnvSSGI {
    int sampleCount;        ///< Number of rays per pixel (default: 8)
    int maxRaySteps;        ///< Maximum ray marching steps (default: 32)
    float stepSize;         ///< Ray step size (default: 0.125)
    float thickness;        ///< Depth tolerance for valid hits (default: 1.0)
    float maxDistance;      ///< Maximum ray distance (default: 4.0)
    float fadeStart;        ///< Distance at which the GI fade begins (default: 8.0)
    float fadeEnd;          ///< Distance at which GI is fully faded (default: 16.0)
    int denoiseSteps;       ///< Number of denoiser iterations (default: 5)
    bool enabled;           ///< Enable/disable SSGI (default: false)
} R3D_EnvSSGI;

/**
 * @brief Screen Space Reflections (SSR) settings.
 *
 * Real-time reflections calculated in screen space.
 */
typedef struct R3D_EnvSSR {
    int maxRaySteps;        ///< Maximum ray marching steps (default: 32)
    int binarySteps;        ///< Binary search refinement steps (default: 4)
    float stepSize;         ///< Ray step size (default: 0.125)
    float thickness;        ///< Depth tolerance for valid hits (default: 0.2)
    float maxDistance;      ///< Maximum ray distance (default: 4.0)
    float edgeFade;         ///< Screen edge fade start [0,1] (default: 0.25)
    bool enabled;           ///< Enable/disable SSR (default: false)
} R3D_EnvSSR;

/**
 * @brief Bloom post-processing settings.
 *
 * Glow effect around bright areas in the scene.
 */
typedef struct R3D_EnvBloom {
    R3D_Bloom mode;         ///< Bloom blending mode (default: R3D_BLOOM_DISABLED)
    float levels;           ///< Mipmap spread factor [0-1]: higher = wider glow (default: 0.5)
    float intensity;        ///< Bloom strength multiplier (default: 0.05)
    float threshold;        ///< Minimum brightness to trigger bloom (default: 0.0)
    float softThreshold;    ///< Softness of brightness cutoff transition (default: 0.5)
    float filterRadius;     ///< Blur filter radius during upscaling (default: 1.0)
} R3D_EnvBloom;

/**
 * @brief Fog atmospheric effect settings.
 */
typedef struct R3D_EnvFog {
    R3D_Fog mode;           ///< Fog distribution mode (default: R3D_FOG_DISABLED)
    Color color;            ///< Fog tint color (default: white)
    float start;            ///< Linear mode: distance where fog begins (default: 1.0)
    float end;              ///< Linear mode: distance of full fog density (default: 50.0)
    float density;          ///< Exponential modes: fog thickness factor (default: 0.05)
    float skyAffect;        ///< Fog influence on skybox [0-1] (default: 0.5)
} R3D_EnvFog;

/**
 * @brief Depth of Field (DoF) camera focus settings.
 *
 * Blurs objects outside the focal plane.
 */
typedef struct R3D_EnvDoF {
    R3D_DoF mode;           ///< Enable/disable state (default: R3D_DOF_DISABLED)
    float focusPoint;       ///< Focus distance in meters from camera (default: 10.0)
    float focusScale;       ///< Depth of field depth: lower = shallower (default: 1.0)
    float maxBlurSize;      ///< Maximum blur radius, similar to aperture (default: 20.0)
} R3D_EnvDoF;

/**
 * @brief Tone mapping and exposure settings.
 *
 * Converts HDR colors to displayable LDR range.
 */
typedef struct R3D_EnvTonemap {
    R3D_Tonemap mode;       ///< Tone mapping algorithm (default: R3D_TONEMAP_LINEAR)
    float exposure;         ///< Scene brightness multiplier (default: 1.0)
    float white;            ///< Reference white point (not used for AGX) (default: 1.0)
} R3D_EnvTonemap;

/**
 * @brief Color grading adjustments.
 *
 * Final color correction applied after all other effects.
 */
typedef struct R3D_EnvColor {
    float brightness;       ///< Overall brightness multiplier (default: 1.0)
    float contrast;         ///< Contrast between dark and bright areas (default: 1.0)
    float saturation;       ///< Color intensity (default: 1.0)
} R3D_EnvColor;

/**
 * @brief Complete environment configuration structure.
 *
 * Contains all rendering environment parameters: background, lighting, and post-processing effects.
 * Initialize with R3D_ENVIRONMENT_BASE for default values.
 */
typedef struct R3D_Environment {
    R3D_EnvBackground background;   ///< Background and skybox settings
    R3D_EnvAmbient    ambient;      ///< Ambient lighting configuration
    R3D_EnvSSAO       ssao;         ///< Screen space ambient occlusion
    R3D_EnvSSIL       ssil;         ///< Screen space indirect lighting
    R3D_EnvSSGI       ssgi;         ///< Screen space global illumination
    R3D_EnvSSR        ssr;          ///< Screen space reflections
    R3D_EnvBloom      bloom;        ///< Bloom glow effect
    R3D_EnvFog        fog;          ///< Atmospheric fog
    R3D_EnvDoF        dof;          ///< Depth of field focus effect
    R3D_EnvTonemap    tonemap;      ///< HDR tone mapping
    R3D_EnvColor      color;        ///< Color grading adjustments
} R3D_Environment;

// ========================================
// HELPER MACROS
// ========================================

/**
 * @brief Quick read access to environment members.
 *
 * @param member The environment member path (e.g., bloom.intensity)
 * @return The current value of the specified member
 *
 * Example: `float intensity = R3D_ENVIRONMENT_GET(bloom.intensity);`
 */
#define R3D_ENVIRONMENT_GET(member) (R3D_GetEnvironment()->member)

/**
 * @brief Quick write access to environment members.
 *
 * @param member The environment member path (e.g., bloom.intensity)
 * @param ... The value to assign
 *
 * Example: `R3D_ENVIRONMENT_SET(bloom.intensity, 0.05f);`
 */
#define R3D_ENVIRONMENT_SET(member, ...) (R3D_GetEnvironment()->member = (__VA_ARGS__))

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Retrieves a pointer to the current environment configuration.
 *
 * Provides direct read/write access to environment settings.
 * Modifications take effect immediately.
 *
 * @return Pointer to the active R3D_Environment structure
 */
R3DAPI R3D_Environment* R3D_GetEnvironment(void);

/**
 * @brief Replaces the entire environment configuration.
 *
 * Copies all settings from the provided structure to the active environment.
 * Useful for switching between presets or restoring saved states.
 *
 * @param env Pointer to the R3D_Environment structure to copy from
 */
R3DAPI void R3D_SetEnvironment(const R3D_Environment* env);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Environment

#endif // R3D_ENVIRONMENT_H
