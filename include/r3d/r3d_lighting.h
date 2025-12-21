/* r3d_lighting.h -- R3D Lighting Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_LIGHTING_H
#define R3D_LIGHTING_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Lighting
 * @{
 */

// ========================================
// ENUMS TYPES
// ========================================

/**
 * @brief Types of lights supported by the rendering engine.
 *
 * Each light type has different behaviors and use cases.
 */
typedef enum R3D_LightType {
    R3D_LIGHT_DIR,                      ///< Directional light, affects the entire scene with parallel rays.
    R3D_LIGHT_SPOT,                     ///< Spot light, emits light in a cone shape.
    R3D_LIGHT_OMNI                      ///< Omni light, emits light in all directions from a single point.
} R3D_LightType;

/**
 * @brief Modes for updating shadow maps.
 *
 * Determines how often the shadow maps are refreshed.
 */
typedef enum R3D_ShadowUpdateMode {
    R3D_SHADOW_UPDATE_MANUAL,           ///< Shadow maps update only when explicitly requested.
    R3D_SHADOW_UPDATE_INTERVAL,         ///< Shadow maps update at defined time intervals.
    R3D_SHADOW_UPDATE_CONTINUOUS        ///< Shadow maps update every frame for real-time accuracy.
} R3D_ShadowUpdateMode;

// ========================================
// ALIASES TYPES
// ========================================

/**
 * @brief Unique identifier for an R3D light.
 *
 * ID type used to reference a light.
 * A negative value indicates an invalid light.
 */
typedef int32_t R3D_Light;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// LIGHTING: Lights Config Functions
// ----------------------------------------

/**
 * @brief Creates a new light of the specified type.
 *
 * This function creates a light of the given type. The light must be destroyed
 * manually when no longer needed by calling `R3D_DestroyLight`.
 *
 * @param type The type of light to create (directional, spot or omni-directional).
 * @return The ID of the created light.
 */
R3DAPI R3D_Light R3D_CreateLight(R3D_LightType type);

/**
 * @brief Destroys the specified light.
 *
 * This function deallocates the resources associated with the light and makes
 * the light ID invalid. It must be called after the light is no longer needed.
 *
 * @param id The ID of the light to destroy.
 */
R3DAPI void R3D_DestroyLight(R3D_Light id);

/**
 * @brief Checks if a light exists.
 *
 * This function checks if the specified light ID is valid and if the light exists.
 *
 * @param id The ID of the light to check.
 * @return True if the light exists, false otherwise.
 */
R3DAPI bool R3D_IsLightExist(R3D_Light id);

/**
 * @brief Gets the type of a light.
 *
 * This function returns the type of the specified light (directional, spot or omni-directional).
 *
 * @param id The ID of the light.
 * @return The type of the light.
 */
R3DAPI R3D_LightType R3D_GetLightType(R3D_Light id);

/**
 * @brief Checks if a light is active.
 *
 * This function checks whether the specified light is currently active (enabled or disabled).
 *
 * @param id The ID of the light to check.
 * @return True if the light is active, false otherwise.
 */
R3DAPI bool R3D_IsLightActive(R3D_Light id);

/**
 * @brief Toggles the state of a light (active or inactive).
 *
 * This function toggles the state of the specified light, turning it on if it is off,
 * or off if it is on.
 *
 * @param id The ID of the light to toggle.
 */
R3DAPI void R3D_ToggleLight(R3D_Light id);

/**
 * @brief Sets the active state of a light.
 *
 * This function allows manually turning a light on or off by specifying its active state.
 *
 * @param id The ID of the light to set the active state for.
 * @param active True to activate the light, false to deactivate it.
 */
R3DAPI void R3D_SetLightActive(R3D_Light id, bool active);

/**
 * @brief Gets the color of a light.
 *
 * This function retrieves the color of the specified light as a `Color` structure.
 *
 * @param id The ID of the light.
 * @return The color of the light as a `Color` structure.
 */
R3DAPI Color R3D_GetLightColor(R3D_Light id);

/**
 * @brief Gets the color of a light as a `Vector3`.
 *
 * This function retrieves the color of the specified light as a `Vector3`, where each
 * component (x, y, z) represents the RGB values of the light.
 *
 * @param id The ID of the light.
 * @return The color of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightColorV(R3D_Light id);

/**
 * @brief Sets the color of a light.
 *
 * This function sets the color of the specified light using a `Color` structure.
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light.
 */
R3DAPI void R3D_SetLightColor(R3D_Light id, Color color);

/**
 * @brief Sets the color of a light using a `Vector3`.
 *
 * This function sets the color of the specified light using a `Vector3`, where each
 * component (x, y, z) represents the RGB values of the light.
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light as a `Vector3`.
 */
R3DAPI void R3D_SetLightColorV(R3D_Light id, Vector3 color);

/**
 * @brief Gets the position of a light.
 *
 * This function retrieves the position of the specified light.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The position of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightPosition(R3D_Light id);

/**
 * @brief Sets the position of a light.
 *
 * This function sets the position of the specified light.
 * Only applicable to spot lights or omni-lights.
 *
 * @note Has no effect for directional lights.
 *       If called on a directional light, 
 *       a warning will be logged.
 *
 * @param id The ID of the light.
 * @param position The new position to set for the light.
 */
R3DAPI void R3D_SetLightPosition(R3D_Light id, Vector3 position);

/**
 * @brief Gets the direction of a light.
 *
 * This function retrieves the direction of the specified light.
 * Only applicable to directional lights or spot lights.
 *
 * @param id The ID of the light.
 * @return The direction of the light as a `Vector3`.
 */
R3DAPI Vector3 R3D_GetLightDirection(R3D_Light id);

/**
 * @brief Sets the direction of a light.
 *
 * This function sets the direction of the specified light.
 * Only applicable to directional lights or spot lights.
 *
 * @note Has no effect for omni-directional lights.
 *       If called on an omni-directional light,
 *       a warning will be logged.
 *
 * @param id The ID of the light.
 * @param direction The new direction to set for the light.
 *                  The vector is automatically normalized.
 */
R3DAPI void R3D_SetLightDirection(R3D_Light id, Vector3 direction);

/**
 * @brief Sets the position and direction of a light to look at a target point.
 *
 * This function sets both the position and the direction of the specified light,
 * causing it to "look at" a given target point.
 *
 * @note - For directional lights, only the direction is updated (position is ignored).
 *       - For omni-directional lights, only the position is updated (direction is not calculated).
 *       - For spot lights, both position and direction are set accordingly.
 *       - This function does **not** emit any warning or log message.
 *
 * @param id The ID of the light.
 * @param position The position to set for the light.
 * @param target The point the light should look at.
 */
R3DAPI void R3D_LightLookAt(R3D_Light id, Vector3 position, Vector3 target);

/**
 * @brief Gets the energy level of a light.
 *
 * This function retrieves the energy level (intensity) of the specified light.
 * Energy typically affects the brightness of the light.
 *
 * @param id The ID of the light.
 * @return The energy level of the light.
 */
R3DAPI float R3D_GetLightEnergy(R3D_Light id);

/**
 * @brief Sets the energy level of a light.
 *
 * This function sets the energy (intensity) of the specified light.
 * A higher energy value will result in a brighter light.
 *
 * @param id The ID of the light.
 * @param energy The new energy value to set for the light.
 */
R3DAPI void R3D_SetLightEnergy(R3D_Light id, float energy);

/**
 * @brief Gets the specular intensity of a light.
 *
 * This function retrieves the current specular intensity of the specified light.
 * Specular intensity affects how shiny surfaces appear when reflecting the light.
 *
 * @param id The ID of the light.
 * @return The current specular intensity of the light.
 */
R3DAPI float R3D_GetLightSpecular(R3D_Light id);

/**
 * @brief Sets the specular intensity of a light.
 *
 * This function sets the specular intensity of the specified light.
 * Higher specular values result in stronger and sharper highlights on reflective surfaces.
 *
 * @param id The ID of the light.
 * @param specular The new specular intensity value to set for the light.
 */
R3DAPI void R3D_SetLightSpecular(R3D_Light id, float specular);

/**
 * @brief Gets the range of a light.
 *
 * This function retrieves the range of the specified light, which determines how far the light can affect.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The range of the light.
 */
R3DAPI float R3D_GetLightRange(R3D_Light id);

/**
 * @brief Sets the range parameter of a light.
 *
 * For spot and omni lights, this defines the maximum illumination distance.
 * For directional lights, this defines the shadow rendering radius around the camera.
 *
 * @param id The ID of the light.
 * @param range The range value to apply.
 */
R3DAPI void R3D_SetLightRange(R3D_Light id, float range);

/**
 * @brief Gets the attenuation factor of a light.
 *
 * This function retrieves the attenuation factor of the specified light.
 * Attenuation controls how the intensity of a light decreases with distance.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @return The attenuation factor of the light.
 */
R3DAPI float R3D_GetLightAttenuation(R3D_Light id);

/**
 * @brief Sets the attenuation factor of a light.
 *
 * This function sets the attenuation factor of the specified light.
 * A higher attenuation value causes the light to lose intensity more quickly as the distance increases.
 * For a realistic effect, an attenuation factor of 2.0f is typically used.
 * Only applicable to spot lights or omni-lights.
 *
 * @param id The ID of the light.
 * @param attenuation The new attenuation factor to set for the light.
 */
R3DAPI void R3D_SetLightAttenuation(R3D_Light id, float attenuation);

/**
 * @brief Gets the inner cutoff angle of a spotlight.
 *
 * This function retrieves the inner cutoff angle of a spotlight.
 * The inner cutoff defines the cone of light where the light is at full intensity.
 *
 * @param id The ID of the light.
 * @return The inner cutoff angle in degrees of the spotlight.
 */
R3DAPI float R3D_GetLightInnerCutOff(R3D_Light id);

/**
 * @brief Sets the inner cutoff angle of a spotlight.
 *
 * This function sets the inner cutoff angle of a spotlight.
 * The inner cutoff angle defines the cone where the light is at full intensity.
 * Anything outside this cone starts to fade.
 *
 * @param id The ID of the light.
 * @param degrees The new inner cutoff angle in degrees.
 */
R3DAPI void R3D_SetLightInnerCutOff(R3D_Light id, float degrees);

/**
 * @brief Gets the outer cutoff angle of a spotlight.
 *
 * This function retrieves the outer cutoff angle of a spotlight.
 * The outer cutoff defines the outer boundary of the light's cone, where the light starts to fade.
 *
 * @param id The ID of the light.
 * @return The outer cutoff angle in degrees of the spotlight.
 */
R3DAPI float R3D_GetLightOuterCutOff(R3D_Light id);

/**
 * @brief Sets the outer cutoff angle of a spotlight.
 *
 * This function sets the outer cutoff angle of a spotlight.
 * The outer cutoff defines the boundary of the light's cone where the light intensity starts to gradually decrease.
 *
 * @param id The ID of the light.
 * @param degrees The new outer cutoff angle in degrees.
 */
R3DAPI void R3D_SetLightOuterCutOff(R3D_Light id, float degrees);

// ----------------------------------------
// LIGHTING: Shadow Config Functions
// ----------------------------------------

/**
 * @brief Enables shadow casting for a light and sets its shadow map resolution.
 *
 * Allocates a shadow map at the given resolution and enables shadow rendering.
 * This function can be called multiple times to resize the shadow map, but doing so
 * reallocates GPU resources and has a performance cost.
 *
 * @param id The ID of the light.
 * @param resolution The shadow map resolution.
 *
 * @note Shadow maps are memory-intensive. High resolutions (e.g. 4K) consume significant
 * GPU memory; creating many high-resolution shadow maps may lead crashes...
 */
R3DAPI void R3D_EnableShadow(R3D_Light id, int resolution);

/**
 * @brief Disables shadow casting for a light.
 *
 * Disables shadow rendering for the light. The shadow map is preserved in memory,
 * and may be reused later, even if the light itself is destroyed.
 *
 * @param id The ID of the light.
 */
R3DAPI void R3D_DisableShadow(R3D_Light id);

/**
 * @brief Checks if shadow casting is enabled for a light.
 *
 * This function checks if shadow casting is currently enabled for the specified light.
 *
 * @param id The ID of the light.
 * @return True if shadow casting is enabled, false otherwise.
 */
R3DAPI bool R3D_IsShadowEnabled(R3D_Light id);

/**
 * @brief Gets the shadow map update mode of a light.
 *
 * This function retrieves the current mode for updating the shadow map of a light. The mode can be:
 * - Interval: Updates the shadow map at a fixed interval.
 * - Continuous: Updates the shadow map continuously.
 * - Manual: Updates the shadow map manually (via explicit function calls).
 *
 * @param id The ID of the light.
 * @return The shadow map update mode.
 */
R3DAPI R3D_ShadowUpdateMode R3D_GetShadowUpdateMode(R3D_Light id);

/**
 * @brief Sets the shadow map update mode of a light.
 *
 * This function sets the mode for updating the shadow map of the specified light.
 * The update mode controls when and how often the shadow map is refreshed.
 *
 * @param id The ID of the light.
 * @param mode The update mode to set for the shadow map (Interval, Continuous, or Manual).
 */
R3DAPI void R3D_SetShadowUpdateMode(R3D_Light id, R3D_ShadowUpdateMode mode);

/**
 * @brief Gets the frequency of shadow map updates for the interval update mode.
 *
 * This function retrieves the frequency (in milliseconds) at which the shadow map should be updated when
 * the interval update mode is enabled. This function is only relevant if the shadow map update mode is set
 * to "Interval".
 *
 * @param id The ID of the light.
 * @return The frequency in milliseconds at which the shadow map is updated.
 */
R3DAPI int R3D_GetShadowUpdateFrequency(R3D_Light id);

/**
 * @brief Sets the frequency of shadow map updates for the interval update mode.
 *
 * This function sets the frequency (in milliseconds) at which the shadow map should be updated when
 * the interval update mode is enabled. This function is only relevant if the shadow map update mode is set
 * to "Interval".
 *
 * @param id The ID of the light.
 * @param msec The frequency in milliseconds at which to update the shadow map.
 */
R3DAPI void R3D_SetShadowUpdateFrequency(R3D_Light id, int msec);

/**
 * @brief Forces an immediate update of the shadow map during the next rendering pass.
 *
 * This function forces the shadow map of the specified light to be updated during the next call to `R3D_End`.
 * This is primarily used for the manual update mode, but may also work for the interval mode.
 *
 * @param id The ID of the light.
 */
R3DAPI void R3D_UpdateShadowMap(R3D_Light id);

/**
 * @brief Retrieves the softness radius used to simulate penumbra in shadows.
 *
 * The softness is expressed as a sampling radius in texels within the shadow map.
 *
 * @param id The ID of the light.
 * @return The softness radius in texels currently set for the shadow.
 */
R3DAPI float R3D_GetShadowSoftness(R3D_Light id);

/**
 * @brief Sets the softness radius used to simulate penumbra in shadows.
 *
 * This function adjusts the softness of the shadow edges for the specified light.
 * The softness value corresponds to a number of texels in the shadow map, independent
 * of its resolution. Larger values increase the blur radius, resulting in softer,
 * more diffuse shadows, while smaller values yield sharper shadows.
 *
 * @param id The ID of the light.
 * @param softness The softness radius in texels to apply (must be >= 0).
 *
 * @note The softness must be set only after shadows have been enabled for the light,
 *       since the shadow map resolution must be known before the softness can be applied.
 */
R3DAPI void R3D_SetShadowSoftness(R3D_Light id, float softness);

/**
 * @brief Gets the shadow depth bias value.
 */
R3DAPI float R3D_GetShadowDepthBias(R3D_Light id);

/**
 * @brief Sets the shadow depth bias value.
 *
 * A higher bias helps reduce "shadow acne" artifacts
 * (shadows flickering or appearing misaligned on surfaces).
 * Be careful: too large values may cause shadows to look detached
 * or floating away from objects.
 */
R3DAPI void R3D_SetShadowDepthBias(R3D_Light id, float value);

/**
 * @brief Gets the shadow slope bias value.
 */
R3DAPI float R3D_GetShadowSlopeBias(R3D_Light id);

/**
 * @brief Sets the shadow slope bias value.
 *
 * This bias mainly compensates artifacts on surfaces angled
 * relative to the light. It helps prevent shadows from
 * incorrectly appearing or disappearing along object edges.
 */
R3DAPI void R3D_SetShadowSlopeBias(R3D_Light id, float value);

// ----------------------------------------
// LIGHTING: Light Helper Functions
// ----------------------------------------

/**
 * @brief Returns the bounding box encompassing the light's area of influence.
 *
 * This function computes the axis-aligned bounding box (AABB) that encloses the 
 * volume affected by the specified light, based on its type:
 * 
 * - For spotlights, the bounding box encloses the light cone.
 * - For omni-directional lights, it encloses a sphere representing the light's range.
 * - For directional lights, it returns an infinite bounding box to represent global influence.
 *
 * This bounding box is primarily useful for spatial partitioning, culling, or visual debugging.
 * 
 * @param light The light for which to compute the bounding box.
 * 
 * @return A BoundingBox struct that encloses the light's influence volume.
 */
R3DAPI BoundingBox R3D_GetLightBoundingBox(R3D_Light light);

/**
 * @brief Draws the area of influence of the light in 3D space.
 *
 * This function visualizes the area affected by a light in 3D space.
 * It draws the light's influence, such as the cone for spotlights or the volume for omni-lights.
 * This function is only relevant for spotlights and omni-lights.
 * 
 * @note This function should be called while using the default 3D rendering mode of raylib, 
 *       not with R3D's rendering mode. It uses raylib's 3D drawing functions to render the light's shape.
 *
 * @param id The ID of the light.
 */
R3DAPI void R3D_DrawLightShape(R3D_Light id);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Lighting

#endif // R3D_LIGHTING_H
