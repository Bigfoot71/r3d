/* r3d_env.c -- Internal R3D environment module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_env.h"
#include <raymath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/r3d_frustum.h"
#include "../common/r3d_helper.h"
#include "../common/r3d_math.h"
#include <r3d_config.h>

// ========================================
// CONSTANTS
// ========================================

#define PROBE_INITIAL_CAPACITY  16
#define LAYER_GROWTH            4

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

static bool allocate_texture_depth(GLuint texture, int size)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, size, size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
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
} cubemap_spec_t;

static inline cubemap_spec_t cubemap_spec(int size, int layers, bool mipmapped)
{
    return (cubemap_spec_t) {
        .size = size,
        .layers = layers,
        .mipLevels = mipmapped ? r3d_get_mip_levels_1d(size) : 1,
        .target = (layers > 0) ? GL_TEXTURE_CUBE_MAP_ARRAY : GL_TEXTURE_CUBE_MAP
    };
}

static bool allocate_cubemap(GLuint texture, cubemap_spec_t spec)
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

static bool resize_cubemap_array(GLuint* texture, cubemap_spec_t oldSpec, cubemap_spec_t newSpec)
{
    GLuint newTexture;
    glGenTextures(1, &newTexture);

    if (!allocate_cubemap(newTexture, newSpec)) {
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

static bool expand_cubemap_capacity(GLuint* texture, r3d_env_layer_pool_t* pool, int size, bool mipmapped)
{
    cubemap_spec_t oldSpec = cubemap_spec(size, pool->totalLayers, mipmapped);
    cubemap_spec_t newSpec = cubemap_spec(size, pool->totalLayers + LAYER_GROWTH, mipmapped);

    if (!resize_cubemap_array(texture, oldSpec, newSpec)) {
        return false;
    }

    return layer_pool_expand(pool, LAYER_GROWTH);
}

// ========================================
// PROBE FUNCTIONS
// ========================================

static bool growth_probe_arrays(void)
{
    int newCapacity = 2 * R3D_MOD_ENV.capacityProbes;

    r3d_env_probe_t* newProbes = RL_REALLOC(R3D_MOD_ENV.probes, newCapacity * sizeof(*R3D_MOD_ENV.probes));
    if (!newProbes) return false;

    R3D_MOD_ENV.capacityProbes = newCapacity;
    R3D_MOD_ENV.probes = newProbes;

    for (int i = 0; i < R3D_ENV_PROBE_ARRAY_COUNT; i++) {
        R3D_Probe* newPtr = RL_REALLOC(R3D_MOD_ENV.arrays[i].probes, newCapacity * sizeof(R3D_Probe));
        if (!newPtr) return false;
        R3D_MOD_ENV.arrays[i].probes = newPtr;
    }

    return true;
}

static bool init_probe(r3d_env_probe_t* probe, R3D_ProbeFlag flags)
{
    probe->flags = flags;
    probe->irradiance = -1;
    probe->prefilter = -1;

    if (flags & R3D_PROBE_ILLUMINATION) {
        probe->irradiance = r3d_env_irradiance_reserve_layer();
        if (probe->irradiance == -1) return false;
    }

    if (flags & R3D_PROBE_REFLECTION) {
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

static void deinit_probe(r3d_env_probe_t* probe)
{
    if (probe->irradiance >= 0) {
        r3d_env_irradiance_release_layer(probe->irradiance);
    }
    if (probe->prefilter >= 0) {
        r3d_env_prefilter_release_layer(probe->prefilter);
    }
}

static void update_probe_matrix_frustum(r3d_env_probe_t* probe)
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

    for (int face = 0; face < 6; face++) {
        Vector3 target = Vector3Add(probe->position, dirs[face]);
        Matrix view = MatrixLookAt(probe->position, target, ups[face]);
        Matrix viewProj = r3d_matrix_multiply(&view, &proj);
        probe->frustum[face] = r3d_frustum_create(viewProj);
        probe->view[face] = view;
        probe->proj[face] = proj;
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
    glGenTextures(1, &R3D_MOD_ENV.captureDepth);
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
    R3D_MOD_ENV.probes = RL_MALLOC(PROBE_INITIAL_CAPACITY * sizeof(*R3D_MOD_ENV.probes));
    R3D_MOD_ENV.capacityProbes = PROBE_INITIAL_CAPACITY;

    if (!R3D_MOD_ENV.probes) {
        R3D_TRACELOG(LOG_FATAL, "Failed to allocate probe array");
        r3d_env_quit();
        return false;
    }

    for (int i = 0; i < R3D_ENV_PROBE_ARRAY_COUNT; i++) {
        R3D_MOD_ENV.arrays[i].probes = RL_MALLOC(PROBE_INITIAL_CAPACITY * sizeof(R3D_Probe));
        if (!R3D_MOD_ENV.arrays[i].probes) {
            R3D_TRACELOG(LOG_FATAL, "Failed to allocate probe list array %i", i);
            r3d_env_quit();
            return false;
        }
    }

    return true;
}

void r3d_env_quit(void)
{
    if (R3D_MOD_ENV.irradianceArray) glDeleteTextures(1, &R3D_MOD_ENV.irradianceArray);
    if (R3D_MOD_ENV.prefilterArray) glDeleteTextures(1, &R3D_MOD_ENV.prefilterArray);
    if (R3D_MOD_ENV.captureDepth) glDeleteTextures(1, &R3D_MOD_ENV.captureDepth);
    if (R3D_MOD_ENV.captureCube) glDeleteTextures(1, &R3D_MOD_ENV.captureCube);
    if (R3D_MOD_ENV.workFramebuffer) glDeleteFramebuffers(1, &R3D_MOD_ENV.workFramebuffer);
    if (R3D_MOD_ENV.captureFramebuffer) glDeleteFramebuffers(1, &R3D_MOD_ENV.captureFramebuffer);

    layer_pool_quit(&R3D_MOD_ENV.irradiancePool);
    layer_pool_quit(&R3D_MOD_ENV.prefilterPool);

    for (int i = 0; i < R3D_ENV_PROBE_ARRAY_COUNT; i++) {
        RL_FREE(R3D_MOD_ENV.arrays[i].probes);
    }

    RL_FREE(R3D_MOD_ENV.probes);
}

R3D_Probe r3d_env_probe_new(R3D_ProbeFlag flags)
{
    r3d_env_probe_array_t* validProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_VALID];
    r3d_env_probe_array_t* freeProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_FREE];

    R3D_Probe index;
    if (freeProbes->count == 0) index = validProbes->count;
    else index = freeProbes->probes[--freeProbes->count];

    if (index >= R3D_MOD_ENV.capacityProbes) {
        if (!growth_probe_arrays()) {
            R3D_TRACELOG(LOG_FATAL, "Failed to grow probe arrays");
            return -1;
        }
    }

    if (!init_probe(&R3D_MOD_ENV.probes[index], flags)) {
        R3D_TRACELOG(LOG_FATAL, "Failed to initialize probe");
        return -1;
    }

    validProbes->probes[validProbes->count++] = index;

    return index;
}

void r3d_env_probe_delete(R3D_Probe index)
{
    if (index < 0) return;

    r3d_env_probe_array_t* validProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_VALID];

    // Find and remove from valid list
    for (int i = 0; i < validProbes->count; i++) {
        if (index == validProbes->probes[i]) {
            int numToMove = validProbes->count - i - 1;
            if (numToMove > 0) {
                memmove(&validProbes->probes[i], &validProbes->probes[i + 1],
                       numToMove * sizeof(validProbes->probes[0]));
            }
            validProbes->count--;

            // Add to free list and cleanup
            r3d_env_probe_array_t* freeProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_FREE];
            freeProbes->probes[freeProbes->count++] = index;
            deinit_probe(&R3D_MOD_ENV.probes[index]);
            return;
        }
    }
}

bool r3d_env_probe_is_valid(R3D_Probe index)
{
    if (index < 0) return false;

    const r3d_env_probe_array_t* validProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_VALID];
    for (int i = 0; i < validProbes->count; i++) {
        if (index == validProbes->probes[i]) return true;
    }
    return false;
}

r3d_env_probe_t* r3d_env_probe_get(R3D_Probe index)
{
    return r3d_env_probe_is_valid(index) ? &R3D_MOD_ENV.probes[index] : NULL;
}

bool r3d_env_probe_has(r3d_env_probe_array_enum_t array)
{
    return (R3D_MOD_ENV.arrays[array].count > 0);
}

bool r3d_env_probe_iter(r3d_env_probe_t** probe, r3d_env_probe_array_enum_t array)
{
    static int index = 0;
    index = (*probe == NULL) ? 0 : index + 1;

    if (index >= R3D_MOD_ENV.arrays[array].count) return false;

    *probe = &R3D_MOD_ENV.probes[R3D_MOD_ENV.arrays[array].probes[index]];
    return true;
}

void r3d_env_probe_update_and_cull(const r3d_frustum_t* viewFrustum)
{
    r3d_env_probe_array_t* visibleProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_VISIBLE];
    r3d_env_probe_array_t* validProbes = &R3D_MOD_ENV.arrays[R3D_ENV_PROBE_ARRAY_VALID];

    visibleProbes->count = 0;

    for (int i = 0; i < validProbes->count; i++) {
        R3D_Probe index = validProbes->probes[i];
        r3d_env_probe_t* probe = &R3D_MOD_ENV.probes[index];

        if (probe->state.matrixShouldBeUpdated) {
            probe->state.matrixShouldBeUpdated = false;
            update_probe_matrix_frustum(probe);
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

        if (r3d_frustum_is_aabb_in(viewFrustum, &aabb)) {
            visibleProbes->probes[visibleProbes->count++] = index;
        }
    }
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
        if (!expand_cubemap_capacity(&R3D_MOD_ENV.irradianceArray, &R3D_MOD_ENV.irradiancePool, R3D_CUBEMAP_IRRADIANCE_SIZE, false)) {
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
        if (!expand_cubemap_capacity(&R3D_MOD_ENV.prefilterArray, &R3D_MOD_ENV.prefilterPool, R3D_CUBEMAP_PREFILTER_SIZE, true)) {
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
        cubemap_spec_t spec = cubemap_spec(R3D_PROBE_CAPTURE_SIZE, 0, true);
        allocate_cubemap(R3D_MOD_ENV.captureCube, spec);
        allocate_texture_depth(R3D_MOD_ENV.captureDepth, R3D_PROBE_CAPTURE_SIZE);
        R3D_MOD_ENV.captureCubeAllocated = true;

        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D, R3D_MOD_ENV.captureDepth, 0
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
