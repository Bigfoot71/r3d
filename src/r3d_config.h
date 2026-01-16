#ifndef R3D_CONFIG_H
#define R3D_CONFIG_H

// ========================================
// Forward rendering limits
// ========================================

/**
 * Maximum number of lights affecting a single mesh in the forward rendering path.
 * This applies to transparent objects and/or non-mixed blending modes,
 * and any rendering performed during probe capture.
 */
#define R3D_MAX_LIGHT_FORWARD_PER_MESH 8

// ========================================
// Probe system
// ========================================

/**
 * Maximum number of probes whose maps can be rendered simultaneously on screen.
 */
#define R3D_MAX_PROBE_ON_SCREEN 8

/**
 * Resolution of each face of the probe capture cubemap.
 */
#define R3D_PROBE_CAPTURE_SIZE 256

// ========================================
// Shadow mapping
// ========================================

/**
 * Resolution of directional light shadow maps.
 */
#define R3D_SHADOW_MAP_DIRECTIONAL_SIZE 4096

/**
 * Resolution of spot light shadow maps.
 */
#define R3D_SHADOW_MAP_SPOT_SIZE        2048

/**
 * Resolution of omni (point) light shadow maps.
 */
#define R3D_SHADOW_MAP_OMNI_SIZE        2048

// ========================================
// IBL / Probes
// ========================================

/**
 * Resolution of each face of irradiance cubemaps
 * used for ambient lighting and probes.
 */
#define R3D_CUBEMAP_IRRADIANCE_SIZE     32

/**
 * Resolution of each face of prefiltered cubemaps
 * used for ambient lighting and probes.
 */
#define R3D_CUBEMAP_PREFILTER_SIZE      128

// ========================================
// Debug
// ========================================

#ifndef R3D_TRACELOG_DISABLED
#   define R3D_TRACELOG(level, ...) TraceLog(level, __VA_ARGS__)
#else
#   define R3D_TRACELOG(level, ...) (void)0
#endif

#endif // R3D_CONFIG_H
