/* r3d_opengl.c -- Internal R3D OpenGL cache module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_opengl.h"
#include <string.h>
#include <stdlib.h>
#include <uthash.h>
#include <glad.h>

// ========================================
// CONFIGURATION
// ========================================

#define R3D_OPENGL_EXT_NAME_MAX  64     // Maximum length of a domain name extension
#define R3D_OPENGL_EXT_CACHE_MAX 32     // Maximum number of extensions to cache

// ========================================
// EXTENSION CACHE
// ========================================

typedef struct {
    char name[R3D_OPENGL_EXT_NAME_MAX];     // key (extension name, inline)
    bool supported;                         // value (supported extension?)
    UT_hash_handle hh;                      // uthash handle
} extension_cache_t;

// ========================================
// MODULE STATE
// ========================================

static struct r3d_opengl {
    extension_cache_t extCacheBuffer[R3D_OPENGL_EXT_CACHE_MAX];     // extension cache buffer
    extension_cache_t* extCache;                                    // uthash pointer
    size_t extCacheUsed;                                            // number of entries used
} R3D_MOD_OPENGL;

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_opengl_init(void)
{
    memset(&R3D_MOD_OPENGL, 0, sizeof(R3D_MOD_OPENGL));
    return true;
}

void r3d_opengl_quit(void)
{
    extension_cache_t* current, * tmp;
    HASH_ITER(hh, R3D_MOD_OPENGL.extCache, current, tmp) {
        HASH_DEL(R3D_MOD_OPENGL.extCache, current);
    }
}

bool r3d_opengl_check_ext(const char* name)
{
    if (!name) return false;

    // Name length check
    size_t nameLen = strlen(name);
    if (nameLen >= R3D_OPENGL_EXT_NAME_MAX) {
        // Name too long, direct verification without cache
        GLint num_extensions = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

        for (GLint i = 0; i < num_extensions; i++) {
            const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
            if (ext && strcmp(ext, name) == 0) {
                return true;
            }
        }
        return false;
    }

    // Search the cache
    extension_cache_t* cached = NULL;
    HASH_FIND_STR(R3D_MOD_OPENGL.extCache, name, cached);
    
    if (cached) {
        return cached->supported;
    }

    // Extension not hidden, verified via OpenGL
    bool supported = false;
    GLint num_extensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);

    for (GLint i = 0; i < num_extensions; i++) {
        const char* ext = (const char*)glGetStringi(GL_EXTENSIONS, i);
        if (ext && strcmp(ext, name) == 0) {
            supported = true;
            break;
        }
    }

    // Add to cache if space available
    if (R3D_MOD_OPENGL.extCacheUsed < R3D_OPENGL_EXT_CACHE_MAX) {
        extension_cache_t* entry = &R3D_MOD_OPENGL.extCacheBuffer[R3D_MOD_OPENGL.extCacheUsed];
        R3D_MOD_OPENGL.extCacheUsed++;

        strncpy(entry->name, name, R3D_OPENGL_EXT_NAME_MAX - 1);
        entry->name[R3D_OPENGL_EXT_NAME_MAX - 1] = '\0';
        entry->supported = supported;

        HASH_ADD_STR(R3D_MOD_OPENGL.extCache, name, entry);
    }

    return supported;
}

bool r3d_opengl_has_anisotropy(float* max)
{
    static bool checked = false;
    static bool hasAniso = false;
    static float maxAniso = 1.0f;

    if (max) *max = 1.0f;

    if (!checked) {
        hasAniso = r3d_opengl_check_ext("GL_EXT_texture_filter_anisotropic");
        if (hasAniso) {
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        }
        checked = true;
    }

    if (max && hasAniso) {
        *max = maxAniso;
    }

    return hasAniso;
}

void r3d_opengl_clear_errors(void)
{
    while (glGetError() != GL_NO_ERROR);
}

bool r3d_opengl_check_error(const char* msg)
{
    int err = glGetError();
    if (err != GL_NO_ERROR) {
        TraceLog(LOG_ERROR, "R3D: OpenGL Error (%s): 0x%04x", msg, err);
        return true;
    }
    return false;
}
