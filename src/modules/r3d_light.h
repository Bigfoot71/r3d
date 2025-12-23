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

#include "../details/r3d_frustum.h"
#include "../details/r3d_math.h"

// ========================================
// HELPER MACROS
// ========================================

#define R3D_LIGHT_FOR_EACH_VISIBLE(light) \
    for (r3d_light_t* light = NULL; \
         r3d_light_iter(&light, R3D_LIGHT_ARRAY_VISIBLE); )

// ========================================
// LIGHT STRUCTURES
// ========================================

typedef struct {
    R3D_ShadowUpdateMode shadowUpdate;
    float shadowFrequencySec;
    float shadowTimerSec;
    bool shadowShouldBeUpdated;
    bool matrixShouldBeUpdated;
} r3d_light_state_t;

typedef struct {
    GLuint fbo, tex;
    int resolution;
} r3d_light_shadow_map_t;

typedef struct {
    Matrix matVP[6];                        //< View/projection matrix of the light (only [0] for dir/spot, 6 for omni lights)
    r3d_frustum_t frustum[6];               //< Frustum of the light (only [0] for dir/spot, 6 for omni lights) (calculated only if shadows are enabled)
    BoundingBox aabb;                       //< AABB in world space of the light volume
    r3d_light_state_t state;                //< Light update config
    r3d_light_shadow_map_t shadowMap;       //< 2D or Cube shadow map
    GLuint shadowCubemap;                   //< Cube shadow map used for omni-directional shadow projection
    Vector3 color;                          //< Light color modulation tint
    Vector3 position;                       //< Light position (spot/omni)
    Vector3 direction;                      //< Light direction (spot/dir)
    float specular;                         //< Specular factor (not physically accurate but provides more flexibility)
    float energy;                           //< Light energy factor
    float range;                            //< Maximum distance the light can travel before being completely attenuated (spot/omni)
    float size;                             //< Light size, currently used only for shadows (PCSS)
    float near;                             //< Near plane for the shadow map projection
    float far;                              //< Far plane for the shadow map projection
    float attenuation;                      //< Additional light attenuation factor (spot/omni)
    float innerCutOff;                      //< Spot light inner cutoff angle
    float outerCutOff;                      //< Spot light outer cutoff angle
    float shadowSoftness;                   //< Softness factor to simulate a penumbra
    float shadowTexelSize;                  //< Size of a texel in the 2D shadow map
    float shadowDepthBias;                  //< Constant depth bias applied to shadow mapping to reduce shadow acne
    float shadowSlopeBias;                  //< Additional bias scaled by surface slope to reduce artifacts on angled geometry
    R3D_LightType type;                     //< Light type (dir/spot/omni)
    bool enabled;                           //< Indicates whether the light is on
    bool shadow;                            //< Indicates whether the light generates shadows
} r3d_light_t;

// ========================================
// MODULE STATE
// ========================================

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

extern struct r3d_light {
    r3d_light_array_t arrays[R3D_LIGHT_ARRAY_COUNT];
    r3d_light_t* lights;
    int capacityLights;
} R3D_MOD_LIGHT;

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_light_init(void);

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_light_quit(void);

/*
 * Create a new light of the given type.
 * Allocates or reuses an internal slot and initializes default values.
 */
R3D_Light r3d_light_new(R3D_LightType type);

/*
 * Delete a light and return it to the free list.
 * The handle becomes invalid after this call.
 */
void r3d_light_delete(R3D_Light index);

/*
 * Check whether a light handle refers to a valid light.
 */
bool r3d_light_is_valid(R3D_Light index);

/*
 * Retrieve the internal light structure from a handle.
 * Returns NULL if the handle is invalid.
 */
r3d_light_t* r3d_light_get(R3D_Light index);

/*
 * Returns the screen-space rectangle covered by the light's influence.
 * Supports SPOT and OMNI lights only, asserts the light is not DIR.
 */
r3d_rect_t r3d_light_get_screen_rect(const r3d_light_t* light, const Matrix* viewProj, int w, int h);

/*
 * Internal helper to iterate over lights by category.
 * Stateful and not thread-safe.
 */
bool r3d_light_iter(r3d_light_t** light, int array_type);

/*
 * Mark a light as needing its matrices and bounding box updated.
 * Actual computation is deferred.
 */
void r3d_light_update_matrix(r3d_light_t* light);

/*
 * Enable shadows for a light and configure shadow parameters.
 * Creates or resizes the shadow map if necessary.
 */
void r3d_light_enable_shadows(r3d_light_t* light, int resolution);

/*
 * Update all lights, recompute state, and collect visible lights.
 * Performs frustum culling and shadow updates when needed.
 */
void r3d_light_update_and_cull(const r3d_frustum_t* viewFrustum, Vector3 viewPosition);

/*
 * Indicate whether the shadow map should be rendered.
 * The internal update state can be reset after the query.
 */
bool r3d_light_shadow_should_be_upadted(r3d_light_t* light, bool willBeUpdated);

#endif // R3D_MODULE_LIGHT_H
