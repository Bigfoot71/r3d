/* r3d_lighting.h -- R3D Lighting Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_LIGHTING_H
#define R3D_LIGHTING_H

#include "./r3d_platform.h"
#include "./r3d_camera.h"
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
    R3D_LIGHT_OMNI,                     ///< Omni light, emits light in all directions from a single point.
    R3D_LIGHT_TYPE_COUNT
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
// HANDLES TYPES
// ========================================

/**
 * @brief Unique identifier for an R3D light.
 *
 * ID type used to reference a light.
 * A zero value indicates an invalid light.
 */
typedef uint32_t R3D_Light;

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
R3DAPI bool R3D_IsLightValid(R3D_Light id);

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
 * @brief Returns whether a light is currently enabled.
 *
 * @param id The ID of the light to query.
 * @return True if the light is enabled, false otherwise.
 */
R3DAPI bool R3D_IsLightEnabled(R3D_Light id);

/**
 * @brief Toggles a light between enabled and disabled states.
 *
 * @param id The ID of the light to toggle.
 */
R3DAPI void R3D_ToggleLight(R3D_Light id);

/**
 * @brief Enables a light.
 *
 * Has no effect if the light is already enabled.
 *
 * @param id The ID of the light to enable.
 */
R3DAPI void R3D_EnableLight(R3D_Light id);

/**
 * @brief Disables a light.
 *
 * Has no effect if the light is already disabled.
 *
 * @param id The ID of the light to disable.
 */
R3DAPI void R3D_DisableLight(R3D_Light id);

/**
 * @brief Gets the color of a light in sRGB space.
 *
 * The internal linear color is converted to sRGB before being returned.
 *
 * @param id The ID of the light.
 * @return The color of the light as an sRGB @ref Color.
 */
R3DAPI Color R3D_GetLightColor(R3D_Light id);

/**
 * @brief Sets the color of a light from an sRGB color.
 *
 * The provided sRGB color is converted to linear space before being stored internally.
 *
 * Default: (Color) {255, 255, 255, 255}
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light, in sRGB space.
 */
R3DAPI void R3D_SetLightColor(R3D_Light id, Color color);

/**
 * @brief Gets the color of a light in linear space.
 *
 * Returns the raw internal color without any color space conversion.
 *
 * @param id The ID of the light.
 * @return The color of the light as a linear RGB @ref Vector3.
 */
R3DAPI Vector3 R3D_GetLightColorLinear(R3D_Light id);

/**
 * @brief Sets the color of a light from a linear RGB color.
 *
 * The provided value is stored as-is, without any color space conversion.
 *
 * Default: (Vector3) {1.0f, 1.0f, 1.0f}
 *
 * @param id The ID of the light.
 * @param color The new color to set for the light, in linear space.
 */
R3DAPI void R3D_SetLightColorLinear(R3D_Light id, Vector3 color);

/**
 * @brief Sets the color of a light from a color temperature.
 *
 * Converts the given temperature to an sRGB color using the Tanner Helland approximation,
 * then applies the appropriate sRGB-to-linear conversion before storing it internally.
 *
 * @param id The ID of the light.
 * @param kelvin The color temperature in Kelvin. Valid range is 1000K to 40000K.
 */
R3DAPI void R3D_SetLightTemperature(R3D_Light id, float kelvin);

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
 * Default: (Vector3) {0.0f, 0.0f, 0.0f}
 *
 * @note For directional lights, the position is stored internally but has no effect
 *       on lighting calculations. It can still be retrieved via @ref R3D_GetLightPosition
 *       and may be used for debug visualization with @ref R3D_DrawLightDebug.
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
 * Default: (Vector3) {0.0f, 0.0f, -1.0f}
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
 * @note - For directional lights, only the direction is updated. The position is stored
 *         internally but has no effect on lighting calculations; it may still be used
 *         for debug visualization with @ref R3D_DrawLightDebug.
 *       - For omni-directional lights, only the position is updated (direction is not calculated).
 *       - For spot lights, both position and direction are set accordingly.
 *
 * @param id The ID of the light.
 * @param position The position to set for the light.
 * @param target The point the light should look at.
 */
R3DAPI void R3D_SetLightTarget(R3D_Light id, Vector3 position, Vector3 target);

/**
 * @brief Gets the energy level of a light.
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
 * Default: 1.0f
 *
 * @param id The ID of the light.
 * @param energy The new energy value to set for the light.
 */
R3DAPI void R3D_SetLightEnergy(R3D_Light id, float energy);

/**
 * @brief Gets the energy level of a light as a luminous flux.
 *
 * Converts the light's current energy factor to lumens using a reference distance of 1 unit,
 * assuming 1 unit = 1 meter. For a custom reference distance, use @ref R3D_EnergyToLumens
 * with @ref R3D_GetLightEnergy directly.
 *
 * @param id The ID of the light.
 * @return The luminous flux in lumens.
 */
R3DAPI float R3D_GetLightLumen(R3D_Light id);

/**
 * @brief Sets the energy of a light from a luminous flux value.
 *
 * Converts the given flux to an energy factor using a reference distance of 1 unit,
 * assuming 1 unit = 1 meter. For a custom reference distance, use @ref R3D_LumensToEnergy
 * and pass the result to @ref R3D_SetLightEnergy directly.
 *
 * @param id The ID of the light.
 * @param lumens The luminous flux in lumens.
 */
R3DAPI void R3D_SetLightLumen(R3D_Light id, float lumens);

/**
 * @brief Gets the specular intensity of a light.
 *
 * @param id The ID of the light.
 * @return The current specular intensity of the light.
 */
R3DAPI float R3D_GetLightSpecular(R3D_Light id);

/**
 * @brief Sets the specular intensity of a light.
 *
 * This function sets the specular intensity of the specified light.
 * Specular intensity affects how shiny surfaces appear when reflecting the light.
 * Higher specular values result in stronger and sharper highlights on reflective surfaces.
 *
 * Default: 0.5f
 *
 * @param id The ID of the light.
 * @param specular The new specular intensity value to set for the light.
 */
R3DAPI void R3D_SetLightSpecular(R3D_Light id, float specular);

/**
 * @brief Gets the range of a light.
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
 * Default: 50.0f
 *
 * @param id The ID of the light.
 * @param range The range value to apply.
 */
R3DAPI void R3D_SetLightRange(R3D_Light id, float range);

/**
 * @brief Gets the falloff exponent of a light.
 *
 * @param id The ID of the light.
 * @return The falloff exponent of the light.
 */
R3DAPI float R3D_GetLightFalloff(R3D_Light id);

/**
 * @brief Sets the falloff exponent of a light.
 *
 * Controls the shape of the attenuation curve over the light's range.
 * A value of 1.0 produces a linear falloff, 2.0 a quadratic (more physically
 * plausible) falloff, and higher values concentrate the light closer to the source.
 * Values of 0.0 or below are clamped to 1.0.
 * Only applicable to spot lights and omni-lights.
 *
 * Default: 1.0f
 *
 * @param id The ID of the light.
 * @param falloff The falloff exponent to set. Typical range is [0.5, 4.0].
 */
R3DAPI void R3D_SetLightFalloff(R3D_Light id, float falloff);

/**
 * @brief Gets the inner and outer cone angles of a spot light.
 *
 * @param id The ID of the light.
 * @param inner Pointer to receive the inner cone angle, in degrees. May be NULL.
 * @param outer Pointer to receive the outer cone angle, in degrees. May be NULL.
 */
R3DAPI void R3D_GetLightAngle(R3D_Light id, float* inner, float* outer);

/**
 * @brief Sets the inner and outer cone angles of a spot light.
 *
 * The inner angle defines the region of full intensity, and the outer angle
 * defines where the light fully fades out. The transition between the two
 * produces a soft edge. Both angles are in degrees. If inner exceeds outer,
 * the two values are swapped automatically.
 * Only applicable to spot lights.
 *
 * Default: 22.5f, 45.0f
 *
 * @param id The ID of the light.
 * @param inner The inner cone half-angle, in degrees.
 * @param outer The outer cone half-angle, in degrees.
 */
R3DAPI void R3D_SetLightAngle(R3D_Light id, float inner, float outer);

/**
 * @brief Gets the volumetric fog energy multiplier of a light.
 *
 * @param id The ID of the light.
 * @return The volumetric fog energy multiplier of the light.
 */
R3DAPI float R3D_GetLightFogEnergy(R3D_Light id);

/**
 * @brief Sets the volumetric fog energy multiplier of a light.
 *
 * The final fog contribution is the light's energy multiplied by this factor,
 * allowing fine-grained control over its volumetric fog intensity independently
 * of its regular lighting contribution.
 *
 * Default: 1.0f
 *
 * @param id The ID of the light.
 * @param energy The new volumetric fog energy multiplier to set for the light.
 */
R3DAPI void R3D_SetLightFogEnergy(R3D_Light id, float energy);

// ----------------------------------------
// LIGHTING: Shadow Config Functions
// ----------------------------------------

/**
 * @brief Enables shadow rendering for a light.
 *
 * Turns on shadow rendering for the light. The engine will allocate a shadow
 * map if needed, or reuse one previously allocated for another light.
 *
 * Shadow map resolutions are by default:
 *   - 2048x2048 for spot and point lights
 *   - 4096x4096 for directional lights
 *
 * @param id The ID of the light.
 *
 * @note Creating too many shadow-casting lights can exhaust GPU memory and
 * potentially crash the graphics driver. Disabling shadows on one light and
 * enabling them on another is free, since existing shadow maps are reused.
 */
R3DAPI void R3D_EnableShadow(R3D_Light id);

/**
 * @brief Disables shadow rendering for a light.
 *
 * Turns off shadow rendering for the light. The associated shadow map is
 * kept in memory and may later be reused by another light.
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
 * Default: R3D_SHADOW_UPDATE_INTERVAL
 *
 * @param id The ID of the light.
 * @param mode The update mode to set for the shadow map (Interval, Continuous, or Manual).
 */
R3DAPI void R3D_SetShadowUpdateMode(R3D_Light id, R3D_ShadowUpdateMode mode);

/**
 * @brief Gets the interval between shadow map updates in interval update mode.
 *
 * Only relevant when the shadow update mode is set to @ref R3D_SHADOW_UPDATE_INTERVAL.
 *
 * @param id The ID of the light.
 * @return The interval in seconds between shadow map updates.
 */
R3DAPI float R3D_GetShadowUpdateInterval(R3D_Light id);

/**
 * @brief Sets the interval between shadow map updates in interval update mode.
 *
 * Only relevant when the shadow update mode is set to @ref R3D_SHADOW_UPDATE_INTERVAL.
 *
 * Default: 0.016f
 *
 * @param id The ID of the light.
 * @param seconds The interval in seconds between shadow map updates.
 */
R3DAPI void R3D_SetShadowUpdateInterval(R3D_Light id, float seconds);

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
 * Default: 4.0f
 *
 * @param id The ID of the light.
 * @param softness The softness radius in texels to apply (must be >= 0).
 *
 * @note The softness must be set only after shadows have been enabled for the light,
 *       since the shadow map resolution must be known before the softness can be applied.
 */
R3DAPI void R3D_SetShadowSoftness(R3D_Light id, float softness);

/**
 * @brief Retrieves the shadow opacity for a light.
 *
 * @param id The ID of the light.
 * @return The current shadow opacity.
 */
R3DAPI float R3D_GetShadowOpacity(R3D_Light id);

/**
 * @brief Sets the shadow opacity for a light.
 *
 * The opacity controls the visual strength of shadows. A value of 0 makes shadows
 * fully transparent, while 1 applies full opacity. Values are not clamped, but the
 * usual range is 0 to 1.
 *
 * When the opacity is exactly 0, the light still owns its shadow map, but shadow
 * map rendering and shadow application are entirely skipped.
 *
 * Default: 1.0f
 *
 * @param id The ID of the light.
 * @param opacity The shadow opacity to apply.
 */
R3DAPI void R3D_SetShadowOpacity(R3D_Light id, float opacity);

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
 *
 * Default per light type:
 *   - Directional: 0.0009765625
 *   - Spot:        0.0001220703
 *   - Omni:        0.025
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
 *
 * Default per light type:
 *   - Directional: 0.0014648438
 *   - Spot:        0.0004882813
 *   - Omni:        0.1
 */
R3DAPI void R3D_SetShadowSlopeBias(R3D_Light id, float value);

/**
 * @brief Gets the shadow caster mask.
 */
R3DAPI R3D_Layer R3D_GetShadowCasterMask(R3D_Light id);

/**
 * @brief Replaces the shadow caster mask.
 */
R3DAPI void R3D_SetShadowCasterMask(R3D_Light id, R3D_Layer cullMask);

/**
 * @brief Enables one or more layers in the shadow caster mask.
 */
R3DAPI void R3D_EnableShadowCasterLayers(R3D_Light id, R3D_Layer layerMask);

/**
 * @brief Disables one or more layers from the shadow caster mask.
 */
R3DAPI void R3D_DisableShadowCasterLayers(R3D_Light id, R3D_Layer layerMask);

/**
 * @brief Toggles one or more layers in the shadow caster mask.
 */
R3DAPI void R3D_ToggleShadowCasterLayers(R3D_Light id, R3D_Layer layerMask);

/**
 * @brief Checks whether at least one object layer is visible to the shadow caster.
 */
R3DAPI bool R3D_IsShadowCasterLayerVisible(R3D_Light id, R3D_Layer layerMask);

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
R3DAPI void R3D_DrawLightDebug(R3D_Light id);

// ----------------------------------------
// LIGHTING: Math Helper Functions
// ----------------------------------------

/**
 * @brief Converts a luminous flux to an energy factor.
 *
 * Computes the illuminance in lux at the given reference distance from an isotropic
 * point source: `energy = lumens / (4 * pi * distance * distance)`
 *
 * @param lumens The luminous flux in lumens.
 * @param referenceDistance The reference distance in scene units (1 unit = 1 meter).
 * @return The corresponding energy factor.
 */
R3DAPI float R3D_LumensToEnergy(float lumens, float referenceDistance);

/**
 * @brief Converts an energy factor back to a luminous flux.
 *
 * Inverse of @ref R3D_LumensToEnergy: `lumens = energy * 4 * pi * distance * distance`
 *
 * @param energy The energy factor.
 * @param referenceDistance The reference distance in scene units (1 unit = 1 meter).
 * @return The corresponding luminous flux in lumens.
 */
R3DAPI float R3D_EnergyToLumens(float energy, float referenceDistance);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Lighting

#endif // R3D_LIGHTING_H
