/* r3d_env.c -- Internal R3D environment module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_env.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// ========================================
// MODULE STATE
// ========================================

static struct r3d_env {

    GLuint irradianceArray;
    GLuint prefilterArray;
    GLuint framebuffer;
    
    // Irradiance layer management
    int* freeIrradianceLayers;
    int irradianceFreeCount;
    int irradianceFreeCapacity;
    int irradianceTotalLayers;

    // Prefilter layer management
    int* freePrefilterLayers;
    int prefilterFreeCount;
    int prefilterFreeCapacity;
    int prefilterTotalLayers;

} R3D_MOD_ENV;

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool allocate_cubemap_array(GLuint texture, int size, int layers, bool mipmapped)
{
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);

    if (mipmapped) {
        for (int level = 0; level < R3D_ENV_PREFILTER_MIPS; level++) {
            int mipSize = size >> level;
            glTexImage3D(
                GL_TEXTURE_CUBE_MAP_ARRAY, level, GL_RGB16F,
                mipSize, mipSize, layers * 6, 0, GL_RGB, GL_FLOAT, NULL
            );
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    else {
        glTexImage3D(
            GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_RGB16F,
            size, size, layers * 6, 0, GL_RGB, GL_FLOAT, NULL
        );
        glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);

    return true;
}

static bool resize_cubemap_array(GLuint* texture, int size, int oldLayers, int newLayers, bool mipmapped)
{
    GLuint newTexture;
    glGenTextures(1, &newTexture);
    
    if (!allocate_cubemap_array(newTexture, size, newLayers, mipmapped)) {
        glDeleteTextures(1, &newTexture);
        return false;
    }
    
    if (oldLayers > 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.framebuffer);

        int levelCount = mipmapped ? R3D_ENV_PREFILTER_MIPS : 1;
        for (int layer = 0; layer < oldLayers; layer++) {
            for (int face = 0; face < 6; face++) {
                for (int level = 0; level < levelCount; level++) {
                    int mipSize = size >> level;

                    glFramebufferTextureLayer(
                        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                        *texture, level, layer * 6 + face
                    );

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
    }

    glDeleteTextures(1, texture);
    *texture = newTexture;

    return true;
}

static bool expand_irradiance_capacity(void)
{
    int newCapacity = R3D_MOD_ENV.irradianceTotalLayers + R3D_ENV_MAP_INTIIAL_CAP;

    if (!resize_cubemap_array(&R3D_MOD_ENV.irradianceArray, R3D_ENV_IRRADIANCE_SIZE, 
                              R3D_MOD_ENV.irradianceTotalLayers, newCapacity, false)) {
        return false;
    }

    int oldTotal = R3D_MOD_ENV.irradianceTotalLayers;
    R3D_MOD_ENV.irradianceTotalLayers = newCapacity;
    
    // Reallocate the free layers array if necessary
    if (R3D_MOD_ENV.irradianceFreeCount + R3D_ENV_MAP_INTIIAL_CAP > R3D_MOD_ENV.irradianceFreeCapacity) {
        R3D_MOD_ENV.irradianceFreeCapacity = R3D_MOD_ENV.irradianceTotalLayers;
        int* newFree = RL_REALLOC(R3D_MOD_ENV.freeIrradianceLayers, R3D_MOD_ENV.irradianceFreeCapacity * sizeof(int));
        if (!newFree) return false;
        R3D_MOD_ENV.freeIrradianceLayers = newFree;
    }
    
    // Add new layers to free list
    for (int i = oldTotal; i < newCapacity; i++) {
        R3D_MOD_ENV.freeIrradianceLayers[R3D_MOD_ENV.irradianceFreeCount++] = i;
    }
    
    return true;
}

static bool expand_prefilter_capacity(void)
{
    int newCapacity = R3D_MOD_ENV.prefilterTotalLayers + R3D_ENV_MAP_INTIIAL_CAP;

    if (!resize_cubemap_array(&R3D_MOD_ENV.prefilterArray, R3D_ENV_PREFILTER_SIZE,
                              R3D_MOD_ENV.prefilterTotalLayers, newCapacity, true)) {
        return false;
    }

    int oldTotal = R3D_MOD_ENV.prefilterTotalLayers;
    R3D_MOD_ENV.prefilterTotalLayers = newCapacity;
    
    // Reallocate the free layers array if necessary
    if (R3D_MOD_ENV.prefilterFreeCount + R3D_ENV_MAP_INTIIAL_CAP > R3D_MOD_ENV.prefilterFreeCapacity) {
        R3D_MOD_ENV.prefilterFreeCapacity = R3D_MOD_ENV.prefilterTotalLayers;
        int* newFree = RL_REALLOC(R3D_MOD_ENV.freePrefilterLayers, R3D_MOD_ENV.prefilterFreeCapacity * sizeof(int));
        if (!newFree) return false;
        R3D_MOD_ENV.freePrefilterLayers = newFree;
    }
    
    // Add new layers to free list
    for (int i = oldTotal; i < newCapacity; i++) {
        R3D_MOD_ENV.freePrefilterLayers[R3D_MOD_ENV.prefilterFreeCount++] = i;
    }
    
    return true;
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_env_init(void)
{
    memset(&R3D_MOD_ENV, 0, sizeof(R3D_MOD_ENV));

    glGenTextures(1, &R3D_MOD_ENV.irradianceArray);
    glGenTextures(1, &R3D_MOD_ENV.prefilterArray);
    glGenFramebuffers(1, &R3D_MOD_ENV.framebuffer);

    // Allocate irradiance free layers array
    R3D_MOD_ENV.irradianceFreeCapacity = R3D_ENV_MAP_INTIIAL_CAP * 2;
    R3D_MOD_ENV.freeIrradianceLayers = RL_MALLOC(R3D_MOD_ENV.irradianceFreeCapacity * sizeof(int));
    if (!R3D_MOD_ENV.freeIrradianceLayers) {
        r3d_env_quit();
        return false;
    }

    // Allocate prefilter free layers array
    R3D_MOD_ENV.prefilterFreeCapacity = R3D_ENV_MAP_INTIIAL_CAP * 2;
    R3D_MOD_ENV.freePrefilterLayers = RL_MALLOC(R3D_MOD_ENV.prefilterFreeCapacity * sizeof(int));
    if (!R3D_MOD_ENV.freePrefilterLayers) {
        r3d_env_quit();
        return false;
    }

    return true;
}

void r3d_env_quit(void)
{
    if (R3D_MOD_ENV.irradianceArray) {
        glDeleteTextures(1, &R3D_MOD_ENV.irradianceArray);
    }
    if (R3D_MOD_ENV.prefilterArray) {
        glDeleteTextures(1, &R3D_MOD_ENV.prefilterArray);
    }
    if (R3D_MOD_ENV.framebuffer) {
        glDeleteFramebuffers(1, &R3D_MOD_ENV.framebuffer);
    }

    RL_FREE(R3D_MOD_ENV.freeIrradianceLayers);
    RL_FREE(R3D_MOD_ENV.freePrefilterLayers);
}

int r3d_env_reserve_irradiance_layer(void)
{
    if (R3D_MOD_ENV.irradianceFreeCount > 0) {
        return R3D_MOD_ENV.freeIrradianceLayers[--R3D_MOD_ENV.irradianceFreeCount];
    }

    if (!expand_irradiance_capacity()) return -1;

    return R3D_MOD_ENV.freeIrradianceLayers[--R3D_MOD_ENV.irradianceFreeCount];
}

void r3d_env_release_irradiance_layer(int layer)
{
    if (layer < 0 || layer >= R3D_MOD_ENV.irradianceTotalLayers) {
        return;
    }

    if (R3D_MOD_ENV.irradianceFreeCount < R3D_MOD_ENV.irradianceFreeCapacity) {
        R3D_MOD_ENV.freeIrradianceLayers[R3D_MOD_ENV.irradianceFreeCount++] = layer;
    }
}

int r3d_env_reserve_prefilter_layer(void)
{
    if (R3D_MOD_ENV.prefilterFreeCount > 0) {
        return R3D_MOD_ENV.freePrefilterLayers[--R3D_MOD_ENV.prefilterFreeCount];
    }

    if (!expand_prefilter_capacity()) return -1;

    return R3D_MOD_ENV.freePrefilterLayers[--R3D_MOD_ENV.prefilterFreeCount];
}

void r3d_env_release_prefilter_layer(int layer)
{
    if (layer < 0 || layer >= R3D_MOD_ENV.prefilterTotalLayers) {
        return;
    }

    if (R3D_MOD_ENV.prefilterFreeCount < R3D_MOD_ENV.prefilterFreeCapacity) {
        R3D_MOD_ENV.freePrefilterLayers[R3D_MOD_ENV.prefilterFreeCount++] = layer;
    }
}

void r3d_env_bind_irradiance_fbo(int layer, int face)
{
    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.framebuffer);
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        R3D_MOD_ENV.irradianceArray, 0, layer * 6 + face
    );
    glViewport(0, 0, R3D_ENV_IRRADIANCE_SIZE, R3D_ENV_IRRADIANCE_SIZE);
}

void r3d_env_bind_prefilter_fbo(int layer, int face, int mipLevel)
{
    assert(mipLevel < R3D_ENV_PREFILTER_MIPS);

    int mipSize = R3D_ENV_PREFILTER_SIZE >> mipLevel;

    glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_ENV.framebuffer);
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        R3D_MOD_ENV.prefilterArray, mipLevel, layer * 6 + face
    );
    glViewport(0, 0, mipSize, mipSize);
}

GLuint r3d_env_get_irradiance_map(void)
{
    return R3D_MOD_ENV.irradianceArray;
}

GLuint r3d_env_get_prefilter_map(void)
{
    return R3D_MOD_ENV.prefilterArray;
}
