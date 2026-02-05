/* r3d_driver.c -- Internal R3D OpenGL cache module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_driver.h"
#include <r3d_config.h>
#include <string.h>
#include <stdlib.h>
#include <uthash.h>
#include <assert.h>
#include <glad.h>

// ========================================
// CONFIGURATION
// ========================================

#define R3D_OPENGL_EXT_NAME_MAX  64     // Maximum length of a domain name extension
#define R3D_OPENGL_EXT_CACHE_MAX 32     // Maximum number of extensions to cache

// ========================================
// INTERNAL ENUMS
// ========================================

typedef enum {
    CAP_STATE_UNKNOWN = 0,
    CAP_STATE_ENABLED = 1,
    CAP_STATE_DISABLED = 2,
} cap_state_t;

typedef enum {
    CAP_INDEX_BLEND = 0,
    CAP_INDEX_CULL_FACE,
    CAP_INDEX_DEPTH_TEST,
    CAP_INDEX_SCISSOR_TEST,
    CAP_INDEX_STENCIL_TEST,
    CAP_INDEX_COUNT
} cap_index_t;

// ========================================
// INTERNAL STRUCTS
// ========================================

typedef struct {
    char name[R3D_OPENGL_EXT_NAME_MAX];     // key (extension name, inline)
    bool supported;                         // value (supported extension?)
    UT_hash_handle hh;                      // uthash handle
} extension_cache_t;

// ========================================
// MODULE STATE
// ========================================

static struct r3d_driver {
    extension_cache_t extCacheBuffer[R3D_OPENGL_EXT_CACHE_MAX];     // extension cache buffer
    extension_cache_t* extCache;                                    // uthash pointer
    size_t extCacheUsed;                                            // number of entries used
    cap_state_t capStates[CAP_INDEX_COUNT];
} R3D_MOD_OPENGL;

// ========================================
// INTERNAL FUNCTIONS
// ========================================

/* Returns -1 if capability is not tracked */
static int get_capability_index(GLenum cap)
{
    switch (cap) {
        case GL_BLEND:        return CAP_INDEX_BLEND;
        case GL_CULL_FACE:    return CAP_INDEX_CULL_FACE;
        case GL_DEPTH_TEST:   return CAP_INDEX_DEPTH_TEST;
        case GL_SCISSOR_TEST: return CAP_INDEX_SCISSOR_TEST;
        case GL_STENCIL_TEST: return CAP_INDEX_STENCIL_TEST;
        default:              break;
    }
    return -1;
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_driver_init(void)
{
    memset(&R3D_MOD_OPENGL, 0, sizeof(R3D_MOD_OPENGL));
    return true;
}

void r3d_driver_quit(void)
{
    extension_cache_t* current, * tmp;
    HASH_ITER(hh, R3D_MOD_OPENGL.extCache, current, tmp) {
        HASH_DEL(R3D_MOD_OPENGL.extCache, current);
    }
}

bool r3d_driver_check_ext(const char* name)
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

bool r3d_driver_has_anisotropy(float* max)
{
    static bool checked = false;
    static bool hasAniso = false;
    static float maxAniso = 1.0f;

    if (max) *max = 1.0f;

    if (!checked) {
        hasAniso = r3d_driver_check_ext("GL_EXT_texture_filter_anisotropic");
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

void r3d_driver_clear_errors(void)
{
    while (glGetError() != GL_NO_ERROR);
}

bool r3d_driver_check_error(const char* msg)
{
    int err = glGetError();
    if (err != GL_NO_ERROR) {
        R3D_TRACELOG(LOG_ERROR, "OpenGL Error (%s): 0x%04x", msg, err);
        return true;
    }
    return false;
}

void r3d_driver_enable(GLenum cap)
{
    int index = get_capability_index(cap);
    if (index < 0) { glEnable(cap); return; }

    if (R3D_MOD_OPENGL.capStates[index] != CAP_STATE_ENABLED) {
        R3D_MOD_OPENGL.capStates[index] = CAP_STATE_ENABLED;
        glEnable(cap);
    }
}

void r3d_driver_disable(GLenum cap)
{
    int index = get_capability_index(cap);
    if (index < 0) { glDisable(cap); return; }

    if (R3D_MOD_OPENGL.capStates[index] != CAP_STATE_DISABLED) {
        R3D_MOD_OPENGL.capStates[index] = CAP_STATE_DISABLED;
        glDisable(cap);
    }
}

void r3d_driver_set_stencil(R3D_StencilState state)
{
    GLenum glFunc;
    switch (state.mode) {
        case R3D_COMPARE_LESS:     glFunc = GL_LESS; break;
        case R3D_COMPARE_LEQUAL:   glFunc = GL_LEQUAL; break;
        case R3D_COMPARE_EQUAL:    glFunc = GL_EQUAL; break;
        case R3D_COMPARE_GREATER:  glFunc = GL_GREATER; break;
        case R3D_COMPARE_GEQUAL:   glFunc = GL_GEQUAL; break;
        case R3D_COMPARE_NOTEQUAL: glFunc = GL_NOTEQUAL; break;
        case R3D_COMPARE_ALWAYS:   glFunc = GL_ALWAYS; break;
        case R3D_COMPARE_NEVER:    glFunc = GL_NEVER; break;
        default:                   glFunc = GL_ALWAYS; break;
    }

    GLenum glOpFail, glOpZFail, glOpPass;

    switch (state.opFail) {
        case R3D_STENCIL_KEEP:    glOpFail = GL_KEEP; break;
        case R3D_STENCIL_ZERO:    glOpFail = GL_ZERO; break;
        case R3D_STENCIL_REPLACE: glOpFail = GL_REPLACE; break;
        case R3D_STENCIL_INCR:    glOpFail = GL_INCR; break;
        case R3D_STENCIL_DECR:    glOpFail = GL_DECR; break;
        default:                  glOpFail = GL_KEEP; break;
    }

    switch (state.opZFail) {
        case R3D_STENCIL_KEEP:    glOpZFail = GL_KEEP; break;
        case R3D_STENCIL_ZERO:    glOpZFail = GL_ZERO; break;
        case R3D_STENCIL_REPLACE: glOpZFail = GL_REPLACE; break;
        case R3D_STENCIL_INCR:    glOpZFail = GL_INCR; break;
        case R3D_STENCIL_DECR:    glOpZFail = GL_DECR; break;
        default:                  glOpZFail = GL_KEEP; break;
    }

    switch (state.opPass) {
        case R3D_STENCIL_KEEP:    glOpPass = GL_KEEP; break;
        case R3D_STENCIL_ZERO:    glOpPass = GL_ZERO; break;
        case R3D_STENCIL_REPLACE: glOpPass = GL_REPLACE; break;
        case R3D_STENCIL_INCR:    glOpPass = GL_INCR; break;
        case R3D_STENCIL_DECR:    glOpPass = GL_DECR; break;
        default:                  glOpPass = GL_KEEP; break;
    }

    glStencilFunc(glFunc, state.ref, state.mask);
    glStencilOp(glOpFail, glOpZFail, glOpPass);
}

void r3d_driver_set_blend(R3D_BlendMode blend, R3D_TransparencyMode transparency)
{
    switch (blend) {
    case R3D_BLEND_MIX:
        if (transparency == R3D_TRANSPARENCY_DISABLED) {
            glBlendFunc(GL_ONE, GL_ZERO);
        }
        else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        break;
    case R3D_BLEND_ADDITIVE:
        if (transparency == R3D_TRANSPARENCY_DISABLED) {
            glBlendFunc(GL_ONE, GL_ONE);
        }
        else {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        break;
    case R3D_BLEND_MULTIPLY:
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    case R3D_BLEND_PREMULTIPLIED_ALPHA:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
    default:
        break;
    }
}

void r3d_driver_set_depth(R3D_CompareMode mode)
{
    switch (mode) {
    case R3D_COMPARE_LESS: glDepthFunc(GL_LESS); break;
    case R3D_COMPARE_LEQUAL: glDepthFunc(GL_LEQUAL); break;
    case R3D_COMPARE_EQUAL: glDepthFunc(GL_EQUAL); break;
    case R3D_COMPARE_GREATER: glDepthFunc(GL_GREATER); break;
    case R3D_COMPARE_GEQUAL: glDepthFunc(GL_GEQUAL); break;
    case R3D_COMPARE_NOTEQUAL: glDepthFunc(GL_NOTEQUAL); break;
    case R3D_COMPARE_ALWAYS: glDepthFunc(GL_ALWAYS); break;
    case R3D_COMPARE_NEVER: glDepthFunc(GL_NEVER); break;
    default: break;
    }
}

void r3d_driver_set_cull(R3D_CullMode mode)
{
    switch (mode) {
    case R3D_CULL_NONE:
        r3d_driver_disable(GL_CULL_FACE);
        break;
    case R3D_CULL_BACK:
        r3d_driver_enable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        break;
    case R3D_CULL_FRONT:
        r3d_driver_enable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        break;
    default:
        break;
    }
}

void r3d_driver_set_cull_shadow(R3D_ShadowCastMode castMode, R3D_CullMode cullMode)
{
    switch (castMode) {
    case R3D_SHADOW_CAST_ON_AUTO:
    case R3D_SHADOW_CAST_ONLY_AUTO:
        r3d_driver_set_cull(cullMode);
        break;

    case R3D_SHADOW_CAST_ON_DOUBLE_SIDED:
    case R3D_SHADOW_CAST_ONLY_DOUBLE_SIDED:
        r3d_driver_set_cull(R3D_CULL_NONE);
        break;

    case R3D_SHADOW_CAST_ON_FRONT_SIDE:
    case R3D_SHADOW_CAST_ONLY_FRONT_SIDE:
        r3d_driver_set_cull(R3D_CULL_BACK);
        break;

    case R3D_SHADOW_CAST_ON_BACK_SIDE:
    case R3D_SHADOW_CAST_ONLY_BACK_SIDE:
        r3d_driver_set_cull(R3D_CULL_FRONT);
        break;

    case R3D_SHADOW_CAST_DISABLED:
    default:
        assert(false && "This shouldn't happen");
        break;
    }
}

void r3d_driver_invalidate(void)
{
    for (int i = 0; i < CAP_INDEX_COUNT; i++) {
        R3D_MOD_OPENGL.capStates[i] = CAP_STATE_UNKNOWN;
    }
}
