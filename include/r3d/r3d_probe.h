/* r3d_probe.h -- R3D Probe Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_PROBE_H
#define R3D_PROBE_H

#include "./r3d_platform.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Probe
 * @{
 */

// ========================================
// ENUM TYPES
// ========================================

/**
 * @brief Bit-flags controlling what components are generated.
 *
 * - R3D_PROBE_ILLUMINATION -> generate diffuse irradiance
 * - R3D_PROBE_REFLECTION   -> generate specular prefiltered map
 */
typedef uint32_t R3D_ProbeFlags;

#define R3D_PROBE_ILLUMINATION    (1 << 0)
#define R3D_PROBE_REFLECTION      (1 << 1)

/**
 * @brief Modes for updating probes.
 *
 * Controls how often probe captures are refreshed.
 */
typedef enum R3D_ProbeUpdateMode {
    R3D_PROBE_UPDATE_ONCE,      ///< Updated only when its state or content changes
    R3D_PROBE_UPDATE_ALWAYS     ///< Updated during every frames
} R3D_ProbeUpdateMode;

// ========================================
// HANDLE TYPE
// ========================================

/**
 * @brief Unique identifier for an R3D probe.
 *
 * Negative values indicate an invalid probe.
 */
typedef int32_t R3D_Probe;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates a new probe of the specified type.
 *
 * The returned probe must be destroyed using ::R3D_DestroyProbe
 * when it is no longer needed.
 *
 * @param flags IBL components that the probe must support.
 * @return A valid probe ID, or a negative value on failure.
 */
R3DAPI R3D_Probe R3D_CreateProbe(R3D_ProbeFlags flags);

/**
 * @brief Destroys a probe and frees its resources.
 *
 * @param id Probe ID to destroy.
 */
R3DAPI void R3D_DestroyProbe(R3D_Probe id);

/**
 * @brief Returns whether a probe exists.
 *
 * @param id Probe ID.
 * @return true if the probe is valid and allocated, otherwise false.
 */
R3DAPI bool R3D_IsProbeExist(R3D_Probe id);

/**
 * @brief Returns the probe flags.
 *
 * @param id Probe ID.
 * @return The flags assigned to the probe.
 */
R3DAPI R3D_ProbeFlags R3D_GetProbeFlags(R3D_Probe id);

/**
 * @brief Returns whether a probe is currently active.
 *
 * Inactive probes do not contribute to lighting.
 *
 * @param id Probe ID.
 * @return true if active, otherwise false.
 */
R3DAPI bool R3D_IsProbeActive(R3D_Probe id);

/**
 * @brief Enables or disables a probe.
 *
 * @param id Probe ID.
 * @param active Set to true to enable the probe.
 */
R3DAPI void R3D_SetProbeActive(R3D_Probe id, bool active);

/**
 * @brief Gets the probe update mode.
 *
 * - R3D_PROBE_UPDATE_ONCE:
 *     Captured once, then reused unless its state changes.
 *
 * - R3D_PROBE_UPDATE_ALWAYS:
 *     Recaptured every frame.
 *
 * Use "ONCE" for static scenes, "ALWAYS" for highly dynamic scenes.
 */
R3DAPI R3D_ProbeUpdateMode R3D_GetProbeUpdateMode(R3D_Probe id);

/**
 * @brief Sets the probe update mode.
 *
 * Controls when the probe capture is refreshed.
 *
 * @param id Probe ID.
 * @param mode Update mode to apply.
 */
R3DAPI void R3D_SetProbeUpdateMode(R3D_Probe id, R3D_ProbeUpdateMode mode);

/**
 * @brief Returns whether the probe is considered indoors.
 *
 * Indoor probes do not sample skybox or environment maps.
 * Instead they rely only on ambient and background colors.
 *
 * Use this for rooms, caves, tunnels, etc...
 * where outside lighting should not bleed inside.
 */
R3DAPI bool R3D_GetProbeInterior(R3D_Probe id);

/**
 * @brief Enables or disables indoor mode for the probe.
 */
R3DAPI void R3D_SetProbeInterior(R3D_Probe id, bool active);

/**
 * @brief Returns whether shadows are captured by this probe.
 *
 * When enabled, shadowing is baked into the captured lighting.
 * This improves realism, but increases capture cost.
 */
R3DAPI bool R3D_GetProbeShadows(R3D_Probe id);

/**
 * @brief Enables or disables shadow rendering during probe capture.
 */
R3DAPI void R3D_SetProbeShadows(R3D_Probe id, bool active);

/**
 * @brief Gets the world position of the probe.
 */
R3DAPI Vector3 R3D_GetProbePosition(R3D_Probe id);

/**
 * @brief Sets the world position of the probe.
 */
R3DAPI void R3D_SetProbePosition(R3D_Probe id, Vector3 position);

/**
 * @brief Gets the effective range of the probe.
 *
 * The range defines the radius (in world units) within which this probe
 * contributes to lighting. Objects outside this sphere receive no influence.
 */
R3DAPI float R3D_GetProbeRange(R3D_Probe id);

/**
 * @brief Sets the effective range of the probe.
 *
 * @param range Radius in world units. Must be > 0.
 */
R3DAPI void R3D_SetProbeRange(R3D_Probe id, float range);

/**
 * @brief Gets the falloff factor applied to probe contributions.
 *
 * Falloff controls how lighting fades as distance increases.
 *
 * Internally this uses a power curve:
 *     attenuation = 1.0 - pow(dist / probe.range, probe.falloff)
 *
 * Effects:
 *   - falloff = 1 -> linear fade
 *   - falloff > 1 -> light stays strong near the probe, drops faster at the edge
 *   - falloff < 1 -> softer fade across the whole range
 */
R3DAPI float R3D_GetProbeFalloff(R3D_Probe id);

/**
 * @brief Sets the falloff factor used for distance attenuation.
 *
 * Larger values make the probe feel more localized.
 */
R3DAPI void R3D_SetProbeFalloff(R3D_Probe id, float falloff);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Probe

#endif // R3D_PROBE_H
