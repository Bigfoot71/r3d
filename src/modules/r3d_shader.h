/* r3d_shader.h -- Internal R3D shader module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_SHADER_H
#define R3D_MODULE_SHADER_H

#include <r3d/r3d_core.h>
#include <r3d_config.h>
#include <stdalign.h>
#include <raylib.h>
#include <stdint.h>
#include <assert.h>
#include <glad.h>

#include "../common/r3d_rshade.h"
#include "../common/r3d_math.h"

// ========================================
// MODULE CONSTANTS
// ========================================

#define R3D_SHADER_BLOCK_VIEW_SLOT          0
#define R3D_SHADER_BLOCK_ENV_SLOT           1
#define R3D_SHADER_BLOCK_LIGHT_SLOT         2
#define R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT   3
#define R3D_SHADER_BLOCK_USER_SLOT          4

// ========================================
// SHADER MANAGEMENT MACROS
// ========================================

#define R3D_SHADER_GET(shader_name, custom)                                     \
    (((r3d_shader_custom_t*)(custom) != NULL)                                   \
        ? &((r3d_shader_custom_t*)(custom))->shader_name                        \
        : &R3D_MOD_SHADER.shader_name)

#define R3D_SHADER_USE(shader_name) do {                                        \
    if (R3D_MOD_SHADER.shader_name.id == 0) {                                   \
        bool ok = R3D_MOD_SHADER_LOADER.shader_name(NULL);                      \
        assert(ok);                                                             \
    }                                                                           \
    if (R3D_MOD_SHADER.currentProgram != R3D_MOD_SHADER.shader_name.id) {       \
        R3D_MOD_SHADER.currentProgram = R3D_MOD_SHADER.shader_name.id;          \
        glUseProgram(R3D_MOD_SHADER.shader_name.id);                            \
    }                                                                           \
} while(0)

#define R3D_SHADER_USE_CUSTOM(custom, shader_name) do {                         \
    r3d_shader_custom_t* c_shader = (r3d_shader_custom_t*)(custom);             \
    assert(c_shader != NULL);                                                   \
    if (c_shader->shader_name.id == 0) {                                        \
        bool ok = R3D_MOD_SHADER_LOADER.shader_name(c_shader);                  \
        assert(ok);                                                             \
    }                                                                           \
    if (R3D_MOD_SHADER.currentProgram != c_shader->shader_name.id) {            \
        R3D_MOD_SHADER.currentProgram = c_shader->shader_name.id;               \
        glUseProgram(c_shader->shader_name.id);                                 \
        r3d_shader_bind_custom_samplers(c_shader);                              \
        r3d_shader_bind_custom_uniforms(c_shader);                              \
    }                                                                           \
} while(0)

#define R3D_SHADER_USE_SELECT(shader_name, custom) do {                         \
    r3d_shader_custom_t* c_shader = (r3d_shader_custom_t*)(custom);             \
    if (c_shader == NULL) R3D_SHADER_USE(shader_name);                          \
    else R3D_SHADER_USE_CUSTOM(custom, shader_name);                            \
} while(0)

#define R3D_SHADER_BIND_SAMPLER(shader_name, uniform, texId) do {               \
    r3d_shader_bind_sampler(R3D_MOD_SHADER.shader_name.uniform.slot, (texId));  \
} while(0)

#define R3D_SHADER_BIND_SAMPLER_CUSTOM(custom, shader_name, uniform, texId) do {\
    r3d_shader_custom_t* c_shader = (r3d_shader_custom_t*)(custom);             \
    r3d_shader_bind_sampler(c_shader->shader_name.uniform.slot, (texId));       \
} while(0)

#define R3D_SHADER_BIND_SAMPLER_SELECT(shader_name, custom, uniform, texId) do {\
    r3d_shader_custom_t* c_shader = (r3d_shader_custom_t*)(custom);             \
    r3d_shader_bind_sampler((c_shader != NULL)                                  \
        ? c_shader->shader_name.uniform.slot                                    \
        : R3D_MOD_SHADER.shader_name.uniform.slot,                              \
        (texId));                                                               \
} while(0)

#define R3D_SHADER_SET_INT(shader_name, uniform, value) do {                    \
    if (R3D_MOD_SHADER.shader_name.uniform.val != (value)) {                    \
        R3D_MOD_SHADER.shader_name.uniform.val = (value);                       \
        glUniform1i(                                                            \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            (value)                                                             \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_INT_CUSTOM(custom, shader_name, uniform, value) do {     \
    if (((r3d_shader_custom_t*)(custom))->shader_name.uniform.val != (value)) { \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = (value);    \
        glUniform1i(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, (value)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_INT_SELECT(shader_name, custom, uniform, value) do {     \
    if (R3D_SHADER_GET(shader_name, custom)->uniform.val != (value)) {          \
        R3D_SHADER_GET(shader_name, custom)->uniform.val = (value);             \
        glUniform1i(R3D_SHADER_GET(shader_name, custom)->uniform.loc, (value)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_FLOAT(shader_name, uniform, value) do {                  \
    if (R3D_MOD_SHADER.shader_name.uniform.val != (value)) {                    \
        R3D_MOD_SHADER.shader_name.uniform.val = (value);                       \
        glUniform1f(                                                            \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            (value)                                                             \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_FLOAT_CUSTOM(custom, shader_name, uniform, value) do {   \
    if (((r3d_shader_custom_t*)(custom))->shader_name.uniform.val != (value)) { \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = (value);    \
        glUniform1f(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, (value)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_FLOAT_SELECT(shader_name, custom, uniform, value) do {   \
    if (R3D_SHADER_GET(shader_name, custom)->uniform.val != (value)) {          \
        R3D_SHADER_GET(shader_name, custom)->uniform.val = (value);             \
        glUniform1f(R3D_SHADER_GET(shader_name, custom)->uniform.loc, (value)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC2(shader_name, uniform, ...) do {                     \
    const Vector2 tmp = (__VA_ARGS__);                                          \
    if (!Vector2Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {          \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                           \
        glUniform2fv(                                                           \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            1,                                                                  \
            (float*)(&tmp)                                                      \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC2_CUSTOM(custom, shader_name, uniform, ...) do {      \
    const Vector2 tmp = (__VA_ARGS__);                                          \
    if (!Vector2Equals(((r3d_shader_custom_t*)(custom))->shader_name.uniform.val, tmp)) { \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = tmp;        \
        glUniform2fv(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC2_SELECT(shader_name, custom, uniform, ...) do {      \
    const Vector2 tmp = (__VA_ARGS__);                                          \
    if (!Vector2Equals(R3D_SHADER_GET(shader_name, custom)->uniform.val, tmp)) {\
        R3D_SHADER_GET(shader_name, custom)->uniform.val = tmp;                 \
        glUniform2fv(R3D_SHADER_GET(shader_name, custom)->uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC3(shader_name, uniform, ...) do {                     \
    const Vector3 tmp = (__VA_ARGS__);                                          \
    if (!Vector3Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {          \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                           \
        glUniform3fv(                                                           \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            1,                                                                  \
            (float*)(&tmp)                                                      \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC3_CUSTOM(custom, shader_name, uniform, ...) do {      \
    const Vector3 tmp = (__VA_ARGS__);                                          \
    if (!Vector3Equals(((r3d_shader_custom_t*)(custom))->shader_name.uniform.val, tmp)) { \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = tmp;        \
        glUniform3fv(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC3_SELECT(shader_name, custom, uniform, ...) do {      \
    const Vector3 tmp = (__VA_ARGS__);                                          \
    if (!Vector3Equals(R3D_SHADER_GET(shader_name, custom)->uniform.val, tmp)) {\
        R3D_SHADER_GET(shader_name, custom)->uniform.val = tmp;                 \
        glUniform3fv(R3D_SHADER_GET(shader_name, custom)->uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC4(shader_name, uniform, ...) do {                     \
    const Vector4 tmp = (__VA_ARGS__);                                          \
    if (!Vector4Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {          \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                           \
        glUniform4fv(                                                           \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            1,                                                                  \
            (float*)(&tmp)                                                      \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC4_CUSTOM(custom, shader_name, uniform, ...) do {      \
    const Vector4 tmp = (__VA_ARGS__);                                          \
    if (!Vector4Equals(((r3d_shader_custom_t*)(custom))->shader_name.uniform.val, tmp)) { \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = tmp;        \
        glUniform4fv(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_VEC4_SELECT(shader_name, custom, uniform, ...) do {      \
    const Vector4 tmp = (__VA_ARGS__);                                          \
    if (!Vector4Equals(R3D_SHADER_GET(shader_name, custom)->uniform.val, tmp)) {\
        R3D_SHADER_GET(shader_name, custom)->uniform.val = tmp;                 \
        glUniform4fv(R3D_SHADER_GET(shader_name, custom)->uniform.loc, 1, (float*)(&tmp)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL3(shader_name, uniform, space, ...) do {              \
    const Color tmp = (__VA_ARGS__);                                            \
    if (R3D_MOD_SHADER.shader_name.uniform.colorSpace != (space) ||             \
        memcmp(&R3D_MOD_SHADER.shader_name.uniform.val, &tmp,                   \
               sizeof(Color)) != 0) {                                           \
        Vector3 v = r3d_color_to_linear_vec3(tmp, (space));                     \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                           \
        R3D_MOD_SHADER.shader_name.uniform.colorSpace = (space);                \
        glUniform3fv(                                                           \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            1,                                                                  \
            (float*)(&v)                                                        \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL3_CUSTOM(custom, shader_name, uniform, space, ...) do { \
    const Color tmp = (__VA_ARGS__);                                            \
    if (((r3d_shader_custom_t*)(custom))->shader_name.uniform.colorSpace != (space) || \
        memcmp(&((r3d_shader_custom_t*)(custom))->shader_name.uniform.val, &tmp, sizeof(Color)) != 0) { \
        Vector3 v = r3d_color_to_linear_vec3(tmp, (space));                     \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = tmp;        \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.colorSpace = (space); \
        glUniform3fv(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, 1, (float*)(&v)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL3_SELECT(shader_name, custom, uniform, space, ...) do { \
    const Color tmp = (__VA_ARGS__);                                            \
    if (R3D_SHADER_GET(shader_name, custom)->uniform.colorSpace != (space) ||   \
        memcmp(&R3D_SHADER_GET(shader_name, custom)->uniform.val, &tmp, sizeof(Color)) != 0) { \
        Vector3 v = r3d_color_to_linear_vec3(tmp, (space));                     \
        R3D_SHADER_GET(shader_name, custom)->uniform.val = tmp;                 \
        R3D_SHADER_GET(shader_name, custom)->uniform.colorSpace = (space);      \
        glUniform3fv(R3D_SHADER_GET(shader_name, custom)->uniform.loc, 1, (float*)(&v)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL4(shader_name, uniform, space, ...) do {              \
    const Color tmp = (__VA_ARGS__);                                            \
    if (R3D_MOD_SHADER.shader_name.uniform.colorSpace != (space) ||             \
        memcmp(&R3D_MOD_SHADER.shader_name.uniform.val, &tmp,                   \
               sizeof(Color)) != 0) {                                           \
        Vector4 v = r3d_color_to_linear_vec4(tmp, (space));                     \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                           \
        R3D_MOD_SHADER.shader_name.uniform.colorSpace = (space);                \
        glUniform4fv(                                                           \
            R3D_MOD_SHADER.shader_name.uniform.loc,                             \
            1,                                                                  \
            (float*)(&v)                                                        \
        );                                                                      \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL4_CUSTOM(custom, shader_name, uniform, space, ...) do { \
    const Color tmp = (__VA_ARGS__);                                            \
    if (((r3d_shader_custom_t*)(custom))->shader_name.uniform.colorSpace != (space) || \
        memcmp(&((r3d_shader_custom_t*)(custom))->shader_name.uniform.val, &tmp, sizeof(Color)) != 0) { \
        Vector4 v = r3d_color_to_linear_vec4(tmp, (space));                     \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.val = tmp;        \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.colorSpace = (space); \
        glUniform4fv(((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc, 1, (float*)(&v)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_COL4_SELECT(shader_name, custom, uniform, space, ...) do { \
    const Color tmp = (__VA_ARGS__);                                            \
    if (R3D_SHADER_GET(shader_name, custom)->uniform.colorSpace != (space) ||   \
        memcmp(&R3D_SHADER_GET(shader_name, custom)->uniform.val, &tmp, sizeof(Color)) != 0) { \
        Vector4 v = r3d_color_to_linear_vec4(tmp, (space));                     \
        R3D_SHADER_GET(shader_name, custom)->uniform.val = tmp;                 \
        R3D_SHADER_GET(shader_name, custom)->uniform.colorSpace = (space);      \
        glUniform4fv(R3D_SHADER_GET(shader_name, custom)->uniform.loc, 1, (float*)(&v)); \
    }                                                                           \
} while(0)

#define R3D_SHADER_SET_MAT4(shader_name, uniform, value) do {                   \
    glUniformMatrix4fv(                                                         \
        R3D_MOD_SHADER.shader_name.uniform.loc,                                 \
        1,                                                                      \
        GL_TRUE,                                                                \
        (float*)(&(value))                                                      \
    );                                                                          \
} while(0)

#define R3D_SHADER_SET_MAT4_CUSTOM(custom, shader_name, uniform, value) do {    \
    glUniformMatrix4fv(                                                         \
        ((r3d_shader_custom_t*)(custom))->shader_name.uniform.loc,              \
        1,                                                                      \
        GL_TRUE,                                                                \
        (float*)(&(value))                                                      \
    );                                                                          \
} while(0)

#define R3D_SHADER_SET_MAT4_SELECT(shader_name, custom, uniform, value) do {    \
    glUniformMatrix4fv(                                                         \
        R3D_SHADER_GET(shader_name, custom)->uniform.loc,                       \
        1,                                                                      \
        GL_TRUE,                                                                \
        (float*)(&(value))                                                      \
    );                                                                          \
} while(0)

// ========================================
// SAMPLER ENUMS
// ========================================

/*
 * Slot '0' is reserved for texture operations exclusively on the client side.
 * Note that the specification guarantees at least 80 binding slots for textures.
 * See: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glActiveTexture.xhtml
 */
typedef enum {

    // Material maps
    R3D_SHADER_SAMPLER_MAP_ALBEDO            = 1,
    R3D_SHADER_SAMPLER_MAP_EMISSION          = 2,
    R3D_SHADER_SAMPLER_MAP_NORMAL            = 3,
    R3D_SHADER_SAMPLER_MAP_ORM               = 4,

    // Shadow maps
    R3D_SHADER_SAMPLER_SHADOW_DIR            = 10,
    R3D_SHADER_SAMPLER_SHADOW_SPOT           = 11,
    R3D_SHADER_SAMPLER_SHADOW_OMNI           = 12,

    // IBL maps
    R3D_SHADER_SAMPLER_IBL_IRRADIANCE        = 15,
    R3D_SHADER_SAMPLER_IBL_PREFILTER         = 16,
    R3D_SHADER_SAMPLER_IBL_BRDF_LUT          = 17,

    // Scene miscs
    R3D_SHADER_SAMPLER_BONE_MATRICES         = 20,

    // Buffers
    R3D_SHADER_SAMPLER_BUFFER_DEPTH          = 25,
    R3D_SHADER_SAMPLER_BUFFER_GEOM_NORMAL    = 26,
    R3D_SHADER_SAMPLER_BUFFER_ALBEDO         = 27,
    R3D_SHADER_SAMPLER_BUFFER_NORMAL         = 28,
    R3D_SHADER_SAMPLER_BUFFER_ORM            = 29,
    R3D_SHADER_SAMPLER_BUFFER_DIFFUSE        = 30,
    R3D_SHADER_SAMPLER_BUFFER_SPECULAR       = 31,
    R3D_SHADER_SAMPLER_BUFFER_SSAO           = 32,
    R3D_SHADER_SAMPLER_BUFFER_SSIL           = 33,
    R3D_SHADER_SAMPLER_BUFFER_SSGI           = 34,
    R3D_SHADER_SAMPLER_BUFFER_SSR            = 35,
    R3D_SHADER_SAMPLER_BUFFER_BLOOM          = 36,
    R3D_SHADER_SAMPLER_BUFFER_DOF_COC        = 37,
    R3D_SHADER_SAMPLER_BUFFER_DOF            = 38,
    R3D_SHADER_SAMPLER_BUFFER_SCENE          = 39,

    // Unamed for special passes
    R3D_SHADER_SAMPLER_SOURCE_2D             = 40,
    R3D_SHADER_SAMPLER_SOURCE_CUBE           = 41,

    // Custom samplers
    R3D_SHADER_SAMPLER_CUSTOM_1D             = 45,
    R3D_SHADER_SAMPLER_CUSTOM_2D             = R3D_SHADER_SAMPLER_CUSTOM_1D + R3D_MAX_SHADER_SAMPLERS,
    R3D_SHADER_SAMPLER_CUSTOM_3D             = R3D_SHADER_SAMPLER_CUSTOM_2D + R3D_MAX_SHADER_SAMPLERS,
    R3D_SHADER_SAMPLER_CUSTOM_CUBE           = R3D_SHADER_SAMPLER_CUSTOM_3D + R3D_MAX_SHADER_SAMPLERS,
    R3D_SHADER_SAMPLER_CUSTOM_SENTINEL       = R3D_SHADER_SAMPLER_CUSTOM_CUBE + R3D_MAX_SHADER_SAMPLERS,

    // Sentinel
    R3D_SHADER_SAMPLER_COUNT = R3D_SHADER_SAMPLER_CUSTOM_SENTINEL

} r3d_shader_sampler_t;

// ========================================
// SAMPLER TYPES
// ========================================

static const GLenum R3D_MOD_SHADER_SAMPLER_TYPES[R3D_SHADER_SAMPLER_COUNT] =
{
    [R3D_SHADER_SAMPLER_MAP_ALBEDO]            = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_EMISSION]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_NORMAL]            = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_ORM]               = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SHADOW_DIR]            = GL_TEXTURE_2D_ARRAY,
    [R3D_SHADER_SAMPLER_SHADOW_SPOT]           = GL_TEXTURE_2D_ARRAY,
    [R3D_SHADER_SAMPLER_SHADOW_OMNI]           = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_IRRADIANCE]        = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_PREFILTER]         = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_BRDF_LUT]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BONE_MATRICES]         = GL_TEXTURE_1D,
    [R3D_SHADER_SAMPLER_BUFFER_DEPTH]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_ALBEDO]         = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_NORMAL]         = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_ORM]            = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_DIFFUSE]        = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SPECULAR]       = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_GEOM_NORMAL]    = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSAO]           = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSIL]           = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSGI]           = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSR]            = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_BLOOM]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_DOF_COC]        = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_DOF]            = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SCENE]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SOURCE_2D]             = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SOURCE_CUBE]           = GL_TEXTURE_CUBE_MAP,

    // NOTE: Custom samplers are defined at initialization
    //       time in `R3D_MOD_SHADER.samplerTargets`
};

// ========================================
// UNIFORMS TYPES
// ========================================

/* Represents any sampler type, stores only the corresponding texture slot */
typedef struct { int slot; } r3d_shader_uniform_sampler_t;

/* Represents scalars (bool/int and float), stores the value and uniform location */
typedef struct { int val; int loc; } r3d_shader_uniform_int_t;
typedef struct { float val; int loc; } r3d_shader_uniform_float_t;

/* Represents vectors, stores the value and uniform location */
typedef struct { Vector2 val; int loc; } r3d_shader_uniform_vec2_t;
typedef struct { Vector3 val; int loc; } r3d_shader_uniform_vec3_t;
typedef struct { Vector4 val; int loc; } r3d_shader_uniform_vec4_t;

/* Represents SDR color vectors plus the color space they should be interpreted in */
typedef struct { Color val; R3D_ColorSpace colorSpace; int loc; } r3d_shader_uniform_col3_t;
typedef struct { Color val; R3D_ColorSpace colorSpace; int loc; } r3d_shader_uniform_col4_t;

/* Represents matrices, stores only the uniform location for efficiency */
typedef struct { int loc; } r3d_shader_uniform_mat4_t;

// ========================================
// UNIFORM BLOCK ENUMS
// ========================================

typedef enum {
    R3D_SHADER_BLOCK_VIEW,
    R3D_SHADER_BLOCK_ENV,
    R3D_SHADER_BLOCK_LIGHT,
    R3D_SHADER_BLOCK_LIGHT_ARRAY,
    R3D_SHADER_BLOCK_COUNT
} r3d_shader_block_t;

// ========================================
// UNIFORM BLOCK STRUCTS
// ========================================

typedef struct {
    alignas(16) Vector3 position;
    alignas(16) Matrix view;
    alignas(16) Matrix invView;
    alignas(16) Matrix proj;
    alignas(16) Matrix invProj;
    alignas(16) Matrix viewProj;
    alignas(4) int32_t projMode;
    alignas(4) float aspect;
    alignas(4) float near;
    alignas(4) float far;
} r3d_shader_block_view_t;

typedef struct {

    struct r3d_shader_block_env_probe
    {
        alignas(16) Vector3 position;
        alignas(4) float falloff;
        alignas(4) float range;
        alignas(4) int32_t irradiance;
        alignas(4) int32_t prefilter;
    }
    uProbes[R3D_MAX_PROBE_ON_SCREEN];

    struct r3d_shader_block_env_ambient
    {
        alignas(16) Vector4 rotation;
        alignas(16) Vector4 color;
        alignas(4) float energy;
        alignas(4) int32_t irradiance;
        alignas(4) int32_t prefilter;
    }
    uAmbient;

    alignas(4) int32_t uNumPrefilterLevels;
    alignas(4) int32_t uNumProbes;

} r3d_shader_block_env_t;

typedef struct {
    alignas(16) Matrix viewProj;
    alignas(16) Vector3 color;
    alignas(16) Vector3 position;
    alignas(16) Vector3 direction;
    alignas(4) float specular;
    alignas(4) float energy;
    alignas(4) float range;
    alignas(4) float near;
    alignas(4) float far;
    alignas(4) float attenuation;
    alignas(4) float innerCutOff;
    alignas(4) float outerCutOff;
    alignas(4) float shadowSoftness;
    alignas(4) float shadowDepthBias;
    alignas(4) float shadowSlopeBias;
    alignas(4) int32_t shadowLayer;
    alignas(4) int32_t type;
} r3d_shader_block_light_t;

typedef struct {
    alignas(16) r3d_shader_block_light_t uLights[R3D_MAX_LIGHT_FORWARD_PER_MESH];
    alignas(4) int32_t uNumLights;
} r3d_shader_block_light_array_t;

// ========================================
// BUILT-IN SHADERS STRUCTURES
// ========================================

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uStepSize;
} r3d_shader_prepare_atrous_wavelet_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_vec2_t uSourceTexel;
} r3d_shader_prepare_bicubic_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_vec2_t uSourceTexel;
} r3d_shader_prepare_lanczos_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_int_t uSourceLod;
} r3d_shader_prepare_blur_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_int_t uSourceLod;
} r3d_shader_prepare_blur_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
} r3d_shader_prepare_ssao_in_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uSampleCount;
    r3d_shader_uniform_float_t uRadius;
    r3d_shader_uniform_float_t uBias;
    r3d_shader_uniform_float_t uIntensity;
    r3d_shader_uniform_float_t uPower;
} r3d_shader_prepare_ssao_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSsaoTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_vec2_t uDirection;
} r3d_shader_prepare_ssao_blur_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
} r3d_shader_prepare_ssil_in_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uHistoryTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_float_t uSampleCount;
    r3d_shader_uniform_float_t uSampleRadius;
    r3d_shader_uniform_float_t uSliceCount;
    r3d_shader_uniform_float_t uHitThickness;
    r3d_shader_uniform_float_t uConvergence;
    r3d_shader_uniform_float_t uAoPower;
    r3d_shader_uniform_float_t uBounce;
} r3d_shader_prepare_ssil_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
} r3d_shader_prepare_ssgi_in_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uHistoryTex;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uSampleCount;
    r3d_shader_uniform_int_t uMaxRaySteps;
    r3d_shader_uniform_float_t uStepSize;
    r3d_shader_uniform_float_t uThickness;
    r3d_shader_uniform_float_t uMaxDistance;
    r3d_shader_uniform_float_t uFadeStart;
    r3d_shader_uniform_float_t uFadeEnd;
} r3d_shader_prepare_ssgi_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uSpecularTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
} r3d_shader_prepare_ssr_in_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uSpecularTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uMaxRaySteps;
    r3d_shader_uniform_int_t uBinarySteps;
    r3d_shader_uniform_float_t uStepSize;
    r3d_shader_uniform_float_t uThickness;
    r3d_shader_uniform_float_t uMaxDistance;
    r3d_shader_uniform_float_t uEdgeFade;
} r3d_shader_prepare_ssr_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uTexture;
    r3d_shader_uniform_vec2_t uTexelSize;
    r3d_shader_uniform_vec4_t uPrefilter;
    r3d_shader_uniform_int_t uDstLevel;
} r3d_shader_prepare_bloom_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uTexture;
    r3d_shader_uniform_vec2_t uFilterRadius;
    r3d_shader_uniform_float_t uSrcLevel;
} r3d_shader_prepare_bloom_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_float_t uFocusPoint;
    r3d_shader_uniform_float_t uFocusScale;
} r3d_shader_prepare_dof_coc_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uCoCTex;
} r3d_shader_prepare_dof_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_float_t uMaxBlurSize;
} r3d_shader_prepare_dof_blur_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_sampler_t uPanoramaTex;
} r3d_shader_prepare_cubemap_from_equirectangular_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_sampler_t uSourceTex;
} r3d_shader_prepare_cubemap_irradiance_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_float_t uSourceNumLevels;
    r3d_shader_uniform_float_t uSourceFaceSize;
    r3d_shader_uniform_float_t uRoughness;
} r3d_shader_prepare_cubemap_prefilter_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_col3_t uSkyTopColor;
    r3d_shader_uniform_col3_t uSkyHorizonColor;
    r3d_shader_uniform_float_t uSkyHorizonCurve;
    r3d_shader_uniform_float_t uSkyEnergy;
    r3d_shader_uniform_col3_t uGroundBottomColor;
    r3d_shader_uniform_col3_t uGroundHorizonColor;
    r3d_shader_uniform_float_t uGroundHorizonCurve;
    r3d_shader_uniform_float_t uGroundEnergy;
    r3d_shader_uniform_vec3_t uSunDirection;
    r3d_shader_uniform_col3_t uSunColor;
    r3d_shader_uniform_float_t uSunSize;
    r3d_shader_uniform_float_t uSunCurve;
    r3d_shader_uniform_float_t uSunEnergy;
} r3d_shader_prepare_cubemap_skybox_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_col3_t uEmissionColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_sampler_t uNormalMap;
    r3d_shader_uniform_sampler_t uEmissionMap;
    r3d_shader_uniform_sampler_t uOrmMap;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
} r3d_shader_scene_geometry_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_col3_t uEmissionColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_sampler_t uEmissionMap;
    r3d_shader_uniform_sampler_t uNormalMap;
    r3d_shader_uniform_sampler_t uOrmMap;
    r3d_shader_uniform_sampler_t uShadowDirTex;
    r3d_shader_uniform_sampler_t uShadowSpotTex;
    r3d_shader_uniform_sampler_t uShadowOmniTex;
    r3d_shader_uniform_sampler_t uIrradianceTex;
    r3d_shader_uniform_sampler_t uPrefilterTex;
    r3d_shader_uniform_sampler_t uBrdfLutTex;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
    r3d_shader_uniform_vec3_t uViewPosition;
} r3d_shader_scene_forward_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatViewProj;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_float_t uAlphaCutoff;
} r3d_shader_scene_unlit_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatViewProj;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_float_t uAlphaCutoff;
} r3d_shader_scene_depth_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatViewProj;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_float_t uFar;
} r3d_shader_scene_depth_cube_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uBoneMatricesTex;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatViewProj;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_col3_t uEmissionColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_sampler_t uEmissionMap;
    r3d_shader_uniform_sampler_t uNormalMap;
    r3d_shader_uniform_sampler_t uOrmMap;
    r3d_shader_uniform_sampler_t uShadowDirTex;
    r3d_shader_uniform_sampler_t uShadowSpotTex;
    r3d_shader_uniform_sampler_t uShadowOmniTex;
    r3d_shader_uniform_sampler_t uIrradianceTex;
    r3d_shader_uniform_sampler_t uPrefilterTex;
    r3d_shader_uniform_sampler_t uBrdfLutTex;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_int_t uProbeInterior;
} r3d_shader_scene_probe_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_col4_t uAlbedoColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_col3_t uEmissionColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_sampler_t uNormalMap;
    r3d_shader_uniform_sampler_t uEmissionMap;
    r3d_shader_uniform_sampler_t uOrmMap;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uGeomNormalTex;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
    r3d_shader_uniform_float_t uNormalThreshold;
    r3d_shader_uniform_float_t uFadeWidth;
    r3d_shader_uniform_int_t uApplyColor;
} r3d_shader_scene_decal_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_vec4_t uColor;
} r3d_shader_scene_background_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatInvProj;
    r3d_shader_uniform_sampler_t uSkyMap;
    r3d_shader_uniform_vec4_t uRotation;
    r3d_shader_uniform_float_t uEnergy;
    r3d_shader_uniform_float_t uLod;
} r3d_shader_scene_skybox_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uSsaoTex;
    r3d_shader_uniform_sampler_t uSsilTex;
    r3d_shader_uniform_sampler_t uSsgiTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uIrradianceTex;
    r3d_shader_uniform_sampler_t uPrefilterTex;
    r3d_shader_uniform_sampler_t uBrdfLutTex;
    r3d_shader_uniform_float_t uSsilEnergy;
} r3d_shader_deferred_ambient_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uShadowDirTex;
    r3d_shader_uniform_sampler_t uShadowSpotTex;
    r3d_shader_uniform_sampler_t uShadowOmniTex;
} r3d_shader_deferred_lighting_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uSpecularTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uSsrTex;
    r3d_shader_uniform_float_t uSsrNumLevels;
} r3d_shader_deferred_compose_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uFogMode;
    r3d_shader_uniform_col3_t uFogColor;
    r3d_shader_uniform_float_t uFogStart;
    r3d_shader_uniform_float_t uFogEnd;
    r3d_shader_uniform_float_t uFogDensity;
    r3d_shader_uniform_float_t uSkyAffect;
} r3d_shader_post_fog_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uBloomTex;
    r3d_shader_uniform_int_t uBloomMode;
    r3d_shader_uniform_float_t uBloomIntensity;
} r3d_shader_post_bloom_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uBlurTex;
} r3d_shader_post_dof_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_vec2_t uResolution;
    r3d_shader_uniform_vec2_t uTexelSize;
} r3d_shader_post_screen_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSceneTex;
    r3d_shader_uniform_float_t uTonemapExposure;
    r3d_shader_uniform_float_t uTonemapWhite;
    r3d_shader_uniform_int_t uTonemapMode;
    r3d_shader_uniform_float_t uBrightness;
    r3d_shader_uniform_float_t uContrast;
    r3d_shader_uniform_float_t uSaturation;
} r3d_shader_post_output_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_vec2_t uSourceTexel;
} r3d_shader_post_fxaa_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_int_t uOutputMode;
} r3d_shader_post_visualizer_t;

// ========================================
// CUSTOM SHADERS STRUCTURES
// ========================================

typedef struct {
    union {
        // Must follow the same naming pattern as `r3d_mod_shader`
        struct {
            r3d_shader_scene_geometry_t geometry;
            r3d_shader_scene_forward_t forward;
            r3d_shader_scene_unlit_t unlit;
            r3d_shader_scene_depth_t depth;
            r3d_shader_scene_depth_cube_t depthCube;
            r3d_shader_scene_probe_t probe;
            r3d_shader_scene_decal_t decal;
        } scene;

        // Must follow the same naming pattern as `r3d_shader_loader`
        struct {
            r3d_shader_post_screen_t screen;
        } post;
    };
    r3d_rshade_sampler_t samplers[R3D_MAX_SHADER_SAMPLERS];
    r3d_rshade_uniform_buffer_t uniforms;
    const char* userCode;
} r3d_shader_custom_t;

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_mod_shader {

    // Program state
    GLuint currentProgram;

    // Samplers state
    GLenum samplerTargets[R3D_SHADER_SAMPLER_COUNT];
    GLuint samplerBindings[R3D_SHADER_SAMPLER_COUNT];

    // Uniform buffers
    GLuint uniformBuffers[R3D_SHADER_BLOCK_COUNT];

    // Prepare shaders
    struct {
        r3d_shader_prepare_atrous_wavelet_t atrousWavelet;
        r3d_shader_prepare_bicubic_up_t bicubicUp;
        r3d_shader_prepare_lanczos_up_t lanczosUp;
        r3d_shader_prepare_blur_down_t blurDown;
        r3d_shader_prepare_blur_up_t blurUp;
        r3d_shader_prepare_ssao_in_down_t ssaoInDown;
        r3d_shader_prepare_ssao_t ssao;
        r3d_shader_prepare_ssao_blur_t ssaoBlur;
        r3d_shader_prepare_ssil_in_down_t ssilInDown;
        r3d_shader_prepare_ssil_t ssil;
        r3d_shader_prepare_ssgi_in_down_t ssgiInDown;
        r3d_shader_prepare_ssgi_t ssgi;
        r3d_shader_prepare_ssr_in_down_t ssrInDown;
        r3d_shader_prepare_ssr_t ssr;
        r3d_shader_prepare_bloom_down_t bloomDown;
        r3d_shader_prepare_bloom_up_t bloomUp;
        r3d_shader_prepare_dof_coc_t dofCoc;
        r3d_shader_prepare_dof_down_t dofDown;
        r3d_shader_prepare_dof_blur_t dofBlur;
        r3d_shader_prepare_cubemap_from_equirectangular_t cubemapFromEquirectangular;
        r3d_shader_prepare_cubemap_irradiance_t cubemapIrradiance;
        r3d_shader_prepare_cubemap_prefilter_t cubemapPrefilter;
        r3d_shader_prepare_cubemap_skybox_t cubemapSkybox;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_scene_geometry_t geometry;
        r3d_shader_scene_forward_t forward;
        r3d_shader_scene_unlit_t unlit;
        r3d_shader_scene_background_t background;
        r3d_shader_scene_skybox_t skybox;
        r3d_shader_scene_depth_t depth;
        r3d_shader_scene_depth_cube_t depthCube;
        r3d_shader_scene_probe_t probe;
        r3d_shader_scene_decal_t decal;
    } scene;

    // Deferred shaders
    struct {
        r3d_shader_deferred_ambient_t ambient;
        r3d_shader_deferred_lighting_t lighting;
        r3d_shader_deferred_compose_t compose;
    } deferred;

    // Post shaders
    struct {
        r3d_shader_post_fog_t fog;
        r3d_shader_post_bloom_t bloom;
        r3d_shader_post_dof_t dof;
        r3d_shader_post_output_t output;
        r3d_shader_post_fxaa_t fxaa;
        r3d_shader_post_visualizer_t visualizer;
    } post;

} R3D_MOD_SHADER;

// ========================================
// BUILT-IN SHADER LOADER
// ========================================

typedef bool (*r3d_shader_loader_func)(r3d_shader_custom_t* custom);

bool r3d_shader_load_prepare_atrous_wavelet(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_bicubic_up(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_lanczos_up(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_blur_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_blur_up(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssao_in_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssao(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssao_blur(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssil_in_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssil(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssgi_in_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssgi(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssr_in_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_ssr(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_bloom_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_bloom_up(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_dof_coc(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_dof_down(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_dof_blur(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_cubemap_from_equirectangular(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_cubemap_irradiance(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_cubemap_prefilter(r3d_shader_custom_t* custom);
bool r3d_shader_load_prepare_cubemap_skybox(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_geometry(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_forward(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_unlit(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_background(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_skybox(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_depth(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_depth_cube(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_probe(r3d_shader_custom_t* custom);
bool r3d_shader_load_scene_decal(r3d_shader_custom_t* custom);
bool r3d_shader_load_deferred_ambient(r3d_shader_custom_t* custom);
bool r3d_shader_load_deferred_lighting(r3d_shader_custom_t* custom);
bool r3d_shader_load_deferred_compose(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_fog(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_bloom(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_dof(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_screen(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_output(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_fxaa(r3d_shader_custom_t* custom);
bool r3d_shader_load_post_visualizer(r3d_shader_custom_t* custom);

static const struct r3d_shader_loader {

    // Prepare shaders
    struct {
        r3d_shader_loader_func atrousWavelet;
        r3d_shader_loader_func bicubicUp;
        r3d_shader_loader_func lanczosUp;
        r3d_shader_loader_func blurDown;
        r3d_shader_loader_func blurUp;
        r3d_shader_loader_func ssaoInDown;
        r3d_shader_loader_func ssao;
        r3d_shader_loader_func ssaoBlur;
        r3d_shader_loader_func ssilInDown;
        r3d_shader_loader_func ssil;
        r3d_shader_loader_func ssgiInDown;
        r3d_shader_loader_func ssgi;
        r3d_shader_loader_func ssrInDown;
        r3d_shader_loader_func ssr;
        r3d_shader_loader_func bloomDown;
        r3d_shader_loader_func bloomUp;
        r3d_shader_loader_func dofCoc;
        r3d_shader_loader_func dofDown;
        r3d_shader_loader_func dofBlur;
        r3d_shader_loader_func cubemapFromEquirectangular;
        r3d_shader_loader_func cubemapIrradiance;
        r3d_shader_loader_func cubemapPrefilter;
        r3d_shader_loader_func cubemapSkybox;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_loader_func geometry;
        r3d_shader_loader_func forward;
        r3d_shader_loader_func unlit;
        r3d_shader_loader_func background;
        r3d_shader_loader_func skybox;
        r3d_shader_loader_func depth;
        r3d_shader_loader_func depthCube;
        r3d_shader_loader_func probe;
        r3d_shader_loader_func decal;
    } scene;

    // Deferred shaders
    struct {
        r3d_shader_loader_func ambient;
        r3d_shader_loader_func lighting;
        r3d_shader_loader_func compose;
     } deferred;

    // Post shaders
    struct {
        r3d_shader_loader_func fog;
        r3d_shader_loader_func bloom;
        r3d_shader_loader_func dof;
        r3d_shader_loader_func screen;
        r3d_shader_loader_func output;
        r3d_shader_loader_func fxaa;
        r3d_shader_loader_func visualizer;
    } post;

} R3D_MOD_SHADER_LOADER = {

    .prepare = {
        .atrousWavelet = r3d_shader_load_prepare_atrous_wavelet,
        .bicubicUp = r3d_shader_load_prepare_bicubic_up,
        .lanczosUp = r3d_shader_load_prepare_lanczos_up,
        .blurDown = r3d_shader_load_prepare_blur_down,
        .blurUp = r3d_shader_load_prepare_blur_up,
        .ssaoInDown = r3d_shader_load_prepare_ssao_in_down,
        .ssao = r3d_shader_load_prepare_ssao,
        .ssaoBlur = r3d_shader_load_prepare_ssao_blur,
        .ssilInDown = r3d_shader_load_prepare_ssil_in_down,
        .ssil = r3d_shader_load_prepare_ssil,
        .ssgiInDown = r3d_shader_load_prepare_ssgi_in_down,
        .ssgi = r3d_shader_load_prepare_ssgi,
        .ssrInDown = r3d_shader_load_prepare_ssr_in_down,
        .ssr = r3d_shader_load_prepare_ssr,
        .bloomDown = r3d_shader_load_prepare_bloom_down,
        .bloomUp = r3d_shader_load_prepare_bloom_up,
        .dofCoc = r3d_shader_load_prepare_dof_coc,
        .dofDown = r3d_shader_load_prepare_dof_down,
        .dofBlur = r3d_shader_load_prepare_dof_blur,
        .cubemapFromEquirectangular = r3d_shader_load_prepare_cubemap_from_equirectangular,
        .cubemapIrradiance = r3d_shader_load_prepare_cubemap_irradiance,
        .cubemapPrefilter = r3d_shader_load_prepare_cubemap_prefilter,
        .cubemapSkybox = r3d_shader_load_prepare_cubemap_skybox,
    },

    .scene = {
        .geometry = r3d_shader_load_scene_geometry,
        .forward = r3d_shader_load_scene_forward,
        .unlit = r3d_shader_load_scene_unlit,
        .background = r3d_shader_load_scene_background,
        .skybox = r3d_shader_load_scene_skybox,
        .depth = r3d_shader_load_scene_depth,
        .depthCube = r3d_shader_load_scene_depth_cube,
        .probe = r3d_shader_load_scene_probe,
        .decal = r3d_shader_load_scene_decal,
    },

    .deferred = {
        .ambient = r3d_shader_load_deferred_ambient,
        .lighting = r3d_shader_load_deferred_lighting,
        .compose = r3d_shader_load_deferred_compose
    },

    .post = {
        .fog = r3d_shader_load_post_fog,
        .bloom = r3d_shader_load_post_bloom,
        .dof = r3d_shader_load_post_dof,
        .screen = r3d_shader_load_post_screen,
        .output = r3d_shader_load_post_output,
        .fxaa = r3d_shader_load_post_fxaa,
        .visualizer = r3d_shader_load_post_visualizer,
    },

};

// ========================================
// MODULE FUNCTIONS
// ========================================

/*
 * Module initialization function.
 * Called once during `R3D_Init()`
 */
bool r3d_shader_init();

/*
 * Module deinitialization function.
 * Called once during `R3D_Close()`
 */
void r3d_shader_quit();

/*
 * Binds the texture to the specified sampler.
 * Called by `R3D_SHADER_BIND_SAMPLER`, no need to call it manually.
 */
void r3d_shader_bind_sampler(r3d_shader_sampler_t sampler, GLuint texture);

/*
 * Upload and bind the specified uniform block with the provided data.
 */
void r3d_shader_set_uniform_block(r3d_shader_block_t block, const void* data);

/*
 * Sets the value of a client-side uniform, marks its state as dirty, and flags it for upload.
 */
bool r3d_shader_set_custom_uniform(r3d_shader_custom_t* shader, const char* name, const void* value);

/*
 * Assigns a texture to a sampler; it must be bound afterwards.
 */
bool r3d_shader_set_custom_sampler(r3d_shader_custom_t* shader, const char* name, Texture texture);

/*
 * Checks if any uniforms are dirty and need to be uploaded then bind it.
 * Automatically called when `R3D_SHADER_USE` (OVR/OPT) is invoked with a custom shader.
 */
void r3d_shader_bind_custom_uniforms(r3d_shader_custom_t* shader);

/*
 * Binds the textures of a custom shader and verifies the state of its samplers then bind them.
 * Automatically called when `R3D_SHADER_USE` (OVR/OPT) is invoked with a custom shader.
 */
void r3d_shader_bind_custom_samplers(r3d_shader_custom_t* shader);

/*
 * Invalidate the internal state cache.
 * Use program zero, and unbind all textures.
 */
void r3d_shader_invalidate_cache(void);

#endif // R3D_MODULE_SHADER_H
