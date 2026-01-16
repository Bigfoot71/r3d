/* r3d_light.h -- Internal R3D light module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_LIGHT_H
#define R3D_MODULE_LIGHT_H

#include <r3d/r3d_lighting.h>
#include <raylib.h>
#include <glad.h>

#include "../common/r3d_frustum.h"
#include "../common/r3d_math.h"

// ========================================
// HELPER MACROS
// ========================================

#define R3D_LIGHT_FOR_EACH_VISIBLE(light) \
    for (r3d_light_t* light = NULL; \
         r3d_light_iter(&light, R3D_LIGHT_ARRAY_VISIBLE); )

// ========================================
// TYPES
// ========================================

typedef struct {
    R3D_ShadowUpdateMode shadowUpdate;
    float shadowFrequencySec;
    float shadowTimerSec;
    bool shadowShouldBeUpdated;
    bool matrixShouldBeUpdated;
} r3d_light_state_t;

typedef struct {

    r3d_frustum_t frustum[6];   // Frustum (only [0] for dir/spot, 6 for omni)
    Matrix viewProj[6];         // View/projection matrix (only [0] for dir/spot, 6 for omni)
    BoundingBox aabb;           // AABB in world space of the light volume

    r3d_light_state_t state;    // Contains the current state useful for the update
    int shadowLayer;            // Shadow map layer index, -1 if no shadow
    
    Vector3 color;
    Vector3 position;           // Light position (spot/omni)
    Vector3 direction;          // Light direction (spot/dir)
    
    float specular;
    float energy;
    float range;                // Maximum distance (spot/omni)
    float near;                 // Near plane for shadow projection
    float far;                  // Far plane for shadow projection
    float attenuation;          // Additional attenuation (spot/omni)
    float innerCutOff;          // Spot light inner cutoff angle
    float outerCutOff;          // Spot light outer cutoff angle
    float shadowSoftness;       // Softness factor for penumbra
    float shadowTexelSize;      // Size of a texel in shadow map
    float shadowDepthBias;      // Constant depth bias
    float shadowSlopeBias;      // Slope-scaled depth bias
    
    R3D_LightType type;
    bool enabled;
    bool shadow;

} r3d_light_t;

typedef enum {
    R3D_LIGHT_ARRAY_VISIBLE,
    R3D_LIGHT_ARRAY_VALID,
    R3D_LIGHT_ARRAY_FREE,
    R3D_LIGHT_ARRAY_COUNT
} r3d_light_array_enum_t;

typedef struct {
    R3D_Light* lights;
    int count;
} r3d_light_array_t;

// Shadow layer pool
typedef struct {
    int* freeLayers;
    int freeCount;
    int freeCapacity;
    int totalLayers;
} r3d_light_shadow_pool_t;

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_light {

    // Common framebuffer for rendering or copy
    GLuint workFramebuffer;

    // Shadow map arrays and layer pools
    GLuint shadowArrays[R3D_LIGHT_TYPE_COUNT];
    r3d_light_shadow_pool_t shadowPools[R3D_LIGHT_TYPE_COUNT];

    // Light management
    r3d_light_array_t arrays[R3D_LIGHT_ARRAY_COUNT];
    r3d_light_t* lights;
    int capacityLights;

} R3D_MOD_LIGHT;

// ========================================
// MODULE FUNCTIONS
// ========================================

/* Initialize module (called once during R3D_Init) */
bool r3d_light_init(void);

/* Deinitialize module (called once during R3D_Close) */
void r3d_light_quit(void);

/* Create a new light of the given type */
R3D_Light r3d_light_new(R3D_LightType type);

/* Delete a light and return it to the free list */
void r3d_light_delete(R3D_Light index);

/* Check whether a light handle is valid */
bool r3d_light_is_valid(R3D_Light index);

/* Get internal light structure (returns NULL if invalid) */
r3d_light_t* r3d_light_get(R3D_Light index);

/* Returns the screen-space rectangle covered by the light's influence */
r3d_rect_t r3d_light_get_screen_rect(const r3d_light_t* light, const Matrix* viewProj, int w, int h);

/* Iterator for lights by category (stateful, not thread-safe) */
bool r3d_light_iter(r3d_light_t** light, r3d_light_array_enum_t array);

/* Enable shadows for a light */
void r3d_light_enable_shadows(r3d_light_t* light);

/* Disable shadows for a light */
void r3d_light_disable_shadows(r3d_light_t* light);

/* Update all lights and collect visible ones */
void r3d_light_update_and_cull(const r3d_frustum_t* viewFrustum, Vector3 viewPosition);

/* Check if shadow map should be rendered (updates state if willBeUpdated is true) */
bool r3d_light_shadow_should_be_updated(r3d_light_t* light, bool willBeUpdated);

/* Bind shadow framebuffer for a light type */
void r3d_light_shadow_bind_fbo(R3D_LightType type, int layer, int face);

/* Get a shadow map array texture ID */
GLuint r3d_light_shadow_get(R3D_LightType type);

// ========================================
// INLINE QUERIES
// ========================================

static inline bool r3d_light_has_visible(void)
{
    return R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VISIBLE].count > 0;
}

#endif // R3D_MODULE_LIGHT_H
