/* r3d_environment.h -- R3D Environment Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ENVIRONMENT_H
#define R3D_ENVIRONMENT_H

#include "./r3d_skybox.h"
#include "./r3d_api.h"
#include <raylib.h>

/**
 * @defgroup Environment
 * @brief Mainly defines post process control.
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Bloom effect modes.
 *
 * Specifies different post-processing bloom techniques that can be applied
 * to the rendered scene. Bloom effects enhance the appearance of bright areas
 * by simulating light bleeding, contributing to a more cinematic and realistic look.
 */
 typedef enum R3D_Bloom {
    R3D_BLOOM_DISABLED,     ///< Bloom effect is disabled. The scene is rendered without any glow enhancement.
    R3D_BLOOM_MIX,          ///< Blends the bloom effect with the original scene using linear interpolation (Lerp).
    R3D_BLOOM_ADDITIVE,     ///< Adds the bloom effect additively to the scene, intensifying bright regions.
    R3D_BLOOM_SCREEN        ///< Combines the scene and bloom using screen blending, which brightens highlights
} R3D_Bloom;

/**
 * @brief Fog effect modes.
 *
 * Determines how fog is applied to the scene, affecting depth perception and atmosphere.
 */
typedef enum R3D_Fog {
    R3D_FOG_DISABLED, ///< Fog effect is disabled.
    R3D_FOG_LINEAR,   ///< Fog density increases linearly with distance from the camera.
    R3D_FOG_EXP2,     ///< Exponential fog (exp2), where density increases exponentially with distance.
    R3D_FOG_EXP       ///< Exponential fog, similar to EXP2 but with a different rate of increase.
} R3D_Fog;

/**
 * @brief Depth of field effect modes.
 *
 * Controls how depth of field is applied to the scene, affecting the focus and blur of objects.
 */
typedef enum R3D_Dof {
    R3D_DOF_DISABLED, ///< Depth of field effect is disabled.
    R3D_DOF_ENABLED,  ///< Depth of field effect is enabled.
} R3D_Dof;

/**
 * @brief Tone mapping modes.
 *
 * Controls how high dynamic range (HDR) colors are mapped to low dynamic range (LDR) for display.
 */
typedef enum R3D_Tonemap {
    R3D_TONEMAP_LINEAR,   ///< Simple linear mapping of HDR values.
    R3D_TONEMAP_REINHARD, ///< Reinhard tone mapping, a balanced method for compressing HDR values.
    R3D_TONEMAP_FILMIC,   ///< Filmic tone mapping, mimicking the response of photographic film.
    R3D_TONEMAP_ACES,     ///< ACES tone mapping, a high-quality cinematic rendering technique.
    R3D_TONEMAP_AGX,      ///< AGX tone mapping, a modern technique designed to preserve both highlight and shadow details for HDR rendering.
    R3D_TONEMAP_COUNT     ///< Number of tone mapping modes (used internally)
} R3D_Tonemap;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// ENVIRONMENT: Background And Ambient
// ----------------------------------------

/**
 * @brief Sets the background color when no skybox is enabled.
 *
 * This function defines the background color to be used when no skybox is active.
 * The color will be used for the clear color of the scene.
 *
 * @param color The color to set as the background color.
 */
R3DAPI void R3D_SetBackgroundColor(Color color);

/**
 * @brief Sets the ambient light color when no skybox is enabled.
 *
 * This function defines the ambient light color to be used when no skybox is active.
 * It affects the overall lighting of the scene when no skybox is present.
 *
 * @param color The color to set for ambient light.
 */
R3DAPI void R3D_SetAmbientColor(Color color);

/**
 * @brief Sets the energy level for ambient light when no skybox is enabled.
 *
 * This function defines the ambient light energy to be used when no skybox is active.
 * Applied multiplicatively to the base ambient color.
 *
 * @param energy The energy to set for ambient light.
 */
R3DAPI void R3D_SetAmbientEnergy(float energy);

/**
 * @brief Enables a skybox for the scene.
 *
 * This function enables a skybox in the scene, replacing the default background with
 * a 3D environment. The skybox is defined by the specified skybox asset.
 *
 * @param skybox The skybox to enable.
 */
R3DAPI void R3D_EnableSkybox(R3D_Skybox skybox);

/**
 * @brief Disables the skybox in the scene.
 *
 * This function disables the skybox, reverting back to the default background
 * color (or no background if none is set). It should be called to remove the skybox
 * from the scene.
 */
R3DAPI void R3D_DisableSkybox(void);

/**
 * @brief Sets the rotation of the skybox.
 *
 * This function allows you to specify the rotation of the skybox along the
 * pitch, yaw, and roll axes, which allows the skybox to be rotated in the scene.
 *
 * @param pitch The rotation angle around the X-axis (in degrees).
 * @param yaw The rotation angle around the Y-axis (in degrees).
 * @param roll The rotation angle around the Z-axis (in degrees).
 */
R3DAPI void R3D_SetSkyboxRotation(float pitch, float yaw, float roll);

/**
 * @brief Gets the current rotation of the skybox.
 *
 * This function returns the current rotation of the skybox as a vector containing
 * the pitch, yaw, and roll values in degrees.
 *
 * @return A vector containing the current pitch, yaw, and roll of the skybox.
 */
R3DAPI Vector3 R3D_GetSkyboxRotation(void);

/**
 * @brief Sets the intensity scaling values used for the environment's skybox.
 *
 * This function controls the intensity of both the rendered skybox as well as
 * the light that is generated from the skybox.
 *
 * @param background The intensity of the skybox rendered as the background.
 *                   A value of 0.0 will disable rendering the skybox but
 *                   allow any generated lighting to still be applied.
 * @param ambient The intensity of ambient light produced by the skybox.
 * @param reflection The intensity of reflections of the skybox in reflective materials.
 */
R3DAPI void R3D_SetSkyboxIntensity(float background, float ambient, float reflection);

/**
 * @brief Gets the intensity scaling values used for the environment's skybox.
 *
 * This function returns the intensity values for the rendered skybox as well
 * the light that is generated from the skybox.
 *
 * @param background Pointer to store the intensity value for the rendered skybox.
 * @param ambient Pointer to store the intensity value for ambient light produced by the skybox.
 * @param reflection Pointer to store the intensity value for reflections from the skybox.
 */
R3DAPI void R3D_GetSkyboxIntensity(float* background, float* ambient, float* reflection);

// ----------------------------------------
// ENVIRONMENT: SSAO Config Functions
// ----------------------------------------

/**
 * @brief Enables or disables Screen Space Ambient Occlusion (SSAO).
 *
 * This function toggles the SSAO effect. When enabled, SSAO enhances the realism
 * of the scene by simulating ambient occlusion, darkening areas where objects
 * are close together or in corners.
 *
 * @param enabled Whether to enable or disable SSAO.
 *
 * Default: false
 */
R3DAPI void R3D_SetSSAO(bool enabled);

/**
 * @brief Gets the current state of SSAO.
 *
 * This function checks if SSAO is currently enabled or disabled.
 *
 * @return True if SSAO is enabled, false otherwise.
 */
R3DAPI bool R3D_GetSSAO(void);

/**
 * @brief Sets the radius for SSAO effect.
 *
 * This function sets the radius used by the SSAO effect to calculate occlusion.
 * A higher value will affect a larger area around each pixel, while a smaller value
 * will create sharper and more localized occlusion.
 *
 * @param value The radius value to set for SSAO.
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetSSAORadius(float value);

/**
 * @brief Gets the current SSAO radius.
 *
 * This function retrieves the current radius value used by the SSAO effect.
 *
 * @return The radius value for SSAO.
 */
R3DAPI float R3D_GetSSAORadius(void);

/**
 * @brief Sets the bias for SSAO effect.
 *
 * This function sets the bias used by the SSAO effect to adjust how much occlusion
 * is applied to the scene. A higher value can reduce artifacts, but may also
 * result in less pronounced ambient occlusion.
 *
 * @param value The bias value for SSAO.
 *
 * Default: 0.025
 */
R3DAPI void R3D_SetSSAOBias(float value);

/**
 * @brief Gets the current SSAO bias.
 *
 * This function retrieves the current bias value used by the SSAO effect.
 *
 * @return The SSAO bias value.
 */
R3DAPI float R3D_GetSSAOBias(void);

/**
 * @brief Sets the number of blur iterations for the SSAO effect.
 *
 * This function sets the number of blur iterations applied to the SSAO effect.
 * By default, one iteration is performed, using a total of 12 samples for the
 * Gaussian blur. Increasing the number of iterations results in a smoother
 * ambient occlusion but may impact performance.
 *
 * @param value The number of blur iterations for SSAO.
 *
 * Default: 1
 */
R3DAPI void R3D_SetSSAOIterations(int value);

/**
 * @brief Gets the current number of blur iterations for the SSAO effect.
 *
 * This function retrieves the current number of blur iterations applied to the SSAO effect.
 *
 * @return The number of blur iterations for SSAO.
 */
R3DAPI int R3D_GetSSAOIterations(void);

/**
 * @brief Sets the intensity multiplier for the SSAO effect.
 *
 * This function sets the the base multiplier used by the SSAO effect.
 * Higher values will result in darker occlusion.
 *
 * @param value The intensity multiplier for SSAO.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSSAOIntensity(float value);

/**
 * @brief Gets the intensity multiplier for the SSAO effect.
 *
 * This function retrieves the intensity multiplier applied to the SSAO effect.
 *
 * @return The intensity multiplier for SSAO.
 */
R3DAPI float R3D_GetSSAOIntensity(void);

/**
 * @brief Sets the power factor for the SSAO effect.
 *
 * This function sets the exponential distributon applied to the SSAO effect.
 * Higher values will result in darker occlusion with an increasingly sharper
 * falloff compared to the SSAO intensity value.
 *
 * @param value The power factor for SSAO.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSSAOPower(float value);

/**
 * @brief Gets the power factor used for the SSAO effect.
 *
 * This function retrieves the exponential distributon value applied to the SSAO effect.
 *
 * @return The power factor for SSAO.
 */
R3DAPI float R3D_GetSSAOPower(void);

/**
 * @brief Controls the influence of SSAO on direct lighting.
 *
 * This function sets the amount of direct light attenuation from the SSAO effect.
 * Values greater than 0.0 will apply the SSAO effect to direct lighting with
 * increasing intensity at higher values. This is in addition to the typical
 * application to ambient light only.
 *
 * @param value SSAO effect intensity on direct light, in the range [0.0f, 1.0f].
 *
 * Default: 0.0
 */
R3DAPI void R3D_SetSSAOLightAffect(float value);

/**
 * @brief Gets the current direct lighting effect of SSAO.
 *
 * This function retrieves the value used for direct light attenuation from the SSAO effect.
 *
 * @return SSAO effect intensity on direct light, in the range [0.0f, 1.0f].
 */
R3DAPI float R3D_GetSSAOLightAffect(void);

// ----------------------------------------
// ENVIRONMENT: Bloom Config Functions
// ----------------------------------------

/**
 * @brief Sets the bloom mode.
 *
 * This function configures the bloom effect mode, which determines how the bloom
 * effect is applied to the rendered scene.
 *
 * @param mode The bloom mode to set.
 *
 * Default: R3D_BLOOM_DISABLED
 */
R3DAPI void R3D_SetBloomMode(R3D_Bloom mode);

/**
 * @brief Gets the current bloom mode.
 *
 * This function retrieves the bloom mode currently applied to the scene.
 *
 * @return The current bloom mode.
 */
R3DAPI R3D_Bloom R3D_GetBloomMode(void);

/**
 * @brief Sets the number of mipmap levels used for the bloom effect.
 *
 * This function controls how many mipmap level are generated for use in the bloom effect.
 * More levels will give a smoother and more widely dispersed effect, while less mipmaps
 * can provide a tighter effect. Setting this value to 0 will result in the maximum
 * possible amount of levels to be used. Use of this function will rebuild the
 * mipmaps and may give a one time performance hit.
 *
 * @param value The number of mipmap level to be used for the bloom effect.
 *
 * Default: 7
 */
R3DAPI void R3D_SetBloomLevels(int value);

/**
 * @brief Gets the current amount of mipmap levels used for the bloom effect.
 *
 * This function retrieves the current amount of mipmap levels in use by the bloom effect.
 *
 * @return The number of mipmap level currently used for the bloom effect.
 */
R3DAPI int R3D_GetBloomLevels(void);

/**
 * @brief Sets the bloom intensity.
 *
 * This function controls the strength of the bloom effect. Higher values result
 * in a more intense glow effect on bright areas of the scene.
 *
 * @param value The intensity value for bloom.
 *
 * Default: 0.05
 */
R3DAPI void R3D_SetBloomIntensity(float value);

/**
 * @brief Gets the current bloom intensity.
 *
 * This function retrieves the intensity value of the bloom effect.
 *
 * @return The current bloom intensity.
 */
R3DAPI float R3D_GetBloomIntensity(void);

/**
 * @brief Sets the bloom filter radius.
 *
 * Controls the radius of the blur filter applied during the upscaling stage
 * of the bloom effect. A larger radius results in a wider glow around bright
 * objects, creating a softer and more diffuse bloom. A value of 0 disables 
 * the filtering effect, preserving sharp bloom highlights.
 *
 * @param value The radius of the bloom filter (in pixels).
 *
 * Default: 0
 */
R3DAPI void R3D_SetBloomFilterRadius(int value);

 /**
  * @brief Gets the current bloom filter radius.
  *
  * Retrieves the current radius used for the bloom filter. This value determines
  * how far the glow effect extends around bright areas in the scene.
  *
  * @return The current bloom filter radius.
  */
R3DAPI int R3D_GetBloomFilterRadius(void);

/**
 * @brief Sets the bloom brightness threshold.
 *
 * Controls the brightness cutoff used during the downsampling stage of the
 * bloom effect. If the color channel with the brightest value is below the
 * set threshold the pixel will not be included in the bloom effect.
 *
 * @param value The lowest value to be included the bloom effect (in color value depending on implementation).
 *
 * Default: 0.0
 */
R3DAPI void R3D_SetBloomThreshold(float value);

/**
 * @brief Gets the bloom brightness threshold.
 *
 * Retrieves the current brightness cutoff used for the bloom effect. This value
 * determines if a pixel will be included in the bloom effect based on it's brightness.
 *
 * @return The current bloom brightness cutoff threshold.
 */
R3DAPI float R3D_GetBloomThreshold(void);

/**
 * @brief Sets the bloom brightness threshold's softness.
 *
 * Controls the softness of the cutoff between being include or excluded in the
 * bloom effect. A value of 0 will result in a hard transition between being
 * included or excluded, while larger values will give an increasingly
 * softer transition.
 *
 * @param value The value of of the bloom brightness threshold's softness.
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetBloomSoftThreshold(float value);

/**
 * @brief Gets the current bloom brightness threshold's softness.
 *
 * Retrieves the softness of the brightness cutoff for the bloom effect.
 * This value determines the softness of the transition between being
 * included or excluded in the bloom effect
 *
 * @return The current bloom brightness threshold's softness.
 */
R3DAPI float R3D_GetBloomSoftThreshold(void);

// ----------------------------------------
// ENVIRONMENT: SSR Config Functions
// ----------------------------------------

/**
 * @brief Enable or disable Screen Space Reflections (SSR).
 *
 * @param enabled Set to true to enable SSR, false to disable it.
 *
 * Default: false
 */
R3DAPI void R3D_SetSSR(bool enabled);

/**
 * @brief Check whether Screen Space Reflections (SSR) are enabled.
 *
 * @return true if SSR is enabled, false otherwise.
 */
R3DAPI bool R3D_GetSSR(void);

/**
 * @brief Set the maximum number of ray-marching steps for SSR.
 *
 * @param maxRaySteps The maximum number of steps taken while marching
 *        along the reflection ray. Higher values improve accuracy but
 *        increase GPU cost.
 *
 * Default: 64
 */
R3DAPI void R3D_SetSSRMaxRaySteps(int maxRaySteps);

/**
 * @brief Get the maximum number of ray-marching steps for SSR.
 *
 * @return The maximum ray-marching steps.
 */
R3DAPI int R3D_GetSSRMaxRaySteps(void);

/**
 * @brief Set the number of refinement steps for the binary search phase.
 *
 * @param binarySearchSteps The number of iterations used to refine
 *        the ray-surface intersection point after a hit is detected.
 *        More steps yield a more precise intersection.
 *
 * Default: 8
 */
R3DAPI void R3D_SetSSRBinarySearchSteps(int binarySearchSteps);

/**
 * @brief Get the number of refinement steps for the binary search phase.
 *
 * @return The number of binary search steps.
 */
R3DAPI int R3D_GetSSRBinarySearchSteps(void);

/**
 * @brief Set the maximum ray marching distance in view space units.
 *
 * @param rayMarchLength The maximum distance a reflection ray can travel.
 *        Larger values allow longer reflections but may cause artifacts.
 *
 * Default: 8.0
 */
R3DAPI void R3D_SetSSRRayMarchLength(float rayMarchLength);

/**
 * @brief Get the maximum ray marching distance.
 *
 * @return The maximum ray marching distance.
 */
R3DAPI float R3D_GetSSRRayMarchLength(void);

/**
 * @brief Set the SSR depth thickness tolerance.
 *
 * @param depthThickness The maximum depth difference allowed between
 *        the ray position and the scene depth to consider a valid hit.
 *        Larger values increase tolerance but can cause ghosting.
 *
 * Default: 0.2
 */
R3DAPI void R3D_SetSSRDepthThickness(float depthThickness);

/**
 * @brief Get the SSR depth thickness tolerance.
 *
 * @return The depth thickness value.
 */
R3DAPI float R3D_GetSSRDepthThickness(void);

/**
 * @brief Set the SSR depth tolerance.
 *
 * @param depthTolerance The negative margin allowed when comparing the
 *        ray position against the scene depth. This prevents false negatives
 *        due to floating-point errors or slight inconsistencies in depth
 *        reconstruction.
 *
 * In practice, a hit is accepted if:
 *    -depthTolerance <= depthDiff < depthThickness
 *
 * Smaller values increase strictness but may cause missed intersections,
 * while larger values reduce artifacts but can introduce ghosting.
 *
 * Default: 0.005
 */
R3DAPI void R3D_SetSSRDepthTolerance(float depthTolerance);

/**
 * @brief Get the SSR depth tolerance.
 *
 * @return The depth tolerance value.
 */
R3DAPI float R3D_GetSSRDepthTolerance(void);

/**
 * @brief Set the fade range near the screen edges to reduce artifacts.
 *
 * @param start Normalized distance from the screen center where edge fading begins (0.0–1.0).
 * @param end   Normalized distance where fading is complete (0.0–1.0).
 *
 * Pixels outside this range will have their reflections gradually
 * faded out to avoid hard cutoffs near the borders.
 *
 * Default: start = 0.7, end = 1.0
 */
R3DAPI void R3D_SetSSRScreenEdgeFade(float start, float end);

/**
 * @brief Get the screen edge fade range.
 *
 * @param start Pointer to receive the fade start value.
 * @param end   Pointer to receive the fade end value.
 */
R3DAPI void R3D_GetSSRScreenEdgeFade(float* start, float* end);

// ----------------------------------------
// ENVIRONMENT: Fog Config Functions
// ----------------------------------------

/**
 * @brief Sets the fog mode.
 *
 * This function defines the type of fog effect applied to the scene.
 * Different modes may provide linear, exponential, or volumetric fog effects.
 *
 * @param mode The fog mode to set.
 *
 * Default: R3D_FOG_DISABLED
 */
R3DAPI void R3D_SetFogMode(R3D_Fog mode);

/**
 * @brief Gets the current fog mode.
 *
 * This function retrieves the fog mode currently applied to the scene.
 *
 * @return The current fog mode.
 */
R3DAPI R3D_Fog R3D_GetFogMode(void);

/**
 * @brief Sets the color of the fog.
 *
 * This function defines the color of the fog effect applied to the scene.
 * The fog color blends with objects as they are affected by fog.
 *
 * @param color The color to set for the fog.
 *
 * Default: WHITE
 */
R3DAPI void R3D_SetFogColor(Color color);

/**
 * @brief Gets the current fog color.
 *
 * This function retrieves the color currently used for the fog effect.
 *
 * @return The current fog color.
 */
R3DAPI Color R3D_GetFogColor(void);

/**
 * @brief Sets the start distance of the fog.
 *
 * This function defines the distance from the camera at which fog begins to appear.
 * Objects closer than this distance will not be affected by fog.
 *
 * @param value The start distance for the fog effect.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetFogStart(float value);

/**
 * @brief Gets the current fog start distance.
 *
 * This function retrieves the distance at which the fog begins to be applied.
 *
 * @return The current fog start distance.
 */
R3DAPI float R3D_GetFogStart(void);

/**
 * @brief Sets the end distance of the fog.
 *
 * This function defines the distance from the camera at which fog reaches full intensity.
 * Objects beyond this distance will be completely covered by fog.
 *
 * @param value The end distance for the fog effect.
 *
 * Default: 50.0
 */
R3DAPI void R3D_SetFogEnd(float value);

/**
 * @brief Gets the current fog end distance.
 *
 * This function retrieves the distance at which the fog is fully applied.
 *
 * @return The current fog end distance.
 */
R3DAPI float R3D_GetFogEnd(void);

/**
 * @brief Sets the density of the fog.
 *
 * This function controls how thick the fog appears. Higher values result in
 * denser fog, making objects fade out more quickly.
 *
 * @param value The density of the fog (higher values increase fog thickness).
 *
 * Default: 0.05
 */
R3DAPI void R3D_SetFogDensity(float value);

/**
 * @brief Gets the current fog density.
 *
 * This function retrieves the current density of the fog.
 *
 * @return The current fog density.
 */
R3DAPI float R3D_GetFogDensity(void);

/**
 * @brief Sets how much the fog affects the sky.
 *
 * This function controls the influence of fog on the sky color and visibility. 
 * A higher value makes the fog blend more strongly with the sky, reducing its clarity.
 *
 * @param value The fog effect on the sky, in the range [0.0f, 1.0f] 
 *              (0 = no effect, 1 = maximum blending).
 *
 * Default: 0.5
 */
R3DAPI void R3D_SetFogSkyAffect(float value);

/**
 * @brief Gets the current fog effect on the sky.
 *
 * This function retrieves the current influence of fog on the sky.
 *
 * @return The current fog-sky affect value, in the range [0.0f, 1.0f].
 */
R3DAPI float R3D_GetFogSkyAffect(void);

// ----------------------------------------
// ENVIRONMENT: Depth of Field (DoF) Functions
// ----------------------------------------

/**
 * @brief Enables or disables the depth of field post-process.
 *
 * @param mode The depth of field mode to set.
 *
 * Default: R3D_DOF_DISABLED
 */
R3DAPI void R3D_SetDofMode(R3D_Dof mode);

/**
 * @brief Gets the current depth of field mode.
 *
 * @return The current depth of field mode.
 */
R3DAPI R3D_Dof R3D_GetDofMode(void);

/**
 * @brief Sets the focus point in world space.
 *
 * This function defines the distance (in meters) from the camera where
 * objects will be in perfect focus. Objects closer or farther will be blurred.
 *
 * @param value The focus point distance in meters.
 *
 * Default: 10.0
 */
R3DAPI void R3D_SetDofFocusPoint(float value);

/**
 * @brief Gets the current focus point.
 *
 * @return The focus point distance in meters.
 */
R3DAPI float R3D_GetDofFocusPoint(void);

/**
 * @brief Sets the focus scale.
 *
 * This function controls how shallow the depth of field effect is.
 * Lower values create a shallower depth of field with more blur,
 * while higher values create a deeper depth of field with less blur.
 *
 * @param value The focus scale value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetDofFocusScale(float value);

/**
 * @brief Gets the current focus scale.
 *
 * @return The current focus scale value.
 */
R3DAPI float R3D_GetDofFocusScale(void);

/**
 * @brief Sets the maximum blur size.
 *
 * This function controls the maximum amount of blur applied to out-of-focus
 * areas. This value is similar to the lens aperture size, larger values
 * create more pronounced blur effects.
 *
 * @param value The maximum blur size value.
 *
 * Default: 20.0
 */
R3DAPI void R3D_SetDofMaxBlurSize(float value);

/**
 * @brief Gets the current maximum blur size.
 *
 * @return The current maximum blur size value.
 */
R3DAPI float R3D_GetDofMaxBlurSize(void);

/**
 * @brief Enables or disables depth-of-field debug mode.
 *
 * In debug mode, the scene uses color coding:
 * - Green: near blur
 * - Black: sharp areas
 * - Blue: far blur
 *
 * @param enabled true to enable, false to disable.
 *
 * Default: false
 */
R3DAPI void R3D_SetDofDebugMode(bool enabled);

/**
 * @brief Gets the current debug mode state.
 *
 * @return True if debug mode is enabled, false otherwise.
 */
R3DAPI bool R3D_GetDofDebugMode(void);

// ----------------------------------------
// ENVIRONMENT: Tonemap Config Functions
// ----------------------------------------

/**
 * @brief Sets the tonemapping mode.
 *
 * This function defines the tonemapping algorithm applied to the final rendered image.
 * Different tonemap modes affect color balance, brightness compression, and overall
 * scene appearance.
 *
 * @param mode The tonemap mode to set.
 *
 * Default: R3D_TONEMAP_LINEAR
 */
R3DAPI void R3D_SetTonemapMode(R3D_Tonemap mode);

/**
 * @brief Gets the current tonemapping mode.
 *
 * This function retrieves the tonemap mode currently applied to the scene.
 *
 * @return The current tonemap mode.
 */
R3DAPI R3D_Tonemap R3D_GetTonemapMode(void);

/**
 * @brief Sets the exposure level for tonemapping.
 *
 * This function adjusts the exposure level used in tonemapping, affecting
 * the overall brightness of the rendered scene.
 *
 * @param value The exposure value (higher values make the scene brighter).
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetTonemapExposure(float value);

/**
 * @brief Gets the current tonemap exposure level.
 *
 * This function retrieves the current exposure setting used in tonemapping.
 *
 * @return The current tonemap exposure value.
 */
R3DAPI float R3D_GetTonemapExposure(void);

/**
 * @brief Sets the white point for tonemapping.
 *
 * This function defines the reference white level, which determines how bright
 * areas of the scene are mapped to the final output.
 *
 * @param value The white point value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetTonemapWhite(float value);

/**
 * @brief Gets the current tonemap white point.
 *
 * This function retrieves the white point setting used in tonemapping.
 *
 * @return The current tonemap white value.
 */
R3DAPI float R3D_GetTonemapWhite(void);

// ----------------------------------------
// ENVIRONMENT: Color Adjustment Functions
// ----------------------------------------

/**
 * @brief Sets the global brightness adjustment.
 *
 * This function controls the brightness of the final rendered image.
 * Higher values make the image brighter, while lower values darken it.
 *
 * @param value The brightness adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetBrightness(float value);

/**
 * @brief Gets the current brightness level.
 *
 * This function retrieves the brightness setting applied to the scene.
 *
 * @return The current brightness value.
 */
R3DAPI float R3D_GetBrightness(void);

/**
 * @brief Sets the global contrast adjustment.
 *
 * This function controls the contrast of the final rendered image.
 * Higher values increase the difference between dark and bright areas.
 *
 * @param value The contrast adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetContrast(float value);

/**
 * @brief Gets the current contrast level.
 *
 * This function retrieves the contrast setting applied to the scene.
 *
 * @return The current contrast value.
 */
R3DAPI float R3D_GetContrast(void);

/**
 * @brief Sets the global saturation adjustment.
 *
 * This function controls the color intensity of the final rendered image.
 * Higher values make colors more vibrant, while lower values desaturate them.
 *
 * @param value The saturation adjustment value.
 *
 * Default: 1.0
 */
R3DAPI void R3D_SetSaturation(float value);

/**
 * @brief Gets the current saturation level.
 *
 * This function retrieves the saturation setting applied to the scene.
 *
 * @return The current saturation value.
 */
R3DAPI float R3D_GetSaturation(void);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Environment

#endif // R3D_ENVIRONMENT_H
