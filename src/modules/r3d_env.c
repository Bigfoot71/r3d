/* r3d_env.c -- Internal R3D environment module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_env.h"

#include <r3d/r3d_frustum.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/r3d_helper.h"

// ========================================
// CONSTANTS
// ========================================

#define LAYER_GROWTH    4

// ========================================
// MODULE STATE
// ========================================

struct r3d_env R3D_MOD_ENV;

// ========================================
// LAYER POOL FUNCTIONS
// ========================================

static bool layer_pool_init(r3d_env_layer_pool_t* pool, int initialCapacity)
{
    pool->freeCapacity = initialCapacity * 2;
    pool->freeLayers = RL_MALLOC(pool->freeCapacity * sizeof(int));
    pool->freeCount = 0;
    pool->totalLayers = 0;
    return (pool->freeLayers != NULL);
}

static void layer_pool_quit(r3d_env_layer_pool_t* pool)
{
    RL_FREE(pool->freeLayers);
    memset(pool, 0, sizeof(*pool));
}

static int layer_pool_reserve(r3d_env_layer_pool_t* pool)
{
    if (pool->freeCount > 0) {
        return pool->freeLayers[--pool->freeCount];
    }
    return -1;  // Needs expansion
}

static void layer_pool_release(r3d_env_layer_pool_t* pool, int layer)
{
    if (layer < 0 || layer >= pool->totalLayers) return;
    if (pool->freeCount < pool->freeCapacity) {
        pool->freeLayers[pool->freeCount++] = layer;
    }
}

static bool layer_pool_expand(r3d_env_layer_pool_t* pool, int addCount)
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
// TEXTURE FUNCTIONS
// ========================================

static bool alloc_depth_stencil_renderbuffer(GLuint renderbuffer, int size)
{
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, size, size);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    return true;
}

// ========================================
// CUBEMAP FUNCTIONS
// ========================================

typedef struct {
    int size;
    int layers;
    int mipLevels;
    GLenum target;
} cubemap_array_spec_t;

static inline cubemap_array_spec_t cubemap_array_spec(int size, int layers, bool mipmapped)
{
    return (cubemap_array_spec_t) {
        .size = size,
        .layers = layers,
        .mipLevels = mipmapped ? r3d_get_mip_levels_1d(size) : 1,
        .target = (layers > 0) ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP
    };
}

static bool cubemap_array_allocate(GLuint texture, cubemap_array_spec_t spec)
{
    glBindTexture(spec.target, texture);

    for (int level = 0; level < spec.mipLevels; level++) {
        int mipSize = spec.size >> level;
        if (mipSize < 1) mipSize = 1;

        if (spec.target == GL_TEXTURE_CUBE_MAP_ARRAY) {
            glTexImage3D(
                spec.target, level, GL_RGB16F,
                mipSize, mipSize, spec.layers * 6,
                0, GL_RGB, GL_FLOAT, NULL
            );
        }
        else {
            for (int face = 0; face < 6; face++) {
                glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, GL_RGB16F,
                    mipSize, mipSize, 0, GL_RGB, GL_FLOAT, NULL
                );
            }
        }
    }

    GLenum minFilter = (spec.mipLevels > 1) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    glTexParameteri(spec.target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(spec.target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(spec.target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(spec.target, GL_TEXTURE_MIN_FILTER, minFilter);
    glTexParameteri(spec.target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(spec.target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(spec.target, GL_TEXTURE_MAX_LEVEL, spec.mipLevels - 1);

    glBindTexture(spec.target, 0);
    return true;
}

static bool cubemap_array_resize(GLuint* texture, cubemap_array_spec_t oldSpec, cubemap_array_spec_t newSpec)
{
    GLuint newTexture;
    glGenTextures(1, &newTexture);

    if (!cubemap_array_allocate(newTexture, newSpec)) {
        glDeleteTextures(1, &newTexture);
        return false;
    }

    if (oldSpec.layers > 0 && *texture != 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.workFramebuffer);
        for (int level = 0; level < oldSpec.mipLevels; level++) {
            int mipSize = oldSpec.size >> level;
            if (mipSize < 1) mipSize = 1;
            for (int layer = 0; layer < oldSpec.layers; layer++) {
                for (int face = 0; face < 6; face++) {
                    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, *texture, level, layer * 6 + face);
                    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, newTexture);
                    glCopyTexSubImage3D(
                        GL_TEXTURE_CUBE_MAP_ARRAY, level,
                        0, 0, layer * 6 + face,
                        0, 0, mipSize, mipSize
                    );
                }
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
    }

    if (*texture != 0) {
        glDeleteTextures(1, texture);
    }
    *texture = newTexture;
    
    return true;
}

static bool cubemap_array_expand_capacity(GLuint* texture, r3d_env_layer_pool_t* pool, int size, bool mipmapped)
{
    cubemap_array_spec_t oldSpec = cubemap_array_spec(size, pool->totalLayers, mipmapped);
    cubemap_array_spec_t newSpec = cubemap_array_spec(size, pool->totalLayers + LAYER_GROWTH, mipmapped);

    if (!cubemap_array_resize(texture, oldSpec, newSpec)) {
        return false;
    }

    return layer_pool_expand(pool, LAYER_GROWTH);
}

// ========================================
// PROBE FUNCTIONS
// ========================================

static bool probe_init(r3d_env_probe_t* probe, R3D_ProbeFlags flags)
{
    probe->flags = flags;
    probe->irradiance = -1;
    probe->prefilter = -1;

    if (BIT_TEST(flags, R3D_PROBE_ILLUMINATION)) {
        probe->irradiance = r3d_env_irradiance_reserve_layer();
        if (probe->irradiance == -1) return false;
    }

    if (BIT_TEST(flags, R3D_PROBE_REFLECTION)) {
        probe->prefilter = r3d_env_prefilter_reserve_layer();
        if (probe->prefilter == -1) {
            if (probe->irradiance >= 0) {
                r3d_env_irradiance_release_layer(probe->irradiance);
            }
            return false;
        }
    }

    probe->state = (r3d_env_probe_state_t) {
        .updateMode = R3D_PROBE_UPDATE_ONCE,
        .matrixShouldBeUpdated = true,
        .sceneShouldBeUpdated = true
    };

    probe->position = (Vector3) {0};
    probe->falloff = 1.0f;
    probe->range = 16.0f;
    probe->interior = false;
    probe->shadows = false;
    probe->enabled = false;

    return true;
}

static void probe_deinit(r3d_env_probe_t* probe)
{
    if (probe->irradiance >= 0) {
        r3d_env_irradiance_release_layer(probe->irradiance);
    }
    if (probe->prefilter >= 0) {
        r3d_env_prefilter_release_layer(probe->prefilter);
    }
}

static void probe_update_matrix_frustum(r3d_env_probe_t* probe)
{
    static const Vector3 dirs[6] = {
        { 1.0,  0.0,  0.0}, {-1.0,  0.0,  0.0},  // +X, -X
        { 0.0,  1.0,  0.0}, { 0.0, -1.0,  0.0},  // +Y, -Y
        { 0.0,  0.0,  1.0}, { 0.0,  0.0, -1.0}   // +Z, -Z
    };

    static const Vector3 ups[6] = {
        { 0.0, -1.0,  0.0 }, { 0.0, -1.0,  0.0},  // +X, -X
        { 0.0,  0.0,  1.0 }, { 0.0,  0.0, -1.0},  // +Y, -Y
        { 0.0, -1.0,  0.0 }, { 0.0, -1.0,  0.0}   // +Z, -Z
    };

    Matrix proj = MatrixPerspective(90 * DEG2RAD, 1.0, 0.05f, probe->range);
    probe->invProj = MatrixInvert(proj);

    for (int face = 0; face < 6; face++) {
        Vector3 target = Vector3Add(probe->position, dirs[face]);
        probe->view[face] = MatrixLookAt(probe->position, target, ups[face]);
        probe->viewProj[face] = MatrixMultiply(probe->view[face], proj);
        probe->frustum[face] = R3D_ComputeFrustum(probe->viewProj[face]);
        probe->invView[face] = MatrixInvert(probe->view[face]);
    }
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_env_init(void)
{
    memset(&R3D_MOD_ENV, 0, sizeof(R3D_MOD_ENV));

    glGenFramebuffers(1, &R3D_MOD_ENV.workFramebuffer);
    glGenFramebuffers(1, &R3D_MOD_ENV.captureFramebuffer);

    glGenTextures(1, &R3D_MOD_ENV.irradianceArray);
    glGenTextures(1, &R3D_MOD_ENV.prefilterArray);

    glGenRenderbuffers(1, &R3D_MOD_ENV.captureDepth);
    glGenTextures(1, &R3D_MOD_ENV.captureCube);

    // Initialize layer pools
    if (!layer_pool_init(&R3D_MOD_ENV.irradiancePool, LAYER_GROWTH)) {
        R3D_TRACELOG(LOG_FATAL, "Failed to init irradiance layer pool");
        r3d_env_quit();
        return false;
    }

    if (!layer_pool_init(&R3D_MOD_ENV.prefilterPool, LAYER_GROWTH)) {
        R3D_TRACELOG(LOG_FATAL, "Failed to init prefilter layer pool");
        r3d_env_quit();
        return false;
    }

    // Allocate probe arrays
    R3D_MOD_ENV.pool = r3d_pool_create(sizeof(r3d_env_probe_t), 16);
    if (!R3D_MOD_ENV.pool) {
        R3D_TRACELOG(LOG_FATAL, "Failed to create probe pool");
        r3d_env_quit();
        return false;
    }

    R3D_MOD_ENV.visible = RL_MALLOC(16 * sizeof(R3D_Probe));
    R3D_MOD_ENV.visibleCapacity = 16;
    R3D_MOD_ENV.visibleCount = 0;
    if (!R3D_MOD_ENV.visible) {
        R3D_TRACELOG(LOG_FATAL, "Failed to allocate visible probe array");
        r3d_env_quit();
        return false;
    }

    return true;
}

void r3d_env_quit(void)
{
    if (R3D_MOD_ENV.irradianceArray) glDeleteTextures(1, &R3D_MOD_ENV.irradianceArray);
    if (R3D_MOD_ENV.prefilterArray) glDeleteTextures(1, &R3D_MOD_ENV.prefilterArray);

    if (R3D_MOD_ENV.captureDepth) glDeleteRenderbuffers(1, &R3D_MOD_ENV.captureDepth);
    if (R3D_MOD_ENV.captureCube) glDeleteTextures(1, &R3D_MOD_ENV.captureCube);

    if (R3D_MOD_ENV.workFramebuffer) glDeleteFramebuffers(1, &R3D_MOD_ENV.workFramebuffer);
    if (R3D_MOD_ENV.captureFramebuffer) glDeleteFramebuffers(1, &R3D_MOD_ENV.captureFramebuffer);

    layer_pool_quit(&R3D_MOD_ENV.irradiancePool);
    layer_pool_quit(&R3D_MOD_ENV.prefilterPool);

    r3d_pool_destroy(R3D_MOD_ENV.pool);
    RL_FREE(R3D_MOD_ENV.visible);
}

R3D_Probe r3d_env_probe_new(R3D_ProbeFlags flags)
{
    if (!BIT_TEST_ANY(flags, R3D_PROBE_ILLUMINATION | R3D_PROBE_REFLECTION)) {
        R3D_TRACELOG(LOG_FATAL, "Failed to create probe; Invalid flags");
        return R3D_POOL_ID_NULL;
    }

    R3D_Probe id = r3d_pool_insert(&R3D_MOD_ENV.pool);
    if (id == R3D_POOL_ID_NULL) {
        R3D_TRACELOG(LOG_ERROR, "Failed to insert probe into pool");
        return R3D_POOL_ID_NULL;
    }

    r3d_env_probe_t* probe = r3d_pool_get(R3D_MOD_ENV.pool, id);
    if (!probe_init(probe, flags)) {
        r3d_pool_remove(R3D_MOD_ENV.pool, id);
        R3D_TRACELOG(LOG_ERROR, "Failed to initialize probe");
        return R3D_POOL_ID_NULL;
    }

    R3D_TRACELOG(LOG_INFO, "[ID %d] Probe created successfully", id);

    return id;
}

void r3d_env_probe_delete(R3D_Probe id)
{
    r3d_env_probe_t* probe = r3d_pool_get(R3D_MOD_ENV.pool, id);
    if (!probe) return;

    probe_deinit(probe);
    r3d_pool_remove(R3D_MOD_ENV.pool, id);

    R3D_TRACELOG(LOG_INFO, "[ID %d] Probe destroyed", id);
}

bool r3d_env_probe_is_valid(R3D_Probe id)
{
    return r3d_pool_valid(R3D_MOD_ENV.pool, id);
}

r3d_env_probe_t* r3d_env_probe_get(R3D_Probe id)
{
    return r3d_pool_get(R3D_MOD_ENV.pool, id);
}

void r3d_env_probe_update_and_cull(const R3D_Frustum* viewFrustum, bool* hasVisibleProbes)
{
    R3D_MOD_ENV.visibleCount = 0;

    R3D_POOL_FOR_EACH(R3D_MOD_ENV.pool, r3d_env_probe_t, probe, idx) {
        if (probe->state.matrixShouldBeUpdated) {
            probe->state.matrixShouldBeUpdated = false;
            probe_update_matrix_frustum(probe);
        }

        if (!probe->enabled) continue;

        BoundingBox aabb = {
            .min = {
                probe->position.x - probe->range,
                probe->position.y - probe->range,
                probe->position.z - probe->range
            },
            .max = {
                probe->position.x + probe->range,
                probe->position.y + probe->range,
                probe->position.z + probe->range
            }
        };

        if (!R3D_FrustumIntersectsBoundingBox(viewFrustum, aabb)) continue;

        // Grow visible array if needed
        if (R3D_MOD_ENV.visibleCount >= R3D_MOD_ENV.visibleCapacity) {
            uint32_t newCap = R3D_MOD_ENV.visibleCapacity * 2;
            R3D_Probe* newPtr = RL_REALLOC(R3D_MOD_ENV.visible, newCap * sizeof(R3D_Probe));
            if (!newPtr) continue; // Skip rather than crash
            R3D_MOD_ENV.visible = newPtr;
            R3D_MOD_ENV.visibleCapacity = newCap;
        }

        R3D_MOD_ENV.visible[R3D_MOD_ENV.visibleCount++] = R3D_POOL_ID_MAKE(idx);
    }

    if (hasVisibleProbes) *hasVisibleProbes = (R3D_MOD_ENV.visibleCount > 0);
}

bool r3d_env_probe_should_be_updated(r3d_env_probe_t* probe, bool willBeUpdated)
{
    bool shouldUpdate = probe->state.sceneShouldBeUpdated;

    if (willBeUpdated && probe->state.updateMode == R3D_PROBE_UPDATE_ONCE) {
        probe->state.sceneShouldBeUpdated = false;
    }

    return shouldUpdate;
}

int r3d_env_irradiance_reserve_layer(void)
{
    int layer = layer_pool_reserve(&R3D_MOD_ENV.irradiancePool);

    if (layer < 0) {
        if (!cubemap_array_expand_capacity(&R3D_MOD_ENV.irradianceArray, &R3D_MOD_ENV.irradiancePool, R3D_CUBEMAP_IRRADIANCE_SIZE, false)) {
            return -1;
        }
        layer = layer_pool_reserve(&R3D_MOD_ENV.irradiancePool);
    }

    return layer;
}

void r3d_env_irradiance_release_layer(int layer)
{
    layer_pool_release(&R3D_MOD_ENV.irradiancePool, layer);
}

void r3d_env_irradiance_bind_fbo(int layer, int face)
{
    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.workFramebuffer);
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        R3D_MOD_ENV.irradianceArray, 0, layer * 6 + face
    );

    glViewport(0, 0, R3D_CUBEMAP_IRRADIANCE_SIZE, R3D_CUBEMAP_IRRADIANCE_SIZE);
}

GLuint r3d_env_irradiance_get(void)
{
    return R3D_MOD_ENV.irradianceArray;
}

int r3d_env_prefilter_reserve_layer(void)
{
    int layer = layer_pool_reserve(&R3D_MOD_ENV.prefilterPool);

    if (layer < 0) {
        if (!cubemap_array_expand_capacity(&R3D_MOD_ENV.prefilterArray, &R3D_MOD_ENV.prefilterPool, R3D_CUBEMAP_PREFILTER_SIZE, true)) {
            return -1;
        }
        layer = layer_pool_reserve(&R3D_MOD_ENV.prefilterPool);
    }

    return layer;
}

void r3d_env_prefilter_release_layer(int layer)
{
    layer_pool_release(&R3D_MOD_ENV.prefilterPool, layer);
}

void r3d_env_prefilter_bind_fbo(int layer, int face, int mipLevel)
{
    assert(mipLevel < r3d_get_mip_levels_1d(R3D_CUBEMAP_PREFILTER_SIZE));

    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.workFramebuffer);
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        R3D_MOD_ENV.prefilterArray, mipLevel, layer * 6 + face
    );

    int mipSize = R3D_CUBEMAP_PREFILTER_SIZE >> mipLevel;
    glViewport(0, 0, mipSize, mipSize);
}

GLuint r3d_env_prefilter_get(void)
{
    return R3D_MOD_ENV.prefilterArray;
}

void r3d_env_capture_bind_fbo(int face, int mipLevel)
{
    assert(mipLevel < r3d_get_mip_levels_1d(R3D_PROBE_CAPTURE_SIZE));

    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.captureFramebuffer);

    if (!R3D_MOD_ENV.captureCubeAllocated) {
        alloc_depth_stencil_renderbuffer(R3D_MOD_ENV.captureDepth, R3D_PROBE_CAPTURE_SIZE);
        cubemap_array_spec_t spec = cubemap_array_spec(R3D_PROBE_CAPTURE_SIZE, 0, true);
        cubemap_array_allocate(R3D_MOD_ENV.captureCube, spec);
        R3D_MOD_ENV.captureCubeAllocated = true;

        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, R3D_MOD_ENV.captureDepth
        );
    }

    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
        R3D_MOD_ENV.captureCube, mipLevel
    );

    int mipSize = R3D_PROBE_CAPTURE_SIZE >> mipLevel;
    glViewport(0, 0, mipSize, mipSize);
}

void r3d_env_capture_gen_mipmaps(void)
{
    assert(R3D_MOD_ENV.captureCubeAllocated);

    glBindTexture(GL_TEXTURE_CUBE_MAP, R3D_MOD_ENV.captureCube);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

GLuint r3d_env_capture_get(void)
{
    return R3D_MOD_ENV.captureCube;
}
