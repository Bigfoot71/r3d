/* r3d_env.h -- Internal R3D environment module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_ENV_H
#define R3D_MODULE_ENV_H

#include <r3d/r3d_probe.h>
#include <raylib.h>
#include <glad.h>

#include "../common/r3d_frustum.h"

// ========================================
// HELPER MACROS
// ========================================

#define R3D_ENV_PROBE_FOR_EACH_VALID(probe) \
    for (r3d_env_probe_t* probe = NULL; \
         r3d_env_probe_iter(&probe, R3D_ENV_PROBE_ARRAY_VALID); )

#define R3D_ENV_PROBE_FOR_EACH_VISIBLE(probe) \
    for (r3d_env_probe_t* probe = NULL; \
         r3d_env_probe_iter(&probe, R3D_ENV_PROBE_ARRAY_VISIBLE); )

// ========================================
// TYPES
// ========================================

typedef struct {
    R3D_ProbeUpdateMode updateMode;
    bool matrixShouldBeUpdated;
    bool sceneShouldBeUpdated;
} r3d_env_probe_state_t;

typedef struct {
    r3d_env_probe_state_t state;
    r3d_frustum_t frustum[6];
    Matrix view[6];
    Matrix proj[6];

    R3D_ProbeType type;
    int irradiance;     // Layer index, -1 if unused
    int prefilter;      // Layer index, -1 if unused

    Vector3 position;
    float falloff;
    float range;

    bool interior;
    bool shadows;
    bool enabled;
} r3d_env_probe_t;

typedef enum {
    R3D_ENV_PROBE_ARRAY_VISIBLE,
    R3D_ENV_PROBE_ARRAY_VALID,
    R3D_ENV_PROBE_ARRAY_FREE,
    R3D_ENV_PROBE_ARRAY_COUNT
} r3d_env_probe_array_enum_t;

typedef struct {
    R3D_Probe* probes;
    int count;
} r3d_env_probe_array_t;

// Cubemap layer pool (manages reusable texture layers)
typedef struct {
    int* freeLayers;        // Stack of available layer indices
    int freeCount;          // Number of free layers
    int freeCapacity;       // Capacity of freeLayers array
    int totalLayers;        // Total allocated layers
} r3d_env_layer_pool_t;

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_env {
    GLuint workFramebuffer;
    GLuint irradianceArray;
    GLuint prefilterArray;

    GLuint captureFramebuffer;
    GLuint captureDepth;
    GLuint captureCube;

    r3d_env_layer_pool_t irradiancePool;
    r3d_env_layer_pool_t prefilterPool;

    bool captureCubeAllocated;

    r3d_env_probe_array_t arrays[R3D_ENV_PROBE_ARRAY_COUNT];
    r3d_env_probe_t* probes;
    int capacityProbes;
} R3D_MOD_ENV;

// ========================================
// MODULE FUNCTIONS
// ========================================

/* Initialize module (called once during R3D_Init) */
bool r3d_env_init(void);

/* Deinitialize module (called once during R3D_Close) */
void r3d_env_quit(void);

/* Create a new probe of the given type */
R3D_Probe r3d_env_probe_new(R3D_ProbeType type);

/* Delete a probe and return it to the free list */
void r3d_env_probe_delete(R3D_Probe index);

/* Check whether a probe handle is valid */
bool r3d_env_probe_is_valid(R3D_Probe index);

/* Get internal probe structure (returns NULL if invalid) */
r3d_env_probe_t* r3d_env_probe_get(R3D_Probe index);

/* Check if the specified probe array is not empty */
bool r3d_env_probe_has(r3d_env_probe_array_enum_t array);

/* Iterator for probes by category (stateful, not thread-safe) */
bool r3d_env_probe_iter(r3d_env_probe_t** probe, r3d_env_probe_array_enum_t array);

/* Update all probes and collect visible ones */
void r3d_env_probe_update_and_cull(const r3d_frustum_t* viewFrustum);

/* Check if probe should be rendered (updates state if willBeUpdated is true) */
bool r3d_env_probe_should_be_updated(r3d_env_probe_t* probe, bool willBeUpdated);

/* Reserve a new irradiance map layer (returns -1 on failure) */
int r3d_env_irradiance_reserve_layer(void);

/* Release an irradiance map layer */
void r3d_env_irradiance_release_layer(int layer);

/* Bind irradiance framebuffer for the given layer and face */
void r3d_env_irradiance_bind_fbo(int layer, int face);

/* Get irradiance cubemap array texture ID */
GLuint r3d_env_irradiance_get(void);

/* Reserve a new prefilter map layer (returns -1 on failure) */
int r3d_env_prefilter_reserve_layer(void);

/* Release a prefilter map layer */
void r3d_env_prefilter_release_layer(int layer);

/* Bind prefilter framebuffer for the given layer, face and mip level */
void r3d_env_prefilter_bind_fbo(int layer, int face, int mipLevel);

/* Get prefiltered cubemap array texture ID */
GLuint r3d_env_prefilter_get(void);

/* Bind capture framebuffer for the given face and mip level */
void r3d_env_capture_bind_fbo(int face, int mipLevel);

/* Generate mipmaps for the capture target */
void r3d_env_capture_gen_mipmaps(void);

/* Get capture cubemap texture ID */
GLuint r3d_env_capture_get(void);

#endif // R3D_MODULE_ENV_H
