/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#ifndef R3D_EMBEDDED_SHADERS_H
#define R3D_EMBEDDED_SHADERS_H

#include <raylib.h>

/* === Shader defines === */

#define R3D_SHADER_FORWARD_NUM_LIGHTS 8

/* === Uniform types === */

typedef struct { int slot1D; int loc; } r3d_shader_uniform_sampler1D_t;
typedef struct { int slot2D; int loc; } r3d_shader_uniform_sampler2D_t;
typedef struct { int slotCube; int loc; } r3d_shader_uniform_samplerCube_t;

typedef struct { int val; int loc; } r3d_shader_uniform_int_t;
typedef struct { float val; int loc; } r3d_shader_uniform_float_t;
typedef struct { Vector2 val; int loc; } r3d_shader_uniform_vec2_t;
typedef struct { Vector3 val; int loc; } r3d_shader_uniform_vec3_t;
typedef struct { Vector4 val; int loc; } r3d_shader_uniform_vec4_t;

typedef struct { int loc; } r3d_shader_uniform_mat4_t;

/* === Shader struct definitions === */

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uTexelDir;
} r3d_shader_generate_gaussian_blur_dual_pass_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uTexelSize;
    r3d_shader_uniform_int_t uMipLevel;
    r3d_shader_uniform_vec4_t uPrefilter;
} r3d_shader_generate_downsampling_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uFilterRadius;
} r3d_shader_generate_upsampling_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_sampler2D_t uTexEquirectangular;
} r3d_shader_generate_cubemap_from_equirectangular_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_samplerCube_t uCubemap;
} r3d_shader_generate_irradiance_convolution_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_samplerCube_t uCubemap;
    r3d_shader_uniform_float_t uResolution;
    r3d_shader_uniform_float_t uRoughness;
} r3d_shader_generate_prefilter_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatVP;
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
} r3d_shader_raster_geometry_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_vec4_t uRotation;
    r3d_shader_uniform_float_t uSkyIntensity;
    r3d_shader_uniform_samplerCube_t uCubeSky;
} r3d_shader_raster_skybox_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_mat4_t uMatMVP;
} r3d_shader_raster_depth_volume_t;

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
} r3d_shader_raster_depth_t;

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
} r3d_shader_raster_depth_cube_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler1D_t uTexBoneMatrices;
    r3d_shader_uniform_mat4_t uMatLightVP[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatNormal;
    r3d_shader_uniform_mat4_t uMatModel;
    r3d_shader_uniform_mat4_t uMatVP;
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
    r3d_shader_uniform_vec3_t uAmbientLight;
    r3d_shader_uniform_vec3_t uEmissionColor;
    r3d_shader_uniform_samplerCube_t uCubeIrradiance;
    r3d_shader_uniform_samplerCube_t uCubePrefilter;
    r3d_shader_uniform_sampler2D_t uTexBrdfLut;
    r3d_shader_uniform_vec4_t uQuatSkybox;
    r3d_shader_uniform_int_t uHasSkybox;
    r3d_shader_uniform_float_t uSkyboxAmbientIntensity;
    r3d_shader_uniform_float_t uSkyboxReflectIntensity;
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
        r3d_shader_uniform_float_t shadowMapTxlSz;
        r3d_shader_uniform_float_t shadowDepthBias;
        r3d_shader_uniform_float_t shadowSlopeBias;
        r3d_shader_uniform_int_t type;
        r3d_shader_uniform_int_t enabled;
        r3d_shader_uniform_int_t shadow;
    } uLights[R3D_SHADER_FORWARD_NUM_LIGHTS];
    r3d_shader_uniform_float_t uAlphaCutoff;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_float_t uFar;
} r3d_shader_raster_forward_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler1D_t uTexKernel;
    r3d_shader_uniform_sampler2D_t uTexNoise;
    r3d_shader_uniform_mat4_t uMatInvProj;
    r3d_shader_uniform_mat4_t uMatProj;
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_float_t uRadius;
    r3d_shader_uniform_float_t uBias;
    r3d_shader_uniform_float_t uIntensity;
} r3d_shader_screen_ssao_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_samplerCube_t uCubeIrradiance;
    r3d_shader_uniform_samplerCube_t uCubePrefilter;
    r3d_shader_uniform_sampler2D_t uTexBrdfLut;
    r3d_shader_uniform_vec4_t uQuatSkybox;
    r3d_shader_uniform_float_t uSkyboxAmbientIntensity;
    r3d_shader_uniform_float_t uSkyboxReflectIntensity;
    r3d_shader_uniform_float_t uSSAOPower;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_mat4_t uMatInvProj;
    r3d_shader_uniform_mat4_t uMatInvView;
} r3d_shader_screen_ambient_ibl_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_float_t uSSAOPower;
    r3d_shader_uniform_vec3_t uAmbientLight;
} r3d_shader_screen_ambient_t;

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
        r3d_shader_uniform_float_t shadowMapTxlSz;
        r3d_shader_uniform_float_t shadowDepthBias;
        r3d_shader_uniform_float_t shadowSlopeBias;
        r3d_shader_uniform_int_t type;
        r3d_shader_uniform_int_t shadow;
    } uLight;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexNormal;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_sampler2D_t uTexORM;
    r3d_shader_uniform_vec3_t uViewPosition;
    r3d_shader_uniform_mat4_t uMatInvProj;
    r3d_shader_uniform_mat4_t uMatInvView;
} r3d_shader_screen_lighting_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexAlbedo;
    r3d_shader_uniform_sampler2D_t uTexEmission;
    r3d_shader_uniform_sampler2D_t uTexDiffuse;
    r3d_shader_uniform_sampler2D_t uTexSpecular;
    r3d_shader_uniform_sampler2D_t uTexSSAO;
    r3d_shader_uniform_float_t uSSAOPower;
    r3d_shader_uniform_float_t uSSAOLightAffect;
} r3d_shader_screen_scene_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexBloomBlur;
    r3d_shader_uniform_int_t uBloomMode;
    r3d_shader_uniform_float_t uBloomIntensity;
} r3d_shader_screen_bloom_t;

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
    r3d_shader_uniform_mat4_t uMatView;
    r3d_shader_uniform_mat4_t uMatInvProj;
    r3d_shader_uniform_mat4_t uMatInvView;
    r3d_shader_uniform_mat4_t uMatViewProj;
    r3d_shader_uniform_vec3_t uViewPosition;
} r3d_shader_screen_ssr_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_float_t uNear;
    r3d_shader_uniform_float_t uFar;
    r3d_shader_uniform_int_t uFogMode;
    r3d_shader_uniform_vec3_t uFogColor;
    r3d_shader_uniform_float_t uFogStart;
    r3d_shader_uniform_float_t uFogEnd;
    r3d_shader_uniform_float_t uFogDensity;
    r3d_shader_uniform_float_t uSkyAffect;
} r3d_shader_screen_fog_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_sampler2D_t uTexDepth;
    r3d_shader_uniform_vec2_t uTexelSize;
    r3d_shader_uniform_float_t uNear;
    r3d_shader_uniform_float_t uFar;
    r3d_shader_uniform_float_t uFocusPoint;
    r3d_shader_uniform_float_t uFocusScale;
    r3d_shader_uniform_float_t uMaxBlurSize;
    r3d_shader_uniform_int_t   uDebugMode;
} r3d_shader_screen_dof_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexColor;
    r3d_shader_uniform_float_t uTonemapExposure;
    r3d_shader_uniform_float_t uTonemapWhite;
    r3d_shader_uniform_float_t uBrightness;
    r3d_shader_uniform_float_t uContrast;
    r3d_shader_uniform_float_t uSaturation;
} r3d_shader_screen_output_t;

typedef struct {
    unsigned int id;
    r3d_shader_uniform_sampler2D_t uTexture;
    r3d_shader_uniform_vec2_t uTexelSize;
} r3d_shader_screen_fxaa_t;

#endif // R3D_EMBEDDED_SHADERS_H
