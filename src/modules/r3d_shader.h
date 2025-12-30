/* r3d_shader.h -- Internal R3D shader module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_MODULE_SHADER_H
#define R3D_MODULE_SHADER_H

#include <raylib.h>
#include <glad.h>

// ========================================
// SHADER MANAGEMENT MACROS
// ========================================

#define R3D_SHADER_USE(shader_name) do {                                                            \
    if (R3D_MOD_SHADER.shader_name.id == 0) {                                                       \
        R3D_MOD_SHADER_LOADER.shader_name();                                                        \
    }                                                                                               \
    glUseProgram(R3D_MOD_SHADER.shader_name.id);                                                    \
} while(0)

#define R3D_SHADER_SLOT_SAMPLER_1D(shader_name, uniform)                                            \
    R3D_MOD_SHADER.shader_name.uniform.slot1D                                                       \

#define R3D_SHADER_SLOT_SAMPLER_2D(shader_name, uniform)                                            \
    R3D_MOD_SHADER.shader_name.uniform.slot2D                                                       \

#define R3D_SHADER_SLOT_SAMPLER_CUBE(shader_name, uniform)                                          \
    R3D_MOD_SHADER.shader_name.uniform.slotCube                                                     \

#define R3D_SHADER_BIND_SAMPLER_1D(shader_name, uniform, texId) do {                                \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slot1D);                       \
    glBindTexture(GL_TEXTURE_1D, (texId));                                                          \
} while(0)

#define R3D_SHADER_BIND_SAMPLER_2D(shader_name, uniform, texId) do {                                \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slot2D);                       \
    glBindTexture(GL_TEXTURE_2D, (texId));                                                          \
} while(0)

#define R3D_SHADER_BIND_SAMPLER_CUBE(shader_name, uniform, texId) do {                              \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slotCube);                     \
    glBindTexture(GL_TEXTURE_CUBE_MAP, (texId));                                                    \
} while(0)

#define R3D_SHADER_UNBIND_SAMPLER_1D(shader_name, uniform) do {                                     \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slot1D);                       \
    glBindTexture(GL_TEXTURE_1D, 0);                                                                \
} while(0)

#define R3D_SHADER_UNBIND_SAMPLER_2D(shader_name, uniform) do {                                     \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slot2D);                       \
    glBindTexture(GL_TEXTURE_2D, 0);                                                                \
} while(0)

#define R3D_SHADER_UNBIND_SAMPLER_CUBE(shader_name, uniform) do {                                   \
    glActiveTexture(GL_TEXTURE0 + R3D_MOD_SHADER.shader_name.uniform.slotCube);                     \
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);                                                          \
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
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
        glUniform2fv(                                                                               \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D_MOD_SHADER.shader_name.uniform.val)                                    \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_VEC3(shader_name, uniform, ...) do {                                         \
    const Vector3 tmp = (__VA_ARGS__);                                                              \
    if (!Vector3Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {                              \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
        glUniform3fv(                                                                               \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D_MOD_SHADER.shader_name.uniform.val)                                    \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_VEC4(shader_name, uniform, ...) do {                                         \
    const Vector4 tmp = (__VA_ARGS__);                                                              \
    if (!Vector4Equals(R3D_MOD_SHADER.shader_name.uniform.val, tmp)) {                              \
        R3D_MOD_SHADER.shader_name.uniform.val = tmp;                                               \
        glUniform4fv(                                                                               \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D_MOD_SHADER.shader_name.uniform.val)                                    \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_COL3(shader_name, uniform, value) do {                                       \
    const Vector3 v = {                                                                             \
        (value).r / 255.0f,                                                                         \
        (value).g / 255.0f,                                                                         \
        (value).b / 255.0f                                                                          \
    };                                                                                              \
    if (!Vector3Equals(R3D_MOD_SHADER.shader_name.uniform.val, v)) {                                \
        R3D_MOD_SHADER.shader_name.uniform.val = v;                                                 \
        glUniform3fv(                                                                               \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D_MOD_SHADER.shader_name.uniform.val)                                    \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_COL4(shader_name, uniform, value) do {                                       \
    const Vector4 v = {                                                                             \
        (value).r / 255.0f,                                                                         \
        (value).g / 255.0f,                                                                         \
        (value).b / 255.0f,                                                                         \
        (value).a / 255.0f                                                                          \
    };                                                                                              \
    if (!Vector4Equals(R3D_MOD_SHADER.shader_name.uniform.val, v)) {                                \
        R3D_MOD_SHADER.shader_name.uniform.val = v;                                                 \
        glUniform4fv(                                                                               \
            R3D_MOD_SHADER.shader_name.uniform.loc,                                                 \
            1, (float*)(&R3D_MOD_SHADER.shader_name.uniform.val)                                    \
        );                                                                                          \
    }                                                                                               \
} while(0)

#define R3D_SHADER_SET_MAT4(shader_name, uniform, value) do {                                       \
    glUniformMatrix4fv(R3D_MOD_SHADER.shader_name.uniform.loc, 1, GL_TRUE, (float*)(&(value)));     \
} while(0)

#define R3D_SHADER_SET_MAT4_V(shader_name, uniform, array, count) do {                              \
    glUniformMatrix4fv(R3D_MOD_SHADER.shader_name.uniform.loc, (count), GL_TRUE, (float*)(array));  \
} while(0)

// ========================================
// MODULE CONSTANTS
// ========================================

#define R3D_SHADER_FORWARD_NUM_LIGHTS   8
#define R3D_SHADER_UBO_VIEW_SLOT        0

// ========================================
// UNIFORMS TYPES
// ========================================

typedef struct { int slot1D; int loc; } r3d_shader_uniform_sampler1D_t;
typedef struct { int slot2D; int loc; } r3d_shader_uniform_sampler2D_t;
typedef struct { int slotCube; int loc; } r3d_shader_uniform_samplerCube_t;

typedef struct { int val; int loc; } r3d_shader_uniform_int_t;
typedef struct { float val; int loc; } r3d_shader_uniform_float_t;
typedef struct { Vector2 val; int loc; } r3d_shader_uniform_vec2_t;
typedef struct { Vector3 val; int loc; } r3d_shader_uniform_vec3_t;
typedef struct { Vector4 val; int loc; } r3d_shader_uniform_vec4_t;

typedef struct { int loc; } r3d_shader_uniform_mat4_t;

// ========================================
// SHADER STRUCTURES DECLARATIONS
// ========================================

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexSource;
    r3d_shader_uniform_int_t uMipSource;
} r3d_shader_prepare_blur_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexSource;
    r3d_shader_uniform_int_t uMipSource;
} r3d_shader_prepare_blur_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_int_t uSampleCount;
    r3d_shader_uniform_float_t uRadius;
    r3d_shader_uniform_float_t uBias;
    r3d_shader_uniform_float_t uIntensity;
    r3d_shader_uniform_float_t uPower;
} r3d_shader_prepare_ssao_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexSource;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_vec2_t uDirection;
} r3d_shader_prepare_ssao_blur_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexLight;
    r3d_shader_uniform_sampler2D_t uTexPrevSSIL;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_float_t uSampleCount;
    r3d_shader_uniform_float_t uSampleRadius;
    r3d_shader_uniform_float_t uSliceCount;
    r3d_shader_uniform_float_t uHitThickness;
    r3d_shader_uniform_float_t uAoPower;
    r3d_shader_uniform_float_t uBounce;
    r3d_shader_uniform_float_t uEnergy;
} r3d_shader_prepare_ssil_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexCurrent;
    r3d_shader_uniform_sampler2D_t uTexHistory;
    r3d_shader_uniform_float_t uConvergence;
} r3d_shader_prepare_ssil_convergence_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_sampler2D_t uTexDepth;
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
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uTexelSize;
    r3d_shader_uniform_vec4_t uPrefilter;
    r3d_shader_uniform_int_t uDstLevel;
} r3d_shader_prepare_bloom_down_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uFilterRadius;
    r3d_shader_uniform_float_t uSrcLevel;
} r3d_shader_prepare_bloom_up_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_sampler2D_t uTexEquirectangular;
} r3d_shader_prepare_cubemap_from_equirectangular_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_samplerCube_t uCubemap;
} r3d_shader_prepare_cubemap_irradiance_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_samplerCube_t uCubemap;
    r3d_shader_uniform_float_t uResolution;
    r3d_shader_uniform_float_t uRoughness;
} r3d_shader_prepare_cubemap_prefilter_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_vec4_t uAlbedoColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_vec3_t uEmissionColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexEmission;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
} r3d_shader_scene_geometry_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatVP;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_float_t uAlpha;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_float_t uAlphaCutoff;
} r3d_shader_scene_depth_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatVP;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_float_t uAlpha;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_float_t uFar;
} r3d_shader_scene_depth_cube_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatLightVP[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_vec4_t uAlbedoColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_int_t uSkinning;
    r3d_shader_uniform_int_t uBillboard;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexEmission;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_samplerCube_t uShadowMapCube[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_sampler2D_t uShadowMap2D[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_float_t uNormalScale;
    r3d_shader_uniform_float_t uOcclusion;
    r3d_shader_uniform_float_t uRoughness;
    r3d_shader_uniform_float_t uMetalness;
    r3d_shader_uniform_vec3_t uAmbientColor;
    r3d_shader_uniform_vec3_t uEmissionColor;
    r3d_shader_uniform_samplerCube_t uCubeIrradiance;
    r3d_shader_uniform_samplerCube_t uCubePrefilter;
    r3d_shader_uniform_sampler2D_t uTexBrdfLut;
    r3d_shader_uniform_vec4_t uQuatSkybox;
    r3d_shader_uniform_int_t uHasSkybox;
    r3d_shader_uniform_float_t uAmbientEnergy;
    r3d_shader_uniform_float_t uReflectEnergy;
    struct {
        r3d_shader_uniform_vec3_t color;
        r3d_shader_uniform_vec3_t position;
        r3d_shader_uniform_vec3_t direction;
        r3d_shader_uniform_float_t specular;
        r3d_shader_uniform_float_t energy;
        r3d_shader_uniform_float_t range;
        r3d_shader_uniform_float_t near;
        r3d_shader_uniform_float_t far;
        r3d_shader_uniform_float_t attenuation;
        r3d_shader_uniform_float_t innerCutOff;
        r3d_shader_uniform_float_t outerCutOff;
        r3d_shader_uniform_float_t shadowSoftness;
        r3d_shader_uniform_float_t shadowTexelSize;
        r3d_shader_uniform_float_t shadowDepthBias;
        r3d_shader_uniform_float_t shadowSlopeBias;
        r3d_shader_uniform_int_t type;
        r3d_shader_uniform_int_t enabled;
        r3d_shader_uniform_int_t shadow;
    } uLights[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_vec3_t uViewPosition;
} r3d_shader_scene_forward_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_vec4_t uAlbedoColor;
    r3d_shader_uniform_float_t uEmissionEnergy;
    r3d_shader_uniform_vec3_t uEmissionColor;
    r3d_shader_uniform_vec2_t uTexCoordOffset;
    r3d_shader_uniform_vec2_t uTexCoordScale;
    r3d_shader_uniform_int_t uInstancing;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexEmission;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_sampler2D_t uTexDepth;
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
    r3d_shader_uniform_float_t uSkyEnergy;
    r3d_shader_uniform_samplerCube_t uCubeSky;
} r3d_shader_scene_skybox_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_sampler2D_t uTexSSIL;
    r3d_shader_uniform_sampler2D_t uTexSSR;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_samplerCube_t uCubeIrradiance;
    r3d_shader_uniform_samplerCube_t uCubePrefilter;
    r3d_shader_uniform_sampler2D_t uTexBrdfLut;
    r3d_shader_uniform_float_t uAmbientEnergy;
    r3d_shader_uniform_float_t uReflectEnergy;
    r3d_shader_uniform_float_t uMipCountSSR;
    r3d_shader_uniform_vec4_t uQuatSkybox;
} r3d_shader_deferred_ambient_ibl_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_sampler2D_t uTexSSIL;
    r3d_shader_uniform_sampler2D_t uTexSSR;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_vec3_t uAmbientColor;
    r3d_shader_uniform_float_t uAmbientEnergy;
    r3d_shader_uniform_float_t uMipCountSSR;
} r3d_shader_deferred_ambient_t;

typedef struct {
    unsigned int id;
    struct {
        r3d_shader_uniform_mat4_t matVP;
        r3d_shader_uniform_sampler2D_t shadowMap;
        r3d_shader_uniform_samplerCube_t shadowCubemap;
        r3d_shader_uniform_vec3_t color;
        r3d_shader_uniform_vec3_t position;
        r3d_shader_uniform_vec3_t direction;
        r3d_shader_uniform_float_t specular;
        r3d_shader_uniform_float_t energy;
        r3d_shader_uniform_float_t range;
        r3d_shader_uniform_float_t near;
        r3d_shader_uniform_float_t far;
        r3d_shader_uniform_float_t attenuation;
        r3d_shader_uniform_float_t innerCutOff;
        r3d_shader_uniform_float_t outerCutOff;
        r3d_shader_uniform_float_t shadowSoftness;
        r3d_shader_uniform_float_t shadowTexelSize;
        r3d_shader_uniform_float_t shadowDepthBias;
        r3d_shader_uniform_float_t shadowSlopeBias;
        r3d_shader_uniform_int_t type;
        r3d_shader_uniform_int_t shadow;
    } uLight;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_float_t uSSAOLightAffect;
} r3d_shader_deferred_lighting_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexDiffuse;
    r3d_shader_uniform_sampler2D_t uTexSpecular;
} r3d_shader_deferred_compose_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexBloomBlur;
    r3d_shader_uniform_int_t uBloomMode;
    r3d_shader_uniform_float_t uBloomIntensity;
} r3d_shader_post_bloom_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_int_t uFogMode;
    r3d_shader_uniform_vec3_t uFogColor;
    r3d_shader_uniform_float_t uFogStart;
    r3d_shader_uniform_float_t uFogEnd;
    r3d_shader_uniform_float_t uFogDensity;
    r3d_shader_uniform_float_t uSkyAffect;
} r3d_shader_post_fog_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_float_t uFocusPoint;
    r3d_shader_uniform_float_t uFocusScale;
    r3d_shader_uniform_float_t uMaxBlurSize;
    r3d_shader_uniform_int_t uDebugMode;
} r3d_shader_post_dof_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_float_t uTonemapExposure;
    r3d_shader_uniform_float_t uTonemapWhite;
    r3d_shader_uniform_int_t uTonemapMode;
    r3d_shader_uniform_float_t uBrightness;
    r3d_shader_uniform_float_t uContrast;
    r3d_shader_uniform_float_t uSaturation;
} r3d_shader_post_output_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uTexelSize;
} r3d_shader_post_fxaa_t;

// ========================================
// MODULE STATE
// ========================================

extern struct r3d_shader {

    // Prepare shaders
    struct {
        r3d_shader_prepare_blur_down_t blurDown;
        r3d_shader_prepare_blur_up_t blurUp;
        r3d_shader_prepare_ssao_t ssao;
        r3d_shader_prepare_ssao_blur_t ssaoBlur;
        r3d_shader_prepare_ssil_t ssil;
        r3d_shader_prepare_ssil_convergence_t ssilConvergence;
        r3d_shader_prepare_ssr_t ssr;
        r3d_shader_prepare_bloom_down_t bloomDown;
        r3d_shader_prepare_bloom_up_t bloomUp;
        r3d_shader_prepare_cubemap_from_equirectangular_t cubemapFromEquirectangular;
        r3d_shader_prepare_cubemap_irradiance_t cubemapIrradiance;
        r3d_shader_prepare_cubemap_prefilter_t cubemapPrefilter;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_scene_geometry_t geometry;
        r3d_shader_scene_forward_t forward;
        r3d_shader_scene_background_t background;
        r3d_shader_scene_skybox_t skybox;
        r3d_shader_scene_depth_t depth;
        r3d_shader_scene_depth_cube_t depthCube;
        r3d_shader_scene_decal_t decal;
    } scene;

    // Deferred shaders
    struct {
        r3d_shader_deferred_ambient_ibl_t ambientIbl;
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

void r3d_shader_load_prepare_blur_down(void);
void r3d_shader_load_prepare_blur_up(void);
void r3d_shader_load_prepare_ssao(void);
void r3d_shader_load_prepare_ssao_blur(void);
void r3d_shader_load_prepare_ssil(void);
void r3d_shader_load_prepare_ssil_convergence(void);
void r3d_shader_load_prepare_ssr(void);
void r3d_shader_load_prepare_bloom_down(void);
void r3d_shader_load_prepare_bloom_up(void);
void r3d_shader_load_prepare_cubemap_from_equirectangular(void);
void r3d_shader_load_prepare_cubemap_irradiance(void);
void r3d_shader_load_prepare_cubemap_prefilter(void);
void r3d_shader_load_scene_geometry(void);
void r3d_shader_load_scene_forward(void);
void r3d_shader_load_scene_background(void);
void r3d_shader_load_scene_skybox(void);
void r3d_shader_load_scene_depth(void);
void r3d_shader_load_scene_depth_cube(void);
void r3d_shader_load_scene_decal(void);
void r3d_shader_load_deferred_ambient_ibl(void);
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
        r3d_shader_loader_func blurDown;
        r3d_shader_loader_func blurUp;
        r3d_shader_loader_func ssao;
        r3d_shader_loader_func ssaoBlur;
        r3d_shader_loader_func ssil;
        r3d_shader_loader_func ssilConvergence;
        r3d_shader_loader_func ssr;
        r3d_shader_loader_func bloomDown;
        r3d_shader_loader_func bloomUp;
        r3d_shader_loader_func cubemapFromEquirectangular;
        r3d_shader_loader_func cubemapIrradiance;
        r3d_shader_loader_func cubemapPrefilter;
    } prepare;

    // Scene shaders
    struct {
        r3d_shader_loader_func geometry;
        r3d_shader_loader_func forward;
        r3d_shader_loader_func background;
        r3d_shader_loader_func skybox;
        r3d_shader_loader_func depth;
        r3d_shader_loader_func depthCube;
        r3d_shader_loader_func decal;
    } scene;

    // Deferred shaders
    struct {
        r3d_shader_loader_func ambientIbl;
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
        .blurDown = r3d_shader_load_prepare_blur_down,
        .blurUp = r3d_shader_load_prepare_blur_up,
        .ssao = r3d_shader_load_prepare_ssao,
        .ssaoBlur = r3d_shader_load_prepare_ssao_blur,
        .ssil = r3d_shader_load_prepare_ssil,
        .ssilConvergence = r3d_shader_load_prepare_ssil_convergence,
        .ssr = r3d_shader_load_prepare_ssr,
        .bloomDown = r3d_shader_load_prepare_bloom_down,
        .bloomUp = r3d_shader_load_prepare_bloom_up,
        .cubemapFromEquirectangular = r3d_shader_load_prepare_cubemap_from_equirectangular,
        .cubemapIrradiance = r3d_shader_load_prepare_cubemap_irradiance,
        .cubemapPrefilter = r3d_shader_load_prepare_cubemap_prefilter,
    },

    .scene = {
        .geometry = r3d_shader_load_scene_geometry,
        .forward = r3d_shader_load_scene_forward,
        .background = r3d_shader_load_scene_background,
        .skybox = r3d_shader_load_scene_skybox,
        .depth = r3d_shader_load_scene_depth,
        .depthCube = r3d_shader_load_scene_depth_cube,
        .decal = r3d_shader_load_scene_decal,
    },

    .deferred = {
        .ambientIbl = r3d_shader_load_deferred_ambient_ibl,
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

#endif // R3D_MODULE_SHADER_H
