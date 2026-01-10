/* r3d_light.c -- Internal R3D light module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_light.h"
#include "r3d/r3d_lighting.h"
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
// CONSTANTS
// ========================================

static const GLenum R3D_LIGHT_SHADOW_TARGET[] = {
    [R3D_LIGHT_DIR]  = GL_TEXTURE_2D_ARRAY,
    [R3D_LIGHT_SPOT] = GL_TEXTURE_2D_ARRAY,
    [R3D_LIGHT_OMNI] = GL_TEXTURE_CUBE_MAP_ARRAY,
};

static const int R3D_LIGHT_SHADOW_SIZE[] = {
    [R3D_LIGHT_DIR]  = R3D_LIGHT_SHADOW_DIR_SIZE,
    [R3D_LIGHT_SPOT] = R3D_LIGHT_SHADOW_SPOT_SIZE,
    [R3D_LIGHT_OMNI] = R3D_LIGHT_SHADOW_OMNI_SIZE,
};

static const int R3D_LIGHT_SHADOW_GROWTH[] = {
    [R3D_LIGHT_DIR]  = R3D_LIGHT_SHADOW_DIR_GROWTH,
    [R3D_LIGHT_SPOT] = R3D_LIGHT_SHADOW_SPOT_GROWTH,
    [R3D_LIGHT_OMNI] = R3D_LIGHT_SHADOW_OMNI_GROWTH,
};

// ========================================
// SHADOW LAYER POOL FUNCTIONS
// ========================================

static bool shadow_pool_init(r3d_light_shadow_pool_t* pool, int initialCapacity)
{
    pool->freeLayers = RL_MALLOC(initialCapacity * sizeof(int));
    if (!pool->freeLayers) return false;

    pool->freeCapacity = initialCapacity;
    pool->freeCount = 0;
    pool->totalLayers = 0;

    return true;
}

static void shadow_pool_quit(r3d_light_shadow_pool_t* pool)
{
    RL_FREE(pool->freeLayers);
}

static int shadow_pool_reserve(r3d_light_shadow_pool_t* pool)
{
    if (pool->freeCount > 0) {
        return pool->freeLayers[--pool->freeCount];
    }
    return -1;  // Needs expansion
}

static void shadow_pool_release(r3d_light_shadow_pool_t* pool, int layer)
{
    if (layer < 0 || layer >= pool->totalLayers) return;
    if (pool->freeCount < pool->freeCapacity) {
        pool->freeLayers[pool->freeCount++] = layer;
    }
}

static bool shadow_pool_expand(r3d_light_shadow_pool_t* pool, int addCount)
{
    int oldTotal = pool->totalLayers;
    int newTotal = oldTotal + addCount;
    
    // Reallocate free layers array if needed
    if (pool->freeCount + addCount > pool->freeCapacity) {
        pool->freeCapacity = newTotal;
        int* newFree = RL_REALLOC(pool->freeLayers, pool->freeCapacity * sizeof(int));
        if (!newFree) return false;
        pool->freeLayers = newFree;
    }
    
    // Add new layers to free list
    for (int i = oldTotal; i < newTotal; i++) {
        pool->freeLayers[pool->freeCount++] = i;
    }
    
    pool->totalLayers = newTotal;
    return true;
}

// ========================================
// SHADOW MAP TEXTURE FUNCTIONS
// ========================================

static bool allocate_shadow_array(GLuint texture, GLenum target, int size, int layers)
{
    int actualLayers = (target == GL_TEXTURE_CUBE_MAP_ARRAY) ? layers * 6 : layers;

    glBindTexture(target, texture);
    glTexImage3D(
        target, 0, GL_DEPTH_COMPONENT16, size, size, actualLayers,
        0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL
    );

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(target, 0);
    return true;
}

static bool resize_shadow_array(GLuint* texture, GLenum target, int size, int oldLayers, int newLayers)
{
    GLuint newTexture;
    glGenTextures(1, &newTexture);

    if (!allocate_shadow_array(newTexture, target, size, newLayers)) {
        glDeleteTextures(1, &newTexture);
        return false;
    }

    // Copy existing data
    if (oldLayers > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_LIGHT.workFramebuffer);
        int facesPerLayer = (target == GL_TEXTURE_CUBE_MAP_ARRAY) ? 6 : 1;
        for (int layer = 0; layer < oldLayers; layer++) {
            for (int face = 0; face < facesPerLayer; face++) {
                int layerIndex = layer * facesPerLayer + face;
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, *texture, 0, layerIndex);
                glBindTexture(target, newTexture);
                glCopyTexSubImage3D(target, 0, 0, 0, layerIndex, 0, 0, size, size);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glDeleteTextures(1, texture);
    *texture = newTexture;
    return true;
}

static bool expand_shadow_array_capacity(R3D_LightType type)
{
    r3d_light_shadow_pool_t* pool = &R3D_MOD_LIGHT.shadowPools[type];
    GLuint* shadowArray = &R3D_MOD_LIGHT.shadowArrays[type];
    GLenum shadowTarget = R3D_LIGHT_SHADOW_TARGET[type];
    int shadowSize = R3D_LIGHT_SHADOW_SIZE[type];
    int growth = R3D_LIGHT_SHADOW_GROWTH[type];

    if (!resize_shadow_array(shadowArray, shadowTarget, shadowSize, pool->totalLayers, pool->totalLayers + growth)) {
        return false;
    }

    return shadow_pool_expand(pool, growth);
}

// ========================================
// SHADOW LAYER RESERVATION
// ========================================

static int reserve_shadow_layer(R3D_LightType type)
{
    int layer = shadow_pool_reserve(&R3D_MOD_LIGHT.shadowPools[type]);
    if (layer < 0) {
        if (!expand_shadow_array_capacity(type)) return -1;
        layer = shadow_pool_reserve(&R3D_MOD_LIGHT.shadowPools[type]);
    }
    return layer;
}

static void release_shadow_layer(r3d_light_t* light)
{
    if (light->shadowLayer >= 0) {
        shadow_pool_release(&R3D_MOD_LIGHT.shadowPools[light->type], light->shadowLayer);
        light->shadowLayer = -1;
    }
}

// ========================================
// LIGHT FUNCTIONS
// ========================================

static bool growth_light_arrays(void)
{
    int newCapacity = 2 * R3D_MOD_LIGHT.capacityLights;

    r3d_light_t* newLights = RL_REALLOC(R3D_MOD_LIGHT.lights, newCapacity * sizeof(*R3D_MOD_LIGHT.lights));
    if (!newLights) return false;

    R3D_MOD_LIGHT.capacityLights = newCapacity;
    R3D_MOD_LIGHT.lights = newLights;

    for (int i = 0; i < R3D_LIGHT_ARRAY_COUNT; i++) {
        R3D_Light* newPtr = RL_REALLOC(R3D_MOD_LIGHT.arrays[i].lights, newCapacity * sizeof(R3D_Light));
        if (!newPtr) return false;
        R3D_MOD_LIGHT.arrays[i].lights = newPtr;
    }

    return true;
}

static bool init_light(r3d_light_t* light, R3D_LightType type)
{
    if (type < 0 || type >= R3D_LIGHT_TYPE_COUNT) {
        return false;
    }

    memset(light, 0, sizeof(*light));

    light->type = type;
    light->shadowLayer = -1;
    
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

    light->enabled = false;
    light->shadow = false;

    light->state = (r3d_light_state_t) {
        .shadowUpdate = R3D_SHADOW_UPDATE_INTERVAL,
        .shadowShouldBeUpdated = true,
        .matrixShouldBeUpdated = true,
        .shadowFrequencySec = 0.016f,
        .shadowTimerSec = 0.0f
    };

    switch (type) {
    case R3D_LIGHT_DIR:
        light->shadowTexelSize = 1.0f / R3D_LIGHT_SHADOW_DIR_SIZE;
        light->shadowDepthBias = 0.0002f;
        light->shadowSlopeBias = 0.002f;
        break;
    case R3D_LIGHT_SPOT:
        light->shadowTexelSize = 1.0f / R3D_LIGHT_SHADOW_SPOT_SIZE;
        light->shadowDepthBias = 0.00002f;
        light->shadowSlopeBias = 0.0002f;
        break;
    case R3D_LIGHT_OMNI:
        light->shadowTexelSize = 1.0f / R3D_LIGHT_SHADOW_OMNI_SIZE;
        light->shadowDepthBias = 0.01f;
        light->shadowSlopeBias = 0.02f;
        break;
    default:
        break;
    }

    return true;
}

static void update_light_shadow_state(r3d_light_t* light)
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
    assert(light->type == R3D_LIGHT_DIR);

    Vector3 lightDir = light->direction;
    float extent = light->range;

    // Create orthonormal basis
    Vector3 up = (fabsf(Vector3DotProduct(lightDir, (Vector3) {0, 1, 0})) > 0.99f) ? (Vector3) {0, 0, 1} : (Vector3) {0, 1, 0};
    Vector3 lightRight = Vector3Normalize(Vector3CrossProduct(up, lightDir));
    Vector3 lightUp = Vector3CrossProduct(lightDir, lightRight);

    // Project camera position into light space
    float camX = Vector3DotProduct(viewPosition, lightRight);
    float camY = Vector3DotProduct(viewPosition, lightUp);
    float camZ = Vector3DotProduct(viewPosition, lightDir);

    // Snap to texel grid to reduce shadow shimmering
    float worldUnitsPerTexel = (2.0f * extent) / R3D_LIGHT_SHADOW_DIR_SIZE;
    float snappedX = floorf(camX / worldUnitsPerTexel) * worldUnitsPerTexel;
    float snappedY = floorf(camY / worldUnitsPerTexel) * worldUnitsPerTexel;

    // Reconstruct snapped world position
    Vector3 lightPosition = {
        lightRight.x * snappedX + lightUp.x * snappedY + lightDir.x * camZ,
        lightRight.y * snappedX + lightUp.y * snappedY + lightDir.y * camZ,
        lightRight.z * snappedX + lightUp.z * snappedY + lightDir.z * camZ
    };

    Matrix view = MatrixLookAt(lightPosition, Vector3Add(lightPosition, lightDir), lightUp);
    Matrix proj = MatrixOrtho(-extent, extent, -extent, extent, -extent, extent);
    light->viewProj[0] = MatrixMultiply(view, proj);

    light->near = -extent;
    light->far = extent;
}

static void update_light_spot_matrix(r3d_light_t* light)
{
    assert(light->type == R3D_LIGHT_SPOT);

    light->near = 0.05f;
    light->far = light->range;

    Vector3 up = {0, 1, 0};
    float upDot = fabsf(Vector3DotProduct(light->direction, up));
    if (upDot > 0.99f) up = (Vector3){1, 0, 0};

    Matrix view = MatrixLookAt(light->position, Vector3Add(light->position, light->direction), up);
    Matrix proj = MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);
    light->viewProj[0] = MatrixMultiply(view, proj);
}

static void update_light_omni_matrix(r3d_light_t* light)
{
    assert(light->type == R3D_LIGHT_OMNI);

    static const Vector3 dirs[6] = {
        {  1.0,  0.0,  0.0 }, { -1.0,  0.0,  0.0 },
        {  0.0,  1.0,  0.0 }, {  0.0, -1.0,  0.0 },
        {  0.0,  0.0,  1.0 }, {  0.0,  0.0, -1.0 }
    };

    static const Vector3 ups[6] = {
        {  0.0, -1.0,  0.0 }, {  0.0, -1.0,  0.0 },
        {  0.0,  0.0,  1.0 }, {  0.0,  0.0, -1.0 },
        {  0.0, -1.0,  0.0 }, {  0.0, -1.0,  0.0 }
    };

    light->near = 0.05f;
    light->far = light->range;

    Matrix proj = MatrixPerspective(90 * DEG2RAD, 1.0, light->near, light->far);

    for (int face = 0; face < 6; face++) {
        Vector3 target = Vector3Add(light->position, dirs[face]);
        Matrix view = MatrixLookAt(light->position, target, ups[face]);
        light->viewProj[face] = MatrixMultiply(view, proj);
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
    default:
        break;
    }
}

static void update_light_frustum(r3d_light_t* light)
{
    int faceCount = (light->type == R3D_LIGHT_OMNI) ? 6 : 1;
    for (int i = 0; i < faceCount; i++) {
        light->frustum[i] = r3d_frustum_create(light->viewProj[i]);
    }
}

static void update_light_bounding_box(r3d_light_t* light)
{
    switch (light->type) {
    case R3D_LIGHT_OMNI:
        light->aabb.min = Vector3AddValue(light->position, -light->range);
        light->aabb.max = Vector3AddValue(light->position, +light->range);
        break;
    case R3D_LIGHT_SPOT:
        light->aabb = r3d_frustum_get_bounding_box(light->viewProj[0]);
        break;
    case R3D_LIGHT_DIR:
        light->aabb.min = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        light->aabb.max = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
        break;
    default:
        break;
    }
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_light_init(void)
{
    memset(&R3D_MOD_LIGHT, 0, sizeof(R3D_MOD_LIGHT));

    glGenFramebuffers(1, &R3D_MOD_LIGHT.workFramebuffer);
    glGenTextures(R3D_LIGHT_TYPE_COUNT, R3D_MOD_LIGHT.shadowArrays);

    // Configure the framebuffer to only consider the depth
    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_LIGHT.workFramebuffer);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Initialize shadow pools
    for (int i = 0; i < R3D_LIGHT_TYPE_COUNT; i++) {
        if (!shadow_pool_init(&R3D_MOD_LIGHT.shadowPools[i], R3D_LIGHT_SHADOW_GROWTH[i])) {
            TraceLog(LOG_FATAL, "R3D: Failed to init shadow pool number %i", i);
            r3d_light_quit();
            return false;
        }
    }

    // Allocate light arrays
    R3D_MOD_LIGHT.lights = RL_MALLOC(R3D_LIGHT_INITIAL_CAP * sizeof(*R3D_MOD_LIGHT.lights));
    R3D_MOD_LIGHT.capacityLights = R3D_LIGHT_INITIAL_CAP;

    if (!R3D_MOD_LIGHT.lights) {
        TraceLog(LOG_FATAL, "R3D: Failed to allocate light array");
        r3d_light_quit();
        return false;
    }

    for (int i = 0; i < R3D_LIGHT_ARRAY_COUNT; i++) {
        R3D_MOD_LIGHT.arrays[i].lights = RL_MALLOC(R3D_LIGHT_INITIAL_CAP * sizeof(R3D_Light));
        if (!R3D_MOD_LIGHT.arrays[i].lights) {
            TraceLog(LOG_FATAL, "R3D: Failed to allocate light list array %i", i);
            r3d_light_quit();
            return false;
        }
    }

    return true;
}

void r3d_light_quit(void)
{
    if (R3D_MOD_LIGHT.workFramebuffer) {
        glDeleteFramebuffers(1, &R3D_MOD_LIGHT.workFramebuffer);
    }

    for (int i = 0; i < R3D_LIGHT_TYPE_COUNT; i++) {
        if (R3D_MOD_LIGHT.shadowArrays[i]) {
            glDeleteTextures(1, &R3D_MOD_LIGHT.shadowArrays[i]);
        }
        shadow_pool_quit(&R3D_MOD_LIGHT.shadowPools[i]);
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

    // Get index (from free list or new)
    R3D_Light index = (freeLights->count > 0) 
        ? freeLights->lights[--freeLights->count]
        : validLights->count;

    // Grow if needed
    if (index >= R3D_MOD_LIGHT.capacityLights && !growth_light_arrays()) {
        TraceLog(LOG_ERROR, "R3D: Failed to grow light arrays");
        goto error_restore_free;
    }

    // Initialize light
    if (!init_light(&R3D_MOD_LIGHT.lights[index], type)) {
        TraceLog(LOG_ERROR, "R3D: Failed to initialize light (type: %d)", type);
        goto error_restore_free;
    }

    // Add to valid array
    validLights->lights[validLights->count++] = index;
    return index;

error_restore_free:
    // Restore free list if we took from it
    if (index < validLights->count) {
        freeLights->lights[freeLights->count++] = index;
    }
    return -1;
}

void r3d_light_delete(R3D_Light index)
{
    if (index < 0) return;

    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];

    for (int i = 0; i < validLights->count; i++) {
        if (index == validLights->lights[i]) {
            int numToMove = validLights->count - i - 1;
            if (numToMove > 0) {
                memmove(
                    &validLights->lights[i], &validLights->lights[i + 1],
                    numToMove * sizeof(validLights->lights[0])
                );
            }
            validLights->count--;

            // Release shadow layer and add to free list
            r3d_light_t* light = &R3D_MOD_LIGHT.lights[index];
            release_shadow_layer(light);

            r3d_light_array_t* freeLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_FREE];
            freeLights->lights[freeLights->count++] = index;
            return;
        }
    }
}

bool r3d_light_is_valid(R3D_Light index)
{
    if (index < 0) return false;

    const r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];
    for (int i = 0; i < validLights->count; i++) {
        if (index == validLights->lights[i]) return true;
    }
    return false;
}

r3d_light_t* r3d_light_get(R3D_Light index)
{
    return r3d_light_is_valid(index) ? &R3D_MOD_LIGHT.lights[index] : NULL;
}

r3d_rect_t r3d_light_get_screen_rect(const r3d_light_t* light, const Matrix* viewProj, int w, int h)
{
    assert(light->type != R3D_LIGHT_DIR);

    Vector3 min = light->aabb.min;
    Vector3 max = light->aabb.max;

    Vector2 minNDC = {+FLT_MAX, +FLT_MAX};
    Vector2 maxNDC = {-FLT_MAX, -FLT_MAX};

    bool allInside = true;
    for (int i = 0; i < 8; i++) {
        Vector4 corner = {
            (i & 1) ? max.x : min.x,
            (i & 2) ? max.y : min.y,
            (i & 4) ? max.z : min.z,
            1.0f
        };
        Vector4 clip = r3d_vector4_transform(corner, viewProj);
        if (clip.w <= 0.0f) {
            allInside = false;
            break;
        }

        Vector2 ndc = Vector2Scale((Vector2){clip.x, clip.y}, 1.0f / clip.w);
        minNDC = Vector2Min(minNDC, ndc);
        maxNDC = Vector2Max(maxNDC, ndc);
    }

    int x = 0, y = 0;
    if (allInside) {
        x = (int)fmaxf((minNDC.x * 0.5f + 0.5f) * w, 0.0f);
        y = (int)fmaxf((minNDC.y * 0.5f + 0.5f) * h, 0.0f);
        w = (int)fminf((maxNDC.x * 0.5f + 0.5f) * w, (float)w) - x;
        h = (int)fminf((maxNDC.y * 0.5f + 0.5f) * h, (float)h) - y;
        assert(w > 0 && h > 0); // should never happen if calculated for visible lights
    }

    return (r3d_rect_t) {x, y, w, h};
}

bool r3d_light_iter(r3d_light_t** light, r3d_light_array_enum_t array)
{
    static int index = 0;
    index = (*light == NULL) ? 0 : index + 1;

    if (index >= R3D_MOD_LIGHT.arrays[array].count) return false;

    *light = &R3D_MOD_LIGHT.lights[R3D_MOD_LIGHT.arrays[array].lights[index]];
    return true;
}

void r3d_light_enable_shadows(r3d_light_t* light)
{
    if (light->shadow) return;

    int layer = reserve_shadow_layer(light->type);
    if (layer < 0) {
        TraceLog(LOG_ERROR, "R3D: Failed to reserve shadow layer for light");
        return;
    }

    light->shadowSoftness = 4.0f * light->shadowTexelSize;
    light->state.shadowShouldBeUpdated = true;
    light->shadowLayer = layer;
    light->shadow = true;
}

void r3d_light_disable_shadows(r3d_light_t* light)
{
    if (!light->shadow) return;

    release_shadow_layer(light);
    light->shadow = false;
}

void r3d_light_update_and_cull(const r3d_frustum_t* viewFrustum, Vector3 viewPosition)
{
    r3d_light_array_t* visibleLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VISIBLE];
    r3d_light_array_t* validLights = &R3D_MOD_LIGHT.arrays[R3D_LIGHT_ARRAY_VALID];

    visibleLights->count = 0;

    for (int i = 0; i < validLights->count; i++) {
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
            update_light_frustum(light);
            if (!isDirectional) {
                update_light_bounding_box(light);
            }
            light->state.matrixShouldBeUpdated = false;
        }

        if (r3d_frustum_is_aabb_in(viewFrustum, &light->aabb)) {
            visibleLights->lights[visibleLights->count++] = index;
        }
    }
}

bool r3d_light_shadow_should_be_updated(r3d_light_t* light, bool willBeUpdated)
{
    bool shouldUpdate = light->state.shadowShouldBeUpdated;

    if (willBeUpdated) {
        switch (light->state.shadowUpdate) {
        case R3D_SHADOW_UPDATE_MANUAL:
        case R3D_SHADOW_UPDATE_INTERVAL:
            light->state.shadowShouldBeUpdated = false;
            break;
        default:
            break;
        }
    }

    return shouldUpdate;
}

void r3d_light_shadow_bind_fbo(R3D_LightType type, int layer, int face)
{
    assert((type == R3D_LIGHT_OMNI && face >= 0 && face < 6) || (type != R3D_LIGHT_OMNI && face == 0));

    GLuint shadowArray = R3D_MOD_LIGHT.shadowArrays[type];
    int shadowSize = R3D_LIGHT_SHADOW_SIZE[type];
    int stride = (type == R3D_LIGHT_OMNI) ? 6 : 1;

    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_LIGHT.workFramebuffer);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadowArray, 0, layer * stride + face);
    glViewport(0, 0, shadowSize, shadowSize);
}

GLuint r3d_light_shadow_get(R3D_LightType type)
{
    return R3D_MOD_LIGHT.shadowArrays[type];
}
