/* r3d_shader.h -- Internal R3D shader module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_SHADER_H
#define R3D_MODULE_SHADER_H

#include <r3d/r3d_core.h>
#include <stdalign.h>
#include <raylib.h>
#include <stdint.h>
#include <glad.h>

#include "../common/r3d_math.h"

// ========================================
// MODULE CONSTANTS
// ========================================

#define R3D_SHADER_NUM_FORWARD_LIGHTS       8
#define R3D_SHADER_NUM_PROBES               8

#define R3D_SHADER_BLOCK_VIEW_SLOT          0
#define R3D_SHADER_BLOCK_ENV_SLOT           1
#define R3D_SHADER_BLOCK_LIGHT_SLOT         2
#define R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT   3

// ========================================
// SHADER MANAGEMENT MACROS
// ========================================

#define R3D_SHADER_USE(shader_name) do {                                                            \
    if (R3D_MOD_SHADER.shader_name.id == 0) {                                                       \
        R3D_MOD_SHADER_LOADER.shader_name();                                                        \
    }                                                                                               \
    glUseProgram(R3D_MOD_SHADER.shader_name.id);                                                    \
} while(0)

#define R3D_SHADER_SLOT_SAMPLER(shader_name, uniform)                                               \
    R3D_MOD_SHADER.shader_name.uniform.slot                                                         \

#define R3D_SHADER_BIND_SAMPLER(shader_name, uniform, texId) do {                                   \
    r3d_shader_bind_sampler(R3D_MOD_SHADER.shader_name.uniform.slot, (texId));                      \
} while(0)

#define R3D_SHADER_SET_INT(shader_name, uniform, value) do {                                        \
    if (R3D_MOD_SHADER.shader_name.uniform.val != (value)) {                                        \
        R3D_MOD_SHADER.shader_name.uniform.val = (value);                                           \
        glUniform1i(                                                                                \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            R3D_MOD_SHADER.shader_name.uniform.val                                                  \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_FLOAT(shader_name, uniform, value) do {                                      \
    if (R3D_MOD_SHADER.shader_name.uniform.val != (value)) {                                        \
        R3D_MOD_SHADER.shader_name.uniform.val = (value);                                           \
        glUniform1f(                                                                                \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            R3D_MOD_SHADER.shader_name.uniform.val                                                  \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_VEC2(shader_name, uniform, ...) do {                                         \
    const Vector2 tmp = (__VA_ARGS__);                                                              \
    if (!Vector2Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {                              \
        glUniform2fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, (float*)(&tmp));                    \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_VEC3(shader_name, uniform, ...) do {                                         \
    const Vector3 tmp = (__VA_ARGS__);                                                              \
    if (!Vector3Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {                              \
        glUniform3fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, (float*)(&tmp));                    \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_VEC4(shader_name, uniform, ...) do {                                         \
    const Vector4 tmp = (__VA_ARGS__);                                                              \
    if (!Vector4Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {                              \
        glUniform4fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, (float*)(&tmp));                    \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_COL3(shader_name, uniform, space, ...) do {                                  \
    const Color tmp = (__VA_ARGS__);                                                                \
    if (R3D_MOD_SHADER.shader_name.uniform.colorSpace != (space) ||                                 \
        memcmp(&R3D_MOD_SHADER.shader_name.uniform.val, &tmp, sizeof(Color)) != 0) {                \
        Vector3 v = r3d_color_to_linear_vec3(tmp, (space));                                         \
        glUniform3fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, (float*)(&v));                      \
        R3D_MOD_SHADER.shader_name.uniform.colorSpace = (space);                                    \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_COL4(shader_name, uniform, space, ...) do {                                  \
    const Color tmp = (__VA_ARGS__);                                                                \
    if (R3D_MOD_SHADER.shader_name.uniform.colorSpace != (space) ||                                 \
        memcmp(&R3D_MOD_SHADER.shader_name.uniform.val, &tmp, sizeof(Color)) != 0) {                \
        Vector4 v = r3d_color_to_linear_vec4(tmp, (space));                                         \
        glUniform4fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, (float*)(&v));                      \
        R3D_MOD_SHADER.shader_name.uniform.colorSpace = (space);                                    \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_MAT4(shader_name, uniform, value) do {                                       \
    glUniformMatrix4fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, GL_TRUE, (float*)(&(value)));     \
} while(0)

#define R3D_SHADER_SET_MAT4_V(shader_name, uniform, array, count) do {                              \
    glUniformMatrix4fv(R3D_MOD_SHADER.shader_name.uniform.loc, (count), GL_TRUE, (float*)(array));  \
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
    R3D_SHADER_SAMPLER_MAP_ALBEDO       = 1,
    R3D_SHADER_SAMPLER_MAP_EMISSION     = 2,
    R3D_SHADER_SAMPLER_MAP_NORMAL       = 3,
    R3D_SHADER_SAMPLER_MAP_ORM          = 4,

    // Shadow maps
    R3D_SHADER_SAMPLER_SHADOW_DIR       = 10,
    R3D_SHADER_SAMPLER_SHADOW_SPOT      = 11,
    R3D_SHADER_SAMPLER_SHADOW_OMNI      = 12,

    // IBL maps
    R3D_SHADER_SAMPLER_IBL_IRRADIANCE   = 15,
    R3D_SHADER_SAMPLER_IBL_PREFILTER    = 16,
    R3D_SHADER_SAMPLER_IBL_BRDF_LUT     = 17,

    // Scene miscs
    R3D_SHADER_SAMPLER_BONE_MATRICES    = 20,

    // Buffers
    R3D_SHADER_SAMPLER_BUFFER_DEPTH     = 25,
    R3D_SHADER_SAMPLER_BUFFER_ALBEDO    = 26,
    R3D_SHADER_SAMPLER_BUFFER_NORMAL    = 27,
    R3D_SHADER_SAMPLER_BUFFER_ORM       = 28,
    R3D_SHADER_SAMPLER_BUFFER_DIFFUSE   = 29,
    R3D_SHADER_SAMPLER_BUFFER_SPECULAR  = 30,
    R3D_SHADER_SAMPLER_BUFFER_SSAO      = 31,
    R3D_SHADER_SAMPLER_BUFFER_SSIL      = 32,
    R3D_SHADER_SAMPLER_BUFFER_SSR       = 33,
    R3D_SHADER_SAMPLER_BUFFER_BLOOM     = 34,
    R3D_SHADER_SAMPLER_BUFFER_SCENE     = 35,

    // Unamed for special passes
    R3D_SHADER_SAMPLER_SOURCE_2D        = 50,
    R3D_SHADER_SAMPLER_SOURCE_CUBE      = 55,

    // Sentinel
    R3D_SHADER_SAMPLER_COUNT

} r3d_shader_sampler_t;

// ========================================
// SAMPLER TYPES
// ========================================

static const GLenum R3D_MOD_SHADER_SAMPLER_TYPES[R3D_SHADER_SAMPLER_COUNT] =
{
    [R3D_SHADER_SAMPLER_MAP_ALBEDO]       = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_EMISSION]     = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_NORMAL]       = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_MAP_ORM]          = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SHADOW_DIR]       = GL_TEXTURE_2D_ARRAY,
    [R3D_SHADER_SAMPLER_SHADOW_SPOT]      = GL_TEXTURE_2D_ARRAY,
    [R3D_SHADER_SAMPLER_SHADOW_OMNI]      = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_IRRADIANCE]   = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_PREFILTER]    = GL_TEXTURE_CUBE_MAP_ARRAY,
    [R3D_SHADER_SAMPLER_IBL_BRDF_LUT]     = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BONE_MATRICES]    = GL_TEXTURE_1D,
    [R3D_SHADER_SAMPLER_BUFFER_DEPTH]     = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_ALBEDO]    = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_NORMAL]    = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_ORM]       = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_DIFFUSE]   = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SPECULAR]  = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSAO]      = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSIL]      = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SSR]       = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_BLOOM]     = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_BUFFER_SCENE]     = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SOURCE_2D]        = GL_TEXTURE_2D,
    [R3D_SHADER_SAMPLER_SOURCE_CUBE]      = GL_TEXTURE_CUBE_MAP,
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
    alignas(16) Vector3 viewPosition;
    alignas(16) Matrix view;
    alignas(16) Matrix invView;
    alignas(16) Matrix proj;
    alignas(16) Matrix invProj;
    alignas(16) Matrix viewProj;
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
    uProbes[R3D_SHADER_NUM_PROBES];

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
    alignas(4) float shadowTexelSize;
    alignas(4) float shadowDepthBias;
    alignas(4) float shadowSlopeBias;
    alignas(4) int32_t shadowLayer;
    alignas(4) int32_t enabled;
    alignas(4) int32_t shadow;
    alignas(4) int32_t type;
} r3d_shader_block_light_t;

typedef struct {
    alignas(16) r3d_shader_block_light_t uLights[R3D_SHADER_NUM_FORWARD_LIGHTS];
    alignas(4) int32_t uNumLights;
} r3d_shader_block_light_array_t;

// ========================================
// SHADER STRUCTURES DECLARATIONS
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
    r3d_shader_uniform_int_t uSampleCount;
    r3d_shader_uniform_float_t uRadius;
    r3d_shader_uniform_float_t uBias;
    r3d_shader_uniform_float_t uIntensity;
    r3d_shader_uniform_float_t uPower;
} r3d_shader_prepare_ssao_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uLightingTex;
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
    r3d_shader_uniform_float_t uEnergy;
} r3d_shader_prepare_ssil_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uLightingTex;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_int_t uMaxRaySteps;
    r3d_shader_uniform_int_t uBinarySearchSteps;
    r3d_shader_uniform_float_t uRayMarchLength;
    r3d_shader_uniform_float_t uDepthThickness;
    r3d_shader_uniform_float_t uDepthTolerance;
    r3d_shader_uniform_float_t uEdgeFadeStart;
    r3d_shader_uniform_float_t uEdgeFadeEnd;
    r3d_shader_uniform_vec3_t uAmbientColor;
    r3d_shader_uniform_float_t uAmbientEnergy;
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
    r3d_shader_uniform_sampler_t uSourceTex;
    r3d_shader_uniform_float_t uSourceTexel;
    r3d_shader_uniform_float_t uSourceLod;
    r3d_shader_uniform_int_t uSourceFace;
} r3d_shader_prepare_cubeface_down_t;

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
    r3d_shader_uniform_sampler_t uAlbedoMap;
    r3d_shader_uniform_sampler_t uNormalMap;
    r3d_shader_uniform_sampler_t uEmissionMap;
    r3d_shader_uniform_sampler_t uOrmMap;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
} r3d_shader_scene_decal_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_vec4_t uColor;
} r3d_shader_scene_background_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_vec4_t uRotation;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_sampler_t uSkyMap;
    r3d_shader_uniform_float_t uSkyEnergy;
} r3d_shader_scene_skybox_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uSsaoTex;
    r3d_shader_uniform_sampler_t uSsilTex;
    r3d_shader_uniform_sampler_t uSsrTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uIrradianceTex;
    r3d_shader_uniform_sampler_t uPrefilterTex;
    r3d_shader_uniform_sampler_t uBrdfLutTex;
    r3d_shader_uniform_float_t uMipCountSSR;
} r3d_shader_deferred_ambient_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uAlbedoTex;
    r3d_shader_uniform_sampler_t uNormalTex;
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_sampler_t uSsaoTex;
    r3d_shader_uniform_sampler_t uOrmTex;
    r3d_shader_uniform_sampler_t uShadowDirTex;
    r3d_shader_uniform_sampler_t uShadowSpotTex;
    r3d_shader_uniform_sampler_t uShadowOmniTex;
    r3d_shader_uniform_float_t uSSAOLightAffect;
} r3d_shader_deferred_lighting_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler_t uDiffuseTex;
    r3d_shader_uniform_sampler_t uSpecularTex;
} r3d_shader_deferred_compose_t;

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
    r3d_shader_uniform_sampler_t uDepthTex;
    r3d_shader_uniform_float_t uFocusPoint;
    r3d_shader_uniform_float_t uFocusScale;
    r3d_shader_uniform_float_t uMaxBlurSize;
    r3d_shader_uniform_int_t uDebugMode;
} r3d_shader_post_dof_t;

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

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_shader {

    // Samplers state
    GLuint samplerBindings[R3D_SHADER_SAMPLER_COUNT];

    // Uniform buffers
    GLuint uniformBuffers[R3D_SHADER_BLOCK_COUNT];

    // Prepare shaders
    struct {
        r3d_shader_prepare_atrous_wavelet_t atrousWavelet;
        r3d_shader_prepare_blur_down_t blurDown;
        r3d_shader_prepare_blur_up_t blurUp;
        r3d_shader_prepare_ssao_t ssao;
        r3d_shader_prepare_ssil_t ssil;
        r3d_shader_prepare_ssr_t ssr;
        r3d_shader_prepare_bloom_down_t bloomDown;
        r3d_shader_prepare_bloom_up_t bloomUp;
        r3d_shader_prepare_cubeface_down_t cubefaceDown;
        r3d_shader_prepare_cubemap_from_equirectangular_t cubemapFromEquirectangular;
        r3d_shader_prepare_cubemap_irradiance_t cubemapIrradiance;
        r3d_shader_prepare_cubemap_prefilter_t cubemapPrefilter;
        r3d_shader_prepare_cubemap_skybox_t cubemapSkybox;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_scene_geometry_t geometry;
        r3d_shader_scene_forward_t forward;
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
        r3d_shader_post_bloom_t bloom;
        r3d_shader_post_fog_t fog;
        r3d_shader_post_dof_t dof;
        r3d_shader_post_output_t output;
        r3d_shader_post_fxaa_t fxaa;
    } post;

} R3D_MOD_SHADER;

// ========================================
// BUILT-IN SHADER LOADER
// ========================================

typedef void (*r3d_shader_loader_func)(void);

void r3d_shader_load_prepare_atrous_wavelet(void);
void r3d_shader_load_prepare_blur_down(void);
void r3d_shader_load_prepare_blur_up(void);
void r3d_shader_load_prepare_ssao(void);
void r3d_shader_load_prepare_ssil(void);
void r3d_shader_load_prepare_ssr(void);
void r3d_shader_load_prepare_bloom_down(void);
void r3d_shader_load_prepare_bloom_up(void);
void r3d_shader_load_prepare_cubeface_down(void);
void r3d_shader_load_prepare_cubemap_from_equirectangular(void);
void r3d_shader_load_prepare_cubemap_irradiance(void);
void r3d_shader_load_prepare_cubemap_prefilter(void);
void r3d_shader_load_prepare_cubemap_skybox(void);
void r3d_shader_load_scene_geometry(void);
void r3d_shader_load_scene_forward(void);
void r3d_shader_load_scene_background(void);
void r3d_shader_load_scene_skybox(void);
void r3d_shader_load_scene_depth(void);
void r3d_shader_load_scene_depth_cube(void);
void r3d_shader_load_scene_probe(void);
void r3d_shader_load_scene_decal(void);
void r3d_shader_load_deferred_ambient(void);
void r3d_shader_load_deferred_lighting(void);
void r3d_shader_load_deferred_compose(void);
void r3d_shader_load_post_bloom(void);
void r3d_shader_load_post_fog(void);
void r3d_shader_load_post_dof(void);
void r3d_shader_load_post_output(void);
void r3d_shader_load_post_fxaa(void);

static const struct r3d_shader_loader {

    // Prepare shaders
    struct {
        r3d_shader_loader_func atrousWavelet;
        r3d_shader_loader_func blurDown;
        r3d_shader_loader_func blurUp;
        r3d_shader_loader_func ssao;
        r3d_shader_loader_func ssil;
        r3d_shader_loader_func ssr;
        r3d_shader_loader_func bloomDown;
        r3d_shader_loader_func bloomUp;
        r3d_shader_loader_func cubefaceDown;
        r3d_shader_loader_func cubemapFromEquirectangular;
        r3d_shader_loader_func cubemapIrradiance;
        r3d_shader_loader_func cubemapPrefilter;
        r3d_shader_loader_func cubemapSkybox;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_loader_func geometry;
        r3d_shader_loader_func forward;
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
        r3d_shader_loader_func bloom;
        r3d_shader_loader_func fog;
        r3d_shader_loader_func dof;
        r3d_shader_loader_func output;
        r3d_shader_loader_func fxaa;
    } post;

} R3D_MOD_SHADER_LOADER = {

    .prepare = {
        .atrousWavelet = r3d_shader_load_prepare_atrous_wavelet,
        .blurDown = r3d_shader_load_prepare_blur_down,
        .blurUp = r3d_shader_load_prepare_blur_up,
        .ssao = r3d_shader_load_prepare_ssao,
        .ssil = r3d_shader_load_prepare_ssil,
        .ssr = r3d_shader_load_prepare_ssr,
        .bloomDown = r3d_shader_load_prepare_bloom_down,
        .bloomUp = r3d_shader_load_prepare_bloom_up,
        .cubefaceDown = r3d_shader_load_prepare_cubeface_down,
        .cubemapFromEquirectangular = r3d_shader_load_prepare_cubemap_from_equirectangular,
        .cubemapIrradiance = r3d_shader_load_prepare_cubemap_irradiance,
        .cubemapPrefilter = r3d_shader_load_prepare_cubemap_prefilter,
        .cubemapSkybox = r3d_shader_load_prepare_cubemap_skybox,
    },

    .scene = {
        .geometry = r3d_shader_load_scene_geometry,
        .forward = r3d_shader_load_scene_forward,
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
        .compose = r3d_shader_load_deferred_compose,
    },

    .post = {
        .bloom = r3d_shader_load_post_bloom,
        .fog = r3d_shader_load_post_fog,
        .output = r3d_shader_load_post_output,
        .fxaa = r3d_shader_load_post_fxaa,
        .dof = r3d_shader_load_post_dof,
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
 * Iterates through all samplers to unbind any textures that were bound.
 * Must be called at the end of each frame to avoid leaving a dirty state to raylib.
 */
void r3d_shader_unbind_samplers(void);

/*
 * Upload and bind the specified uniform block with the provided data.
 */
void r3d_shader_set_uniform_block(r3d_shader_block_t block, const void* data);

#endif // R3D_MODULE_SHADER_H
