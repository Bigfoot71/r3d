/* r3d_storage.c -- Internal R3D storage module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_storage.h"
#include <string.h>
#include <assert.h>

// ========================================
// INTERNAL CONSTANTS
// ========================================

#define RING_BUFFER_COUNT 3

// ========================================
// MODULE STATE
// ========================================

typedef struct {
    GLuint textures[RING_BUFFER_COUNT];
    int current;
} storage_buffer_t;

static struct r3d_storage {
    storage_buffer_t buffers[R3D_STORAGE_COUNT];
    bool loaded[R3D_STORAGE_COUNT];
} R3D_MOD_STORAGE;

// ========================================
// LOAD FUNCTIONS
// ========================================

typedef void (*storage_loader_func)(void);

static void load_bone_matrices(void);

const storage_loader_func LOADERS[] = {
    [R3D_STORAGE_BONE_MATRICES] = load_bone_matrices,
};

void load_bone_matrices(void)
{
    const storage_buffer_t* buffer = &R3D_MOD_STORAGE.buffers[R3D_STORAGE_BONE_MATRICES];

    for (int i = 0; i < RING_BUFFER_COUNT; i++) {
        glBindTexture(GL_TEXTURE_1D, buffer->textures[i]);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 4 * R3D_STORAGE_MAX_BONE_MATRICES, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_1D, 0);
}

// ========================================
// UPLOAD FUNCTIONS
// ========================================

typedef void (*storage_uploader_func)(GLuint id, int slot, const void* data, int count);

void upload_bone_matrices(GLuint id, int slot, const void* data, int count);

const storage_uploader_func UPLOADERS[] = {
    [R3D_STORAGE_BONE_MATRICES] = upload_bone_matrices,
};

void upload_bone_matrices(GLuint id, int slot, const void* data, int count)
{
    if (count > R3D_STORAGE_MAX_BONE_MATRICES) {
        TraceLog(LOG_WARNING, "Cannot send more than %i bone matrices to GPU; Animations may be incorrect", R3D_STORAGE_MAX_BONE_MATRICES);
        count = R3D_STORAGE_MAX_BONE_MATRICES;
    }

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_1D, id);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4 * count, GL_RGBA, GL_FLOAT, data);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_storage_init(void)
{
    memset(&R3D_MOD_STORAGE, 0, sizeof(R3D_MOD_STORAGE));
    for (int i = 0; i < R3D_STORAGE_COUNT; i++) {
        glGenTextures(RING_BUFFER_COUNT, R3D_MOD_STORAGE.buffers[i].textures);
    }
    return true;
}

void r3d_storage_quit(void)
{
    for (int i = 0; i < R3D_STORAGE_COUNT; i++) {
        glDeleteTextures(RING_BUFFER_COUNT, R3D_MOD_STORAGE.buffers[i].textures);
    }
}

void r3d_storage_use(r3d_storage_t storage, int slot, const void* data, int count)
{
    if (!R3D_MOD_STORAGE.loaded[storage]) {
        R3D_MOD_STORAGE.loaded[storage] = true;
        LOADERS[storage]();
    }

    storage_buffer_t* buffer = &R3D_MOD_STORAGE.buffers[storage];
    GLuint id = buffer->textures[buffer->current];
    UPLOADERS[storage](id, slot, data, count);

    buffer->current = (buffer->current + 1) % RING_BUFFER_COUNT;
}
