/* r3d_light.c -- Internal R3D light module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_light.h"
#include <r3d_config.h>
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <float.h>

// ========================================
// CONSTANTS
// ========================================

#define SHADOW_DIR_LAYER_GROWTH     2
#define SHADOW_SPOT_LAYER_GROWTH    4
#define SHADOW_OMNI_LAYER_GROWTH    4

// ========================================
// MODULE STATE
// ========================================

struct r3d_light R3D_MOD_LIGHT;

// ========================================
// CONSTANTS
// ========================================

static const GLenum SHADOW_TEXTURE_TARGET[] = {
    [R3D_LIGHT_DIR]  = GL_TEXTURE_2D_ARRAY,
    [R3D_LIGHT_SPOT] = GL_TEXTURE_2D_ARRAY,
    [R3D_LIGHT_OMNI] = GL_TEXTURE_CUBE_MAP_ARRAY,
};

static const int SHADOW_LAYER_GROWTH[] = {
    [R3D_LIGHT_DIR]  = SHADOW_DIR_LAYER_GROWTH,
    [R3D_LIGHT_SPOT] = SHADOW_SPOT_LAYER_GROWTH,
    [R3D_LIGHT_OMNI] = SHADOW_OMNI_LAYER_GROWTH,
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

static bool shadow_array_allocate(GLuint texture, GLenum target, int size, int layers)
{
    int actualLayers = (target == GL_TEXTURE_CUBE_MAP_ARRAY) ? layers * 6 : layers;

    glBindTexture(target, texture);
    glTexImage3D(
        target, 0, GL_DEPTH_COMPONENT16, size, size, actualLayers,
        0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL
    );

    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (target == GL_TEXTURE_CUBE_MAP_ARRAY) {
        glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }

    glTexParameteri(target, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(target, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    glBindTexture(target, 0);
    return true;
}

static bool shadow_array_resize(GLuint* texture, GLenum target, int size, int oldLayers, int newLayers)
{
    GLuint newTexture;
    glGenTextures(1, &newTexture);

    if (!shadow_array_allocate(newTexture, target, size, newLayers)) {
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

static bool shadow_array_expand_capacity(R3D_LightType type)
{
    r3d_light_shadow_pool_t* pool = &R3D_MOD_LIGHT.shadowPools[type];
    GLuint* shadowArray = &R3D_MOD_LIGHT.shadowArrays[type];
    GLenum shadowTarget = SHADOW_TEXTURE_TARGET[type];
    int shadowSize = R3D_LIGHT_SHADOW_SIZE[type];
    int growth = SHADOW_LAYER_GROWTH[type];

    if (!shadow_array_resize(shadowArray, shadowTarget, shadowSize, pool->totalLayers, pool->totalLayers + growth)) {
        return false;
    }

    return shadow_pool_expand(pool, growth);
}

// ========================================
// LIGHT FUNCTIONS
// ========================================

static bool light_init(r3d_light_t* light, R3D_LightType type)
{
    if (type < 0 || type >= R3D_LIGHT_TYPE_COUNT) {
        return false;
    }

    memset(light, 0, sizeof(*light));

    light->state = (r3d_light_state_t) {
        .shadowUpdate = R3D_SHADOW_UPDATE_INTERVAL,
        .shadowShouldBeUpdated = true,
        .matrixShouldBeUpdated = true,
        .shadowUpdateInterval = 0.016f,
        .shadowUpdateTimer = 0.0f
    };

    light->shadowLayer = -1;
    
    light->aabb.min = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    light->aabb.max = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};

    light->color = (Vector3) {1, 1, 1};
    light->position = (Vector3) {0};
    light->direction = (Vector3) {0, 0, -1};

    light->energy = 1.0f;
    light->specular = 0.5f;
    light->range = 50.0f;
    light->falloff = 1.0f;
    light->innerCutOff = cosf(22.5f * DEG2RAD);
    light->outerCutOff = cosf(45.0f * DEG2RAD);
    light->fogEnergy = 1.0f;

    light->shadowSoftness = 4.0f / R3D_LIGHT_SHADOW_SIZE[type];
    light->shadowOpacity = 1.0f;
    switch (type) {
    case R3D_LIGHT_DIR:
        light->shadowDepthBias = 4.0f / R3D_LIGHT_SHADOW_SIZE[type];
        light->shadowSlopeBias = 6.0f / R3D_LIGHT_SHADOW_SIZE[type];
        break;
    case R3D_LIGHT_SPOT:
        light->shadowDepthBias = 0.25f / R3D_LIGHT_SHADOW_SIZE[type];
        light->shadowSlopeBias = 1.0f / R3D_LIGHT_SHADOW_SIZE[type];
        break;
    case R3D_LIGHT_OMNI:
        light->shadowDepthBias = 0.025f;
        light->shadowSlopeBias = 0.1f;
        break;
    default:
        break;
    }
    light->casterMask = R3D_LAYER_ALL;

    light->type = type;
    light->enabled = false;

    return true;
}

static void light_update_shadow_state(r3d_light_t* light)
{
    switch (light->state.shadowUpdate) {
    case R3D_SHADOW_UPDATE_MANUAL:
        break;
    case R3D_SHADOW_UPDATE_INTERVAL:
        if (!light->state.shadowShouldBeUpdated) {
            light->state.shadowUpdateTimer += GetFrameTime();
            if (light->state.shadowUpdateTimer >= light->state.shadowUpdateInterval) {
                light->state.shadowUpdateTimer -= light->state.shadowUpdateInterval;
                light->state.shadowShouldBeUpdated = true;
            }
        }
        break;
    case R3D_SHADOW_UPDATE_CONTINUOUS:
        light->state.shadowShouldBeUpdated = true;
        break;
    }
}

static void light_update_dir_matrix(r3d_light_t* light, R3D_Camera camera, double aspect)
{
    assert(light->type == R3D_LIGHT_DIR);

    float camNear = light->range / 1000.0f;
    float camFar = light->range;
    float camFovy = (float)camera.fovy;
    float camAspect = (float)aspect;

    float farH = camFar * tanf(camFovy * (DEG2RAD * 0.5f));
    float halfDepth = (camFar - camNear) * 0.5f;
    float radius = sqrtf(farH * farH * (1.0f + camAspect * camAspect) + halfDepth * halfDepth);

    Vector3 forward = R3D_GetCameraForward(camera);
    Vector3 frustumCenter = Vector3Add(camera.position, Vector3Scale(forward, (camNear + camFar) * 0.5f));

    Vector3 lightDir = Vector3Normalize(light->direction);
    float ax = fabsf(lightDir.x), ay = fabsf(lightDir.y), az = fabsf(lightDir.z);
    Vector3 up = (ax <= ay && ax <= az) ? (Vector3){1,0,0} : (ay <= az) ? (Vector3){0,1,0} : (Vector3){0,0,1};
    Vector3 lightRight = Vector3Normalize(Vector3CrossProduct(up, lightDir));
    Vector3 lightUp = Vector3CrossProduct(lightDir, lightRight);

    float texelSize = (radius * 2.0f) / R3D_SHADOW_MAP_DIRECTIONAL_SIZE;
    float cx = floorf(Vector3DotProduct(frustumCenter, lightRight) / texelSize) * texelSize;
    float cy = floorf(Vector3DotProduct(frustumCenter, lightUp) / texelSize) * texelSize;
    float cz = Vector3DotProduct(frustumCenter, lightDir);

    Vector3 snappedCenter = Vector3Add(
        Vector3Add(
            Vector3Scale(lightRight, cx),
            Vector3Scale(lightUp, cy)
        ),
        Vector3Scale(lightDir, cz)
    );

    const float zExtension = 100.0f; // Extent to capture objects behind the camera
    Vector3 eye = Vector3Subtract(snappedCenter, Vector3Scale(lightDir, radius + zExtension));
    Matrix view = MatrixLookAt(eye, snappedCenter, lightUp);

    light->near = 0.0f;
    light->far = zExtension + radius * 2.0f;
    light->viewProj[0] = MatrixMultiply(view, MatrixOrtho(-radius, radius, -radius, radius, light->near, light->far));
}

static void light_update_spot_matrix(r3d_light_t* light)
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

static void light_update_omni_matrix(r3d_light_t* light)
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

static void light_update_matrix(r3d_light_t* light, R3D_Camera camera, double aspect)
{
    switch (light->type) {
    case R3D_LIGHT_DIR:
        light_update_dir_matrix(light, camera, aspect);
        break;
    case R3D_LIGHT_SPOT:
        light_update_spot_matrix(light);
        break;
    case R3D_LIGHT_OMNI:
        light_update_omni_matrix(light);
        break;
    default:
        break;
    }
}

static void light_update_frustum(r3d_light_t* light)
{
    int faceCount = (light->type == R3D_LIGHT_OMNI) ? 6 : 1;
    for (int i = 0; i < faceCount; i++) {
        light->frustum[i] = R3D_ComputeFrustum(light->viewProj[i]);
    }
}

static void light_update_bounding_box(r3d_light_t* light)
{
    switch (light->type) {
    case R3D_LIGHT_OMNI:
        light->aabb.min = Vector3AddValue(light->position, -light->range);
        light->aabb.max = Vector3AddValue(light->position, +light->range);
        break;
    case R3D_LIGHT_SPOT:
        light->aabb = R3D_ComputeFrustumBoundingBox(MatrixInvert(light->viewProj[0]));
        break;
    case R3D_LIGHT_DIR:
        light->aabb.min = (Vector3) {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        light->aabb.max = (Vector3) {+FLT_MAX, +FLT_MAX, +FLT_MAX};
        break;
    default:
        break;
    }
}

static const char* light_get_type_name(R3D_LightType type)
{
    switch (type) {
    case R3D_LIGHT_DIR:  return "Directional";
    case R3D_LIGHT_SPOT: return "Spot";
    case R3D_LIGHT_OMNI: return "Omni";
    default: break;
    }
    return NULL;
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
        if (!shadow_pool_init(&R3D_MOD_LIGHT.shadowPools[i], SHADOW_LAYER_GROWTH[i])) {
            R3D_TRACELOG(LOG_FATAL, "Failed to init shadow pool number %i", i);
            r3d_light_quit();
            return false;
        }
    }

    // Allocate light arrays
    R3D_MOD_LIGHT.pool = r3d_pool_create(sizeof(r3d_light_t), 32);
    if (!R3D_MOD_LIGHT.pool) {
        R3D_TRACELOG(LOG_FATAL, "Failed to allocate light array");
        r3d_light_quit();
        return false;
    }

    R3D_MOD_LIGHT.visible = RL_MALLOC(32 * sizeof(R3D_Light));
    R3D_MOD_LIGHT.visibleCapacity = 32;
    R3D_MOD_LIGHT.visibleCount = 0;

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

    r3d_pool_destroy(R3D_MOD_LIGHT.pool);
    RL_FREE(R3D_MOD_LIGHT.visible);
}

R3D_Light r3d_light_new(R3D_LightType type)
{
    R3D_Light id = r3d_pool_insert(&R3D_MOD_LIGHT.pool);
    if (id == R3D_POOL_ID_NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to insert light into pool");
        return R3D_POOL_ID_NULL;
    }

    r3d_light_t* light = r3d_pool_get(R3D_MOD_LIGHT.pool, id);
    if (!light_init(light, type)) {
        R3D_TRACELOG(LOG_ERROR, "Failed to initialize light");
        r3d_pool_remove(R3D_MOD_LIGHT.pool, id);
        return R3D_POOL_ID_NULL;
    }

    R3D_TRACELOG(LOG_INFO, "[ID %d] Light created successfully (type: %s)", id, light_get_type_name(type));

    return id;
}

void r3d_light_delete(R3D_Light id)
{
    r3d_light_t* light = r3d_pool_get(R3D_MOD_LIGHT.pool, id);
    if (!light) return;

    if (light->shadowLayer >= 0) {
        shadow_pool_release(&R3D_MOD_LIGHT.shadowPools[light->type], light->shadowLayer);
    }
    r3d_pool_remove(R3D_MOD_LIGHT.pool, id);

    R3D_TRACELOG(LOG_INFO, "[ID %d] Light destroyed", id);
}

bool r3d_light_is_valid(R3D_Light id)
{
    return r3d_pool_valid(R3D_MOD_LIGHT.pool, id);
}

r3d_light_t* r3d_light_get(R3D_Light id)
{
    return r3d_pool_get(R3D_MOD_LIGHT.pool, id);
}

r3d_rect_t r3d_light_get_screen_rect(const r3d_light_t* light, const Matrix* viewProj, int w, int h)
{
    assert(light->type != R3D_LIGHT_DIR);

    Vector3 min = light->aabb.min;
    Vector3 max = light->aabb.max;

    Vector2 minNDC = {+FLT_MAX, +FLT_MAX};
    Vector2 maxNDC = {-FLT_MAX, -FLT_MAX};

    for (int i = 0; i < 8; i++) {
        Vector4 corner = {
            (i & 1) ? max.x : min.x,
            (i & 2) ? max.y : min.y,
            (i & 4) ? max.z : min.z,
            1.0f
        };
        Vector4 clip = r3d_vector4_transform(corner, viewProj);

        // If the AABB crosses the near plane: fullscreen
        if (clip.w <= 0.0f) {
            return (r3d_rect_t){0, 0, w, h};
        }

        Vector2 ndc = Vector2Scale((Vector2){clip.x, clip.y}, 1.0f / clip.w);
        minNDC = Vector2Min(minNDC, ndc);
        maxNDC = Vector2Max(maxNDC, ndc);
    }

    // NDC to screen
    int x = (int)fmaxf((minNDC.x * 0.5f + 0.5f) * w, 0.0f);
    int y = (int)fmaxf((minNDC.y * 0.5f + 0.5f) * h, 0.0f);
    int rectW = (int)fminf((maxNDC.x * 0.5f + 0.5f) * w, (float)w) - x;
    int rectH = (int)fminf((maxNDC.y * 0.5f + 0.5f) * h, (float)h) - y;

    // Security: Invalid dimensions = skip
    if (rectW <= 0 || rectH <= 0) {
        return (r3d_rect_t){0, 0, 0, 0};
    }

    return (r3d_rect_t){x, y, rectW, rectH};
}

bool r3d_light_enable_shadows(r3d_light_t* light)
{
    if (light->shadowLayer >= 0) return true;

    int layer = shadow_pool_reserve(&R3D_MOD_LIGHT.shadowPools[light->type]);
    if (layer < 0) {
        if (!shadow_array_expand_capacity(light->type)) {
            return false;
        }
        layer = shadow_pool_reserve(&R3D_MOD_LIGHT.shadowPools[light->type]);
    }

    light->state.shadowShouldBeUpdated = true;
    light->shadowLayer = layer;

    return true;
}

void r3d_light_disable_shadows(r3d_light_t* light)
{
    if (light->shadowLayer >= 0) {
        shadow_pool_release(&R3D_MOD_LIGHT.shadowPools[light->type], light->shadowLayer);
        light->shadowLayer = -1;
    }
}

void r3d_light_update_and_cull(const R3D_Frustum* viewFrustum, R3D_Camera camera,
                               double aspect, bool* hasVisibleShadows)
{
    R3D_MOD_LIGHT.visibleCount = 0;

    R3D_POOL_FOR_EACH(R3D_MOD_LIGHT.pool, r3d_light_t, light, idx) {
        if (!light->enabled) continue;

        if (light->shadowLayer >= 0) {
            light_update_shadow_state(light);
        }

        bool isDirectional = (light->type == R3D_LIGHT_DIR);
        bool shouldUpdateMatrix = isDirectional
            ? light->state.shadowShouldBeUpdated
            : light->state.matrixShouldBeUpdated;

        if (shouldUpdateMatrix) {
            light_update_matrix(light, camera, aspect);
            light_update_frustum(light);
            if (!isDirectional) light_update_bounding_box(light);
            light->state.matrixShouldBeUpdated = false;
        }

        if (!R3D_FrustumIntersectsBoundingBox(viewFrustum, light->aabb)) continue;
        if (hasVisibleShadows) *hasVisibleShadows |= (light->shadowLayer >= 0);

        // Grow visible array if needed
        if (R3D_MOD_LIGHT.visibleCount >= R3D_MOD_LIGHT.visibleCapacity) {
            uint32_t newCap = R3D_MOD_LIGHT.visibleCapacity * 2;
            R3D_Light* newPtr = RL_REALLOC(R3D_MOD_LIGHT.visible, newCap * sizeof(R3D_Light));
            if (!newPtr) continue; // Skip rather than crash
            R3D_MOD_LIGHT.visible = newPtr;
            R3D_MOD_LIGHT.visibleCapacity = newCap;
        }

        R3D_MOD_LIGHT.visible[R3D_MOD_LIGHT.visibleCount++] = R3D_POOL_ID_MAKE(idx);
    }
}

bool r3d_light_shadow_should_be_updated(r3d_light_t* light, bool willBeUpdated)
{
    // If the light does not have a shadow map assigned, we return
    if (light->shadowLayer < 0) return false;

    // If the shadow opacity is set to zero, the update is ignored
    if (light->shadowOpacity == 0.0f) return false;

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
