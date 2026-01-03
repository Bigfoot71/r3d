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
    
    int* freeLayers;        // List of free layers
    int freeCount;          // Number of free layers
    int freeCapacity;       // freeLayers array capacity
    int totalLayers;        // Total number of layers allocated

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

static bool expand_capacity(void)
{
    int newCapacity = R3D_MOD_ENV.totalLayers + R3D_ENV_MAP_INTIIAL_CAP;

    if (!resize_cubemap_array(&R3D_MOD_ENV.irradianceArray, R3D_ENV_IRRADIANCE_SIZE, 
                              R3D_MOD_ENV.totalLayers, newCapacity, false)) {
        return false;
    }

    if (!resize_cubemap_array(&R3D_MOD_ENV.prefilterArray, R3D_ENV_PREFILTER_SIZE,
                              R3D_MOD_ENV.totalLayers, newCapacity, true)) {
        return false;
    }

    int oldTotal = R3D_MOD_ENV.totalLayers;
    R3D_MOD_ENV.totalLayers = newCapacity;
    
    // Reallocate the freeLayers array if necessary
    if (R3D_MOD_ENV.freeCount + R3D_ENV_MAP_INTIIAL_CAP > R3D_MOD_ENV.freeCapacity) {
        R3D_MOD_ENV.freeCapacity = R3D_MOD_ENV.totalLayers;
        int* newFree = RL_REALLOC(R3D_MOD_ENV.freeLayers, R3D_MOD_ENV.freeCapacity * sizeof(int));
        if (!newFree) return false;
        R3D_MOD_ENV.freeLayers = newFree;
    }
    
    // Adding new layers
    for (int i = oldTotal; i < newCapacity; i++) {
        R3D_MOD_ENV.freeLayers[R3D_MOD_ENV.freeCount++] = i;
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

    R3D_MOD_ENV.freeCapacity = R3D_ENV_MAP_INTIIAL_CAP * 2;
    R3D_MOD_ENV.freeLayers = RL_MALLOC(R3D_MOD_ENV.freeCapacity * sizeof(int));
    if (!R3D_MOD_ENV.freeLayers) {
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

    RL_FREE(R3D_MOD_ENV.freeLayers);
}

int r3d_env_reserve_map_layer(void)
{
    if (R3D_MOD_ENV.freeCount > 0) {
        return R3D_MOD_ENV.freeLayers[--R3D_MOD_ENV.freeCount];
    }

    if (!expand_capacity()) return -1;

    return R3D_MOD_ENV.freeLayers[--R3D_MOD_ENV.freeCount];
}

void r3d_env_release_map_layer(int layer)
{
    if (layer < 0 || layer >= R3D_MOD_ENV.totalLayers) {
        return;
    }

    if (R3D_MOD_ENV.freeCount < R3D_MOD_ENV.freeCapacity) {
        R3D_MOD_ENV.freeLayers[R3D_MOD_ENV.freeCount++] = layer;
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
