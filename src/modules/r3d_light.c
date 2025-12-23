/* r3d_light.c -- Internal R3D light module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_light.h"
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_light R3D_MOD_LIGHT;

// ========================================
// INTERNAL LIGHT FUNCTIONS
// ========================================

static void init_light(r3d_light_t* light, R3D_LightType type)
{
    memset(light, 0, sizeof(r3d_light_t));

    /* --- Set base light parameters --- */

    light->aabb.min = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    light->aabb.max = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};

    light->color = (Vector3) {1, 1, 1};
    light->position = (Vector3) {0};
    light->direction = (Vector3) {0, 0, -1};

    light->specular = 0.5f;
    light->energy = 1.0f;
    light->range = 50.0f;

    light->attenuation = 1.0f;
    light->innerCutOff = cosf(22.5f * DEG2RAD);
    light->outerCutOff = cosf(45.0f * DEG2RAD);

    light->type = type;
    light->enabled = false;

    /* --- Set common shadow config --- */

    light->state.shadowUpdate = R3D_SHADOW_UPDATE_INTERVAL;
    light->state.shadowShouldBeUpdated = true;
    light->state.shadowFrequencySec = 0.016f;
    light->state.shadowTimerSec = 0.0f;

    /* --- Set specific shadow config --- */

    switch (type) {
    case R3D_LIGHT_DIR:
        light->shadowDepthBias = 0.0002f;
        light->shadowSlopeBias = 0.002f;
        break;
    case R3D_LIGHT_SPOT:
        light->shadowDepthBias = 0.00002f;
        light->shadowSlopeBias = 0.0002f;
        break;
    case R3D_LIGHT_OMNI:
        light->shadowDepthBias = 0.01f;
        light->shadowSlopeBias = 0.02f;
        break;
    }
}

void update_light_shadow_state(r3d_light_t* light)
{
    switch (light->state.shadowUpdate) {
    case R3D_SHADOW_UPDATE_MANUAL:
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        if (!light->state.shadowShouldBeUpdated) {
            light->state.shadowTimerSec += GetFrameTime();
            if (light->state.shadowTimerSec >= light->state.shadowFrequencySec) {
                light->state.shadowTimerSec -= light->state.shadowFrequencySec;
                light->state.shadowShouldBeUpdated = true;
            }
        }
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        light->state.shadowShouldBeUpdated = true;
        break;
    }
}

static void update_light_dir_matrix(r3d_light_t* light, Vector3 viewPosition)
{
    Vector3 lightDir = light->direction;
    float extent = light->range;

    /* --- Create an orthonormal basis for light --- */

    Vector3 up = (fabsf(Vector3DotProduct(lightDir, (Vector3) {0, 1, 0})) > 0.99f) ? (Vector3) {0, 0, 1} : (Vector3) {0, 1, 0};
    Vector3 lightRight = Vector3Normalize(Vector3CrossProduct(up, lightDir));
    Vector3 lightUp = Vector3CrossProduct(lightDir, lightRight);

    /* --- Project the camera's position into light space --- */

    float camX = Vector3DotProduct(viewPosition, lightRight);
    float camY = Vector3DotProduct(viewPosition, lightUp);
    float camZ = Vector3DotProduct(viewPosition, lightDir);

    /* --- Snap to the texel grid --- */

    float shadowMapSize = light->shadowMap.resolution;
    float worldUnitsPerTexel = (2.0f * extent) / shadowMapSize;

    float snappedX = floorf(camX / worldUnitsPerTexel) * worldUnitsPerTexel;
    float snappedY = floorf(camY / worldUnitsPerTexel) * worldUnitsPerTexel;

    /* --- Reconstruct the snapped world space position --- */

    Vector3 lightPosition;
    lightPosition.x = lightRight.x * snappedX + lightUp.x * snappedY + lightDir.x * camZ;
    lightPosition.y = lightRight.y * snappedX + lightUp.y * snappedY + lightDir.y * camZ;
    lightPosition.z = lightRight.z * snappedX + lightUp.z * snappedY + lightDir.z * camZ;

    /* --- Construct view projection --- */

    Matrix view = MatrixLookAt(lightPosition, Vector3Add(lightPosition, lightDir), lightUp);
    Matrix proj = MatrixOrtho(-extent, extent, -extent, extent, -extent, extent);

    light->matVP[0] = MatrixMultiply(view, proj);

    /* --- Keep near / far --- */

    light->near = -extent;  // Save near plane (can be used in shaders)
    light->far = extent;    // Save far plane (can be used in shaders)
}

static void update_light_spot_matrix(r3d_light_t* light)
{
    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)

    Matrix view = MatrixLookAt(light->position, Vector3Add(light->position, light->direction), (Vector3) {0, 1, 0});
    Matrix proj = MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);

    light->matVP[0] = MatrixMultiply(view, proj);
}

static void update_light_omni_matrix(r3d_light_t* light)
{
    assert(light->type == R3D_LIGHT_OMNI);

    static const Vector3 dirs[6] = {
        {  1.0,  0.0,  0.0 }, // +X
        { -1.0,  0.0,  0.0 }, // -X
        {  0.0,  1.0,  0.0 }, // +Y
        {  0.0, -1.0,  0.0 }, // -Y
        {  0.0,  0.0,  1.0 }, // +Z
        {  0.0,  0.0, -1.0 }  // -Z
    };

    static const Vector3 ups[6] = {
        {  0.0, -1.0,  0.0 }, // +X
        {  0.0, -1.0,  0.0 }, // -X
        {  0.0,  0.0,  1.0 }, // +Y
        {  0.0,  0.0, -1.0 }, // -Y
        {  0.0, -1.0,  0.0 }, // +Z
        {  0.0, -1.0,  0.0 }  // -Z
    };

    light->near = 0.05f;        // Save near plane (can be used in shaders)
    light->far = light->range;  // Save far plane (can be used in shaders)

    Matrix proj = MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);

    for (int i = 0; i < 6; i++) {
        Matrix view = MatrixLookAt(light->position, Vector3Add(light->position, dirs[i]), ups[i]);
        light->matVP[i] = MatrixMultiply(view, proj);
    }
}

static void update_light_matrix(r3d_light_t* light, Vector3 viewPosition)
{
    switch (light->type) {
    case R3D_LIGHT_DIR:
        update_light_dir_matrix(light, viewPosition);
        break;
    case R3D_LIGHT_SPOT:
        update_light_spot_matrix(light);
        break;
    case R3D_LIGHT_OMNI:
        update_light_omni_matrix(light);
        break;
    }
}

static void update_light_frustum(r3d_light_t* light)
{
    int n = (light->type == R3D_LIGHT_OMNI) ? 6 : 1;

    for (int i = 0; i < n; i++) {
        light->frustum[i] = r3d_frustum_create(light->matVP[i]);
    }
}

static void update_light_bounding_box(r3d_light_t* light)
{
    BoundingBox* aabb = &light->aabb;

    aabb->min = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    aabb->max = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};

    switch (light->type)
    {
    case R3D_LIGHT_OMNI:
        aabb->min = Vector3AddValue(light->position, -light->range);
        aabb->max = Vector3AddValue(light->position, +light->range);
        break;
    case R3D_LIGHT_SPOT:
        *aabb = r3d_frustum_get_bounding_box(light->matVP[0]);
        break;
    case R3D_LIGHT_DIR:
        assert(false);
        break;
    }
}

// ========================================
// INTERNAL SHADOW MAP FUNCTIONS
// ========================================

static r3d_light_shadow_map_t alloc_shadow_map_2d(int resolution)
{
    r3d_light_shadow_map_t shadowMap = {0};
    shadowMap.resolution = resolution;

    if (shadowMap.fbo == 0) {
        glGenFramebuffers(1, &shadowMap.fbo);
        glGenTextures(1, &shadowMap.tex);
    }

    glBindTexture(GL_TEXTURE_2D, shadowMap.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap.tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "R3D: Framebuffer creation error for the 2D shadow map");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return shadowMap;
}

static r3d_light_shadow_map_t alloc_shadow_map_cube(int resolution)
{
    r3d_light_shadow_map_t shadowMap = { 0 };
    shadowMap.resolution = resolution;

    if (shadowMap.fbo == 0) {
        glGenFramebuffers(1, &shadowMap.fbo);
        glGenTextures(1, &shadowMap.tex);
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, shadowMap.tex);
    for (int i = 0; i < 6; ++i) {
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT16,
            resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMap.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_CUBE_MAP_POSITIVE_X, shadowMap.tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "R3D: Framebuffer creation error for the Cube shadow map");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return shadowMap;
}

// ========================================
// INTERNAL ARRAY FUNCTIONS
// ========================================

bool growth_arrays(void)
{
    int newCapacity = 2 * R3D_MOD_LIGHT.capacityLights;

    r3d_light_t* newLights = RL_REALLOC(R3D_MOD_LIGHT.lights, newCapacity * sizeof(*R3D_MOD_LIGHT.lights));
    if (newLights == NULL) return false;

    R3D_MOD_LIGHT.capacityLights = newCapacity;
    R3D_MOD_LIGHT.lights = newLights;

    for (int i = 0; i < R3D_LIGHT_ARRAY_COUNT; i++) {
        R3D_Light** oldPtr = &R3D_MOD_LIGHT.arrays[i].lights;
        R3D_Light* newPtr = RL_REALLOC(*oldPtr, newCapacity * sizeof(**oldPtr));
        if (newPtr == NULL) return false;
        *oldPtr = newPtr;
    }

    return true;
}

// ========================================
// MODULE STATE
// ========================================

bool r3d_light_init(void)
{
    memset(&R3D_MOD_LIGHT, 0, sizeof(R3D_MOD_LIGHT));

    const int LIGHT_RESERVE_COUNT = 32;

    R3D_MOD_LIGHT.lights = RL_MALLOC(LIGHT_RESERVE_COUNT * sizeof(*R3D_MOD_LIGHT.lights));
    R3D_MOD_LIGHT.capacityLights = LIGHT_RESERVE_COUNT;
    if (!R3D_MOD_LIGHT.lights) {
        TraceLog(LOG_FATAL, "R3D: Failed to init light module; Main light array allocation failed");
        return false;
    }

    for (int i = 0; i < R3D_LIGHT_ARRAY_COUNT; i++) {
        R3D_MOD_LIGHT.arrays[i].lights = RL_MALLOC(LIGHT_RESERVE_COUNT * sizeof(*R3D_MOD_LIGHT.arrays[i].lights));
        if (R3D_MOD_LIGHT.arrays[i].lights == NULL) {
            TraceLog(LOG_FATAL, "R3D: Failed to init light module; Light array %i allocation failed", i);
            for (int j = 0; j <= i; j++) RL_FREE(R3D_MOD_LIGHT.arrays[j].lights);
            return false;
        }
    }

    return true;
}

void r3d_light_quit(void)
{
    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];
    r3d_light_array_t* freeLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_FREE];

    for (int i = 0; i < validLights->count; i++) {
        r3d_light_t* light = &R3D_MOD_LIGHT.lights[validLights->lights[i]];
        if (light->shadowMap.fbo != 0) {
            glDeleteFramebuffers(1, &light->shadowMap.fbo);
            glDeleteTextures(1, &light->shadowMap.tex);
        }
    }

    for (int i = 0; i < freeLights->count; i++) {
        r3d_light_t* light = &R3D_MOD_LIGHT.lights[freeLights->lights[i]];
        if (light->shadowMap.fbo != 0) {
            glDeleteFramebuffers(1, &light->shadowMap.fbo);
            glDeleteTextures(1, &light->shadowMap.tex);
        }
    }

    for (int i = 0; i < R3D_LIGHT_ARRAY_COUNT; i++) {
        RL_FREE(R3D_MOD_LIGHT.arrays[i].lights);
    }

    RL_FREE(R3D_MOD_LIGHT.lights);
}

R3D_Light r3d_light_new(R3D_LightType type)
{
    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];
    r3d_light_array_t* freeLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_FREE];

    R3D_Light index = validLights->count;

    if (freeLights->count == 0) {
        validLights->lights[validLights->count++] = index;
    }
    else {
        index = freeLights->lights[--freeLights->count];
    }

    if (index >= R3D_MOD_LIGHT.capacityLights) {
        if (!growth_arrays()) {
            TraceLog(LOG_FATAL, "R3D: Bad alloc on light creation");
            return -1;
        }
    }

    init_light(&R3D_MOD_LIGHT.lights[index], type);

    return index;
}

void r3d_light_delete(R3D_Light index)
{
    if (index < 0) return;

    bool lightFound = false;

    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];
    for (int i = 0; i < validLights->count; i++) {
        if (index == validLights->lights[i]) {
            int numToMove = validLights->count - i - 1;
            if (numToMove > 0) {
                memmove(
                    &validLights->lights[i],
                    &validLights->lights[i + 1],
                    numToMove * sizeof(validLights->lights[0])
                );
            }
            validLights->count--;
            lightFound = true;
            break;
        }
    }

    if (!lightFound) {
        return;
    }

    r3d_light_array_t* freeLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_FREE];
    freeLights->lights[freeLights->count++] = index;
}

bool r3d_light_is_valid(R3D_Light index)
{
    if (index < 0) return false;

    const r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];
    for (int i = 0; i < validLights->count; i++) {
        if (index == validLights->lights[i]) {
            return true;
        }
    }

    return false;
}

r3d_light_t* r3d_light_get(R3D_Light index)
{
    if (r3d_light_is_valid(index)) {
        return &R3D_MOD_LIGHT.lights[index];
    }

    return NULL;
}

bool r3d_light_iter(r3d_light_t** light, int array_type)
{
    static int index = 0;

    index = (*light == NULL) ? 0 : index + 1;
    if (index >= R3D_MOD_LIGHT.arrays[array_type].count) return false;
    *light = &R3D_MOD_LIGHT.lights[R3D_MOD_LIGHT.arrays[array_type].lights[index]];

    return true;
}

void r3d_light_update_matrix(r3d_light_t* light)
{
    light->state.matrixShouldBeUpdated = true;
}

void r3d_light_enable_shadows(r3d_light_t* light, int resolution)
{
    if (light->shadowMap.fbo == 0 || light->shadowMap.resolution == resolution) {
        switch (light->type) {
        case R3D_LIGHT_DIR:
        case R3D_LIGHT_SPOT:
            light->shadowMap = alloc_shadow_map_2d(resolution);
            break;
        case R3D_LIGHT_OMNI:
            light->shadowMap = alloc_shadow_map_cube(resolution);
            break;
        }
    }

    light->shadow = true;
    light->state.shadowShouldBeUpdated = true;
    light->shadowTexelSize = 1.0f / resolution;
    light->shadowSoftness = 4.0f * light->shadowTexelSize;
}

void r3d_light_update_and_cull(const r3d_frustum_t* viewFrustum, Vector3 viewPosition)
{
    r3d_light_array_t* visibleLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VISIBLE];
    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];

    visibleLights->count = 0;

    for (int i = 0; i < validLights->count; i++)
    {
        R3D_Light index = validLights->lights[i];
        r3d_light_t* light = &R3D_MOD_LIGHT.lights[index];

        if (!light->enabled) continue;

        if (light->shadow) {
            update_light_shadow_state(light);
        }

        bool isDirectional = (light->type == R3D_LIGHT_DIR);
        bool shouldUpdateMatrix = isDirectional 
            ? light->state.shadowShouldBeUpdated
            : light->state.matrixShouldBeUpdated;

        if (shouldUpdateMatrix) {
            update_light_matrix(light, viewPosition);
            if (light->shadow) update_light_frustum(light);
            if (!isDirectional) update_light_bounding_box(light);
            light->state.matrixShouldBeUpdated = false;
        }

        if (r3d_frustum_is_aabb_in(viewFrustum, &light->aabb)) {
            visibleLights->lights[visibleLights->count++] = index;
        }
    }
}

bool r3d_light_shadow_should_be_upadted(r3d_light_t* light, bool willBeUpdated)
{
    bool shadowShouldBeUpdated = light->state.shadowShouldBeUpdated;

    if (willBeUpdated) {
        switch (light->state.shadowUpdate) {
        case R3D_SHADOW_UPDATE_MANUAL:
            light->state.shadowShouldBeUpdated = false;
            break;
        case R3D_SHADOW_UPDATE_INTERVAL:
            light->state.shadowShouldBeUpdated = false;
            break;
        case R3D_SHADOW_UPDATE_CONTINUOUS:
            break;
        }
    }

    return shadowShouldBeUpdated;
}
