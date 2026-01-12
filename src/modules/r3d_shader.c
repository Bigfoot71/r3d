/* r3d_shader.c -- Internal R3D shader module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_shader.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <rlgl.h>

#include "../common/r3d_helper.h"

// ========================================
// SHADER CODE INCLUDES
// ========================================

#include <shaders/color.frag.h>
#include <shaders/screen.vert.h>
#include <shaders/cubemap.vert.h>
#include <shaders/buffer_down.frag.h>
#include <shaders/atrous_wavelet.frag.h>
#include <shaders/bicubic_up.frag.h>
#include <shaders/lanczos_up.frag.h>
#include <shaders/blur_down.frag.h>
#include <shaders/blur_up.frag.h>
#include <shaders/ssao.frag.h>
#include <shaders/ssil.frag.h>
#include <shaders/ssr.frag.h>
#include <shaders/bloom_down.frag.h>
#include <shaders/bloom_up.frag.h>
#include <shaders/cubemap_from_equirectangular.frag.h>
#include <shaders/cubemap_irradiance.frag.h>
#include <shaders/cubemap_prefilter.frag.h>
#include <shaders/cubemap_skybox.frag.h>
#include <shaders/scene.vert.h>
#include <shaders/geometry.frag.h>
#include <shaders/forward.frag.h>
#include <shaders/depth.frag.h>
#include <shaders/depth_cube.frag.h>
#include <shaders/decal.frag.h>
#include <shaders/skybox.vert.h>
#include <shaders/skybox.frag.h>
#include <shaders/ambient.frag.h>
#include <shaders/lighting.frag.h>
#include <shaders/compose.frag.h>
#include <shaders/bloom.frag.h>
#include <shaders/fog.frag.h>
#include <shaders/dof.frag.h>
#include <shaders/output.frag.h>
#include <shaders/fxaa.frag.h>
#include <shaders/visualizer.frag.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_shader R3D_MOD_SHADER;

// ========================================
// INTERNAL MACROS
// ========================================

#define LOAD_SHADER(shader_name, vsCode, fsCode) do {                           \
    R3D_MOD_SHADER.shader_name.id = load_shader(vsCode, fsCode);                \
    if (R3D_MOD_SHADER.shader_name.id == 0) {                                   \
        TraceLog(LOG_ERROR, "R3D: Failed to load shader '" #shader_name "'");   \
        assert(false);                                                          \
        return;                                                                 \
    }                                                                           \
} while(0)

#define USE_SHADER(shader_name) do {                                            \
    glUseProgram(R3D_MOD_SHADER.shader_name.id);                                \
} while(0)                                                                      \

#define GET_LOCATION(shader_name, uniform) do {                                 \
    R3D_MOD_SHADER.shader_name.uniform.loc = glGetUniformLocation(              \
        R3D_MOD_SHADER.shader_name.id, #uniform                                 \
    );                                                                          \
} while(0)

#define SET_SAMPLER(shader_name, uniform, value) do {                           \
    GLint loc = glGetUniformLocation(R3D_MOD_SHADER.shader_name.id, #uniform);  \
    glUniform1i(loc, (int)(value));                                             \
    R3D_MOD_SHADER.shader_name.uniform.slot = (int)(value);                     \
} while(0)

#define SET_UNIFORM_BUFFER(shader_name, uniform, slot) do {                     \
    GLuint idx = glGetUniformBlockIndex(R3D_MOD_SHADER.shader_name.id, #uniform);\
    glUniformBlockBinding(R3D_MOD_SHADER.shader_name.id, idx, slot);            \
} while(0)                                                                      \

#define UNLOAD_SHADER(shader_name) do {                                         \
    if (R3D_MOD_SHADER.shader_name.id != 0) {                                   \
        glDeleteProgram(R3D_MOD_SHADER.shader_name.id);                         \
    }                                                                           \
} while(0)

// ========================================
// HELPER FUNCTIONS DECLARATIONS
// ========================================

static char* inject_defines_to_shader_code(const char* code, const char* defines[], int count);

// ========================================
// SHADER COMPLING / LINKING FUNCTIONS
// ========================================

static GLuint compile_shader(const char* source, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader == 0) {
        TraceLog(LOG_ERROR, "R3D: Failed to create shader object");
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        const char* type_str = (shaderType == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        TraceLog(LOG_ERROR, "R3D: %s shader compilation failed: %s", type_str, infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_shader(GLuint vertShader, GLuint fragShader)
{
    GLuint program = glCreateProgram();
    if (program == 0) {
        TraceLog(LOG_ERROR, "R3D: Failed to create shader program");
        return 0;
    }

    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        TraceLog(LOG_ERROR, "R3D: Shader program linking failed: %s", infoLog);
        glDeleteProgram(program);
        return 0;
    }

    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);

    return program;
}

GLuint load_shader(const char* vsCode, const char* fsCode)
{
    GLuint vs = compile_shader(vsCode, GL_VERTEX_SHADER);
    if (vs == 0) return 0;

    GLuint fs = compile_shader(fsCode, GL_FRAGMENT_SHADER);
    if (fs == 0) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint program = link_shader(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

// ========================================
// SHADER LOADING FUNCTIONS
// ========================================

void r3d_shader_load_prepare_buffer_down(void)
{
    LOAD_SHADER(prepare.bufferDown, SCREEN_VERT, BUFFER_DOWN_FRAG);
    USE_SHADER(prepare.bufferDown);
    SET_SAMPLER(prepare.bufferDown, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(prepare.bufferDown, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(prepare.bufferDown, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);
    SET_SAMPLER(prepare.bufferDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(prepare.bufferDown, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
}

void r3d_shader_load_prepare_atrous_wavelet(void)
{
    LOAD_SHADER(prepare.atrousWavelet, SCREEN_VERT, ATROUS_WAVELET_FRAG);
    SET_UNIFORM_BUFFER(prepare.atrousWavelet, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    GET_LOCATION(prepare.atrousWavelet, uStepSize);
    USE_SHADER(prepare.atrousWavelet);
    SET_SAMPLER(prepare.atrousWavelet, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D);
    SET_SAMPLER(prepare.atrousWavelet, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(prepare.atrousWavelet, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_prepare_bicubic_up(void)
{
    LOAD_SHADER(prepare.bicubicUp, SCREEN_VERT, BICUBIC_UP_FRAG);
    GET_LOCATION(prepare.bicubicUp, uSourceTexel);
    USE_SHADER(prepare.bicubicUp);
    SET_SAMPLER(prepare.bicubicUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D);
}

void r3d_shader_load_prepare_lanczos_up(void)
{
    LOAD_SHADER(prepare.lanczosUp, SCREEN_VERT, LANCZOS_UP_FRAG);
    GET_LOCATION(prepare.lanczosUp, uSourceTexel);
    USE_SHADER(prepare.lanczosUp);
    SET_SAMPLER(prepare.lanczosUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D);
}

void r3d_shader_load_prepare_blur_down(void)
{
    LOAD_SHADER(prepare.blurDown, SCREEN_VERT, BLUR_DOWN_FRAG);
    GET_LOCATION(prepare.blurDown, uSourceLod);
    USE_SHADER(prepare.blurDown);
    SET_SAMPLER(prepare.blurDown, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D);
}

void r3d_shader_load_prepare_blur_up(void)
{
    LOAD_SHADER(prepare.blurUp, SCREEN_VERT, BLUR_UP_FRAG);
    GET_LOCATION(prepare.blurUp, uSourceLod);
    USE_SHADER(prepare.blurUp);
    SET_SAMPLER(prepare.blurUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D);
}

void r3d_shader_load_prepare_ssao(void)
{
    LOAD_SHADER(prepare.ssao, SCREEN_VERT, SSAO_FRAG);

    SET_UNIFORM_BUFFER(prepare.ssao, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(prepare.ssao, uSampleCount);
    GET_LOCATION(prepare.ssao, uRadius);
    GET_LOCATION(prepare.ssao, uBias);
    GET_LOCATION(prepare.ssao, uIntensity);
    GET_LOCATION(prepare.ssao, uPower);

    USE_SHADER(prepare.ssao);

    SET_SAMPLER(prepare.ssao, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(prepare.ssao, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_prepare_ssil(void)
{
    LOAD_SHADER(prepare.ssil, SCREEN_VERT, SSIL_FRAG);

    SET_UNIFORM_BUFFER(prepare.ssil, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(prepare.ssil, uSampleCount);
    GET_LOCATION(prepare.ssil, uSampleRadius);
    GET_LOCATION(prepare.ssil, uSliceCount);
    GET_LOCATION(prepare.ssil, uHitThickness);
    GET_LOCATION(prepare.ssil, uConvergence);
    GET_LOCATION(prepare.ssil, uAoPower);
    GET_LOCATION(prepare.ssil, uBounce);

    USE_SHADER(prepare.ssil);

    SET_SAMPLER(prepare.ssil, uLightingTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(prepare.ssil, uHistoryTex, R3D_SHADER_SAMPLER_BUFFER_SSIL);
    SET_SAMPLER(prepare.ssil, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(prepare.ssil, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_prepare_ssr(void)
{
    LOAD_SHADER(prepare.ssr, SCREEN_VERT, SSR_FRAG);

    SET_UNIFORM_BUFFER(prepare.ssr, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(prepare.ssr, uMaxRaySteps);
    GET_LOCATION(prepare.ssr, uBinarySearchSteps);
    GET_LOCATION(prepare.ssr, uRayMarchLength);
    GET_LOCATION(prepare.ssr, uDepthThickness);
    GET_LOCATION(prepare.ssr, uDepthTolerance);
    GET_LOCATION(prepare.ssr, uEdgeFadeStart);
    GET_LOCATION(prepare.ssr, uEdgeFadeEnd);
    GET_LOCATION(prepare.ssr, uAmbientColor);
    GET_LOCATION(prepare.ssr, uAmbientEnergy);

    USE_SHADER(prepare.ssr);

    SET_SAMPLER(prepare.ssr, uLightingTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(prepare.ssr, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(prepare.ssr, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(prepare.ssr, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);
    SET_SAMPLER(prepare.ssr, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_prepare_bloom_down(void)
{
    LOAD_SHADER(prepare.bloomDown, SCREEN_VERT, BLOOM_DOWN_FRAG);

    GET_LOCATION(prepare.bloomDown, uTexelSize);
    GET_LOCATION(prepare.bloomDown, uPrefilter);
    GET_LOCATION(prepare.bloomDown, uDstLevel);

    USE_SHADER(prepare.bloomDown);

    SET_SAMPLER(prepare.bloomDown, uTexture, R3D_SHADER_SAMPLER_BUFFER_BLOOM);
}

void r3d_shader_load_prepare_bloom_up(void)
{
    LOAD_SHADER(prepare.bloomUp, SCREEN_VERT, BLOOM_UP_FRAG);

    GET_LOCATION(prepare.bloomUp, uFilterRadius);
    GET_LOCATION(prepare.bloomUp, uSrcLevel);

    USE_SHADER(prepare.bloomUp);

    SET_SAMPLER(prepare.bloomUp, uTexture, R3D_SHADER_SAMPLER_BUFFER_BLOOM);
}

void r3d_shader_load_prepare_cubemap_from_equirectangular(void)
{
    LOAD_SHADER(prepare.cubemapFromEquirectangular, CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG);

    GET_LOCATION(prepare.cubemapFromEquirectangular, uMatProj);
    GET_LOCATION(prepare.cubemapFromEquirectangular, uMatView);

    USE_SHADER(prepare.cubemapFromEquirectangular);

    SET_SAMPLER(prepare.cubemapFromEquirectangular, uPanoramaTex, R3D_SHADER_SAMPLER_SOURCE_2D);
}

void r3d_shader_load_prepare_cubemap_irradiance(void)
{
    LOAD_SHADER(prepare.cubemapIrradiance, CUBEMAP_VERT, CUBEMAP_IRRADIANCE_FRAG);

    GET_LOCATION(prepare.cubemapIrradiance, uMatProj);
    GET_LOCATION(prepare.cubemapIrradiance, uMatView);

    USE_SHADER(prepare.cubemapIrradiance);

    SET_SAMPLER(prepare.cubemapIrradiance, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_CUBE);
}

void r3d_shader_load_prepare_cubemap_prefilter(void)
{
    LOAD_SHADER(prepare.cubemapPrefilter, CUBEMAP_VERT, CUBEMAP_PREFILTER_FRAG);

    GET_LOCATION(prepare.cubemapPrefilter, uMatProj);
    GET_LOCATION(prepare.cubemapPrefilter, uMatView);
    GET_LOCATION(prepare.cubemapPrefilter, uSourceNumLevels);
    GET_LOCATION(prepare.cubemapPrefilter, uSourceFaceSize);
    GET_LOCATION(prepare.cubemapPrefilter, uRoughness);

    USE_SHADER(prepare.cubemapPrefilter);

    SET_SAMPLER(prepare.cubemapPrefilter, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_CUBE);
}

void r3d_shader_load_prepare_cubemap_skybox(void)
{
    LOAD_SHADER(prepare.cubemapSkybox, CUBEMAP_VERT, CUBEMAP_SKYBOX_FRAG);

    GET_LOCATION(prepare.cubemapSkybox, uMatProj);
    GET_LOCATION(prepare.cubemapSkybox, uMatView);
    GET_LOCATION(prepare.cubemapSkybox, uSkyTopColor);
    GET_LOCATION(prepare.cubemapSkybox, uSkyHorizonColor);
    GET_LOCATION(prepare.cubemapSkybox, uSkyHorizonCurve);
    GET_LOCATION(prepare.cubemapSkybox, uSkyEnergy);
    GET_LOCATION(prepare.cubemapSkybox, uGroundBottomColor);
    GET_LOCATION(prepare.cubemapSkybox, uGroundHorizonColor);
    GET_LOCATION(prepare.cubemapSkybox, uGroundHorizonCurve);
    GET_LOCATION(prepare.cubemapSkybox, uGroundEnergy);
    GET_LOCATION(prepare.cubemapSkybox, uSunDirection);
    GET_LOCATION(prepare.cubemapSkybox, uSunColor);
    GET_LOCATION(prepare.cubemapSkybox, uSunSize);
    GET_LOCATION(prepare.cubemapSkybox, uSunCurve);
    GET_LOCATION(prepare.cubemapSkybox, uSunEnergy);

    USE_SHADER(prepare.cubemapSkybox);
}

void r3d_shader_load_scene_geometry(void)
{
    const char* VS_DEFINES[] = {"GEOMETRY"};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    LOAD_SHADER(scene.geometry, vsCode, GEOMETRY_FRAG);
    RL_FREE(vsCode);

    SET_UNIFORM_BUFFER(scene.geometry, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(scene.geometry, uMatNormal);
    GET_LOCATION(scene.geometry, uMatModel);
    GET_LOCATION(scene.geometry, uAlbedoColor);
    GET_LOCATION(scene.geometry, uEmissionEnergy);
    GET_LOCATION(scene.geometry, uEmissionColor);
    GET_LOCATION(scene.geometry, uTexCoordOffset);
    GET_LOCATION(scene.geometry, uTexCoordScale);
    GET_LOCATION(scene.geometry, uInstancing);
    GET_LOCATION(scene.geometry, uSkinning);
    GET_LOCATION(scene.geometry, uBillboard);
    GET_LOCATION(scene.geometry, uAlphaCutoff);
    GET_LOCATION(scene.geometry, uNormalScale);
    GET_LOCATION(scene.geometry, uOcclusion);
    GET_LOCATION(scene.geometry, uRoughness);
    GET_LOCATION(scene.geometry, uMetalness);

    USE_SHADER(scene.geometry);

    SET_SAMPLER(scene.geometry, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.geometry, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(scene.geometry, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(scene.geometry, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(scene.geometry, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
}

void r3d_shader_load_scene_forward(void)
{
    char defNumForwardLights[32] = {0}, defNumProbes[32] = {0};
    r3d_string_format(defNumForwardLights, sizeof(defNumForwardLights), "NUM_FORWARD_LIGHTS %i", R3D_SHADER_NUM_FORWARD_LIGHTS);
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_SHADER_NUM_PROBES);

    const char* VS_DEFINES[] = {"FORWARD", defNumForwardLights};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));

    const char* FS_DEFINES[] = {defNumForwardLights, defNumProbes};
    char* fsCode = inject_defines_to_shader_code(FORWARD_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    LOAD_SHADER(scene.forward, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(scene.forward, LightArrayBlock, R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT);
    SET_UNIFORM_BUFFER(scene.forward, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(scene.forward, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    GET_LOCATION(scene.forward, uMatNormal);
    GET_LOCATION(scene.forward, uMatModel);
    GET_LOCATION(scene.forward, uAlbedoColor);
    GET_LOCATION(scene.forward, uEmissionColor);
    GET_LOCATION(scene.forward, uEmissionEnergy);
    GET_LOCATION(scene.forward, uTexCoordOffset);
    GET_LOCATION(scene.forward, uTexCoordScale);
    GET_LOCATION(scene.forward, uInstancing);
    GET_LOCATION(scene.forward, uSkinning);
    GET_LOCATION(scene.forward, uBillboard);
    GET_LOCATION(scene.forward, uNormalScale);
    GET_LOCATION(scene.forward, uOcclusion);
    GET_LOCATION(scene.forward, uRoughness);
    GET_LOCATION(scene.forward, uMetalness);
    GET_LOCATION(scene.forward, uViewPosition);

    USE_SHADER(scene.forward);

    SET_SAMPLER(scene.forward, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.forward, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(scene.forward, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(scene.forward, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(scene.forward, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(scene.forward, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(scene.forward, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(scene.forward, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);
    SET_SAMPLER(scene.forward, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(scene.forward, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(scene.forward, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);
}

void r3d_shader_load_scene_background(void)
{
    LOAD_SHADER(scene.background, SCREEN_VERT, COLOR_FRAG);
    GET_LOCATION(scene.background, uColor);
}

void r3d_shader_load_scene_skybox(void)
{
    LOAD_SHADER(scene.skybox, SKYBOX_VERT, SKYBOX_FRAG);

    GET_LOCATION(scene.skybox, uRotation);
    GET_LOCATION(scene.skybox, uMatView);
    GET_LOCATION(scene.skybox, uMatProj);
    GET_LOCATION(scene.skybox, uSkyEnergy);
    GET_LOCATION(scene.skybox, uSkyLod);

    USE_SHADER(scene.skybox);

    SET_SAMPLER(scene.skybox, uSkyMap, R3D_SHADER_SAMPLER_SOURCE_CUBE);
}

void r3d_shader_load_scene_depth(void)
{
    const char* VS_DEFINES[] = {"DEPTH"};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    LOAD_SHADER(scene.depth, vsCode, DEPTH_FRAG);
    RL_FREE(vsCode);

    GET_LOCATION(scene.depth, uMatInvView);
    GET_LOCATION(scene.depth, uMatModel);
    GET_LOCATION(scene.depth, uMatViewProj);
    GET_LOCATION(scene.depth, uAlbedoColor);
    GET_LOCATION(scene.depth, uTexCoordOffset);
    GET_LOCATION(scene.depth, uTexCoordScale);
    GET_LOCATION(scene.depth, uInstancing);
    GET_LOCATION(scene.depth, uSkinning);
    GET_LOCATION(scene.depth, uBillboard);
    GET_LOCATION(scene.depth, uAlphaCutoff);

    USE_SHADER(scene.depth);

    SET_SAMPLER(scene.depth, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.depth, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
}

void r3d_shader_load_scene_depth_cube(void)
{
    const char* VS_DEFINES[] = {"DEPTH_CUBE"};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    LOAD_SHADER(scene.depthCube, vsCode, DEPTH_CUBE_FRAG);
    RL_FREE(vsCode);

    GET_LOCATION(scene.depthCube, uMatInvView);
    GET_LOCATION(scene.depthCube, uMatModel);
    GET_LOCATION(scene.depthCube, uMatViewProj);
    GET_LOCATION(scene.depthCube, uAlbedoColor);
    GET_LOCATION(scene.depthCube, uTexCoordOffset);
    GET_LOCATION(scene.depthCube, uTexCoordScale);
    GET_LOCATION(scene.depthCube, uInstancing);
    GET_LOCATION(scene.depthCube, uSkinning);
    GET_LOCATION(scene.depthCube, uBillboard);
    GET_LOCATION(scene.depthCube, uAlphaCutoff);
    GET_LOCATION(scene.depthCube, uViewPosition);
    GET_LOCATION(scene.depthCube, uFar);

    USE_SHADER(scene.depthCube);

    SET_SAMPLER(scene.depthCube, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.depthCube, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
}

void r3d_shader_load_scene_probe(void)
{
    char defNumForwardLights[32] = {0}, defNumProbes[32] = {0};
    r3d_string_format(defNumForwardLights, sizeof(defNumForwardLights), "NUM_FORWARD_LIGHTS %i", R3D_SHADER_NUM_FORWARD_LIGHTS);
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_SHADER_NUM_PROBES);

    const char* VS_DEFINES[] = {"PROBE", defNumForwardLights};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));

    const char* FS_DEFINES[] = {"PROBE", defNumForwardLights, defNumProbes};
    char* fsCode = inject_defines_to_shader_code(FORWARD_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    LOAD_SHADER(scene.probe, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(scene.probe, LightArrayBlock, R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT);
    SET_UNIFORM_BUFFER(scene.probe, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(scene.probe, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    GET_LOCATION(scene.probe, uMatInvView);
    GET_LOCATION(scene.probe, uMatNormal);
    GET_LOCATION(scene.probe, uMatModel);
    GET_LOCATION(scene.probe, uMatViewProj);
    GET_LOCATION(scene.probe, uAlbedoColor);
    GET_LOCATION(scene.probe, uEmissionColor);
    GET_LOCATION(scene.probe, uEmissionEnergy);
    GET_LOCATION(scene.probe, uTexCoordOffset);
    GET_LOCATION(scene.probe, uTexCoordScale);
    GET_LOCATION(scene.probe, uInstancing);
    GET_LOCATION(scene.probe, uSkinning);
    GET_LOCATION(scene.probe, uBillboard);
    GET_LOCATION(scene.probe, uNormalScale);
    GET_LOCATION(scene.probe, uOcclusion);
    GET_LOCATION(scene.probe, uRoughness);
    GET_LOCATION(scene.probe, uMetalness);
    GET_LOCATION(scene.probe, uViewPosition);
    GET_LOCATION(scene.probe, uProbeInterior);

    USE_SHADER(scene.probe);

    SET_SAMPLER(scene.probe, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.probe, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(scene.probe, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(scene.probe, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(scene.probe, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(scene.probe, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(scene.probe, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(scene.probe, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);
    SET_SAMPLER(scene.probe, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(scene.probe, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(scene.probe, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);
}

void r3d_shader_load_scene_decal(void)
{
    const char* VS_DEFINES[] = {"DECAL"};
    char* vsCode = inject_defines_to_shader_code(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    LOAD_SHADER(scene.decal, vsCode, DECAL_FRAG);
    RL_FREE(vsCode);

    SET_UNIFORM_BUFFER(scene.decal, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(scene.decal, uMatNormal);
    GET_LOCATION(scene.decal, uMatModel);
    GET_LOCATION(scene.decal, uAlbedoColor);
    GET_LOCATION(scene.decal, uEmissionEnergy);
    GET_LOCATION(scene.decal, uEmissionColor);
    GET_LOCATION(scene.decal, uTexCoordOffset);
    GET_LOCATION(scene.decal, uTexCoordScale);
    GET_LOCATION(scene.decal, uInstancing);
    GET_LOCATION(scene.decal, uSkinning);
    GET_LOCATION(scene.decal, uAlphaCutoff);
    GET_LOCATION(scene.decal, uNormalScale);
    GET_LOCATION(scene.decal, uOcclusion);
    GET_LOCATION(scene.decal, uRoughness);
    GET_LOCATION(scene.decal, uMetalness);
    GET_LOCATION(scene.decal, uNormalThreshold);
    GET_LOCATION(scene.decal, uFadeWidth);

    USE_SHADER(scene.decal);

    SET_SAMPLER(scene.decal, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(scene.decal, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(scene.decal, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(scene.decal, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(scene.decal, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(scene.decal, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(scene.decal, uNormTanTex, R3D_SHADER_SAMPLER_BUFFER_GEOM_NORM_TAN);
}

void r3d_shader_load_deferred_ambient(void)
{
    char defNumProbes[32] = {0};
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_SHADER_NUM_PROBES);

    const char* FS_DEFINES[] = {defNumProbes};
    char* fsCode = inject_defines_to_shader_code(AMBIENT_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));
    LOAD_SHADER(deferred.ambient, SCREEN_VERT, fsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(deferred.ambient, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(deferred.ambient, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    GET_LOCATION(deferred.ambient, uSsrNumLevels);
    GET_LOCATION(deferred.ambient, uSsilEnergy);

    USE_SHADER(deferred.ambient);

    SET_SAMPLER(deferred.ambient, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(deferred.ambient, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(deferred.ambient, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(deferred.ambient, uSsaoTex, R3D_SHADER_SAMPLER_BUFFER_SSAO);
    SET_SAMPLER(deferred.ambient, uSsilTex, R3D_SHADER_SAMPLER_BUFFER_SSIL);
    SET_SAMPLER(deferred.ambient, uSsrTex, R3D_SHADER_SAMPLER_BUFFER_SSR);
    SET_SAMPLER(deferred.ambient, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);

    SET_SAMPLER(deferred.ambient, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(deferred.ambient, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(deferred.ambient, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);
}

void r3d_shader_load_deferred_lighting(void)
{
    LOAD_SHADER(deferred.lighting, SCREEN_VERT, LIGHTING_FRAG);

    SET_UNIFORM_BUFFER(deferred.lighting, LightBlock, R3D_SHADER_BLOCK_LIGHT_SLOT);
    SET_UNIFORM_BUFFER(deferred.lighting, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    USE_SHADER(deferred.lighting);

    SET_SAMPLER(deferred.lighting, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(deferred.lighting, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(deferred.lighting, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(deferred.lighting, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);

    SET_SAMPLER(deferred.lighting, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(deferred.lighting, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(deferred.lighting, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);
}

void r3d_shader_load_deferred_compose(void)
{
    LOAD_SHADER(deferred.compose, SCREEN_VERT, COMPOSE_FRAG);

    USE_SHADER(deferred.compose);

    SET_SAMPLER(deferred.compose, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(deferred.compose, uSpecularTex, R3D_SHADER_SAMPLER_BUFFER_SPECULAR);
}

void r3d_shader_load_post_bloom(void)
{
    LOAD_SHADER(post.bloom, SCREEN_VERT, BLOOM_FRAG);

    GET_LOCATION(post.bloom, uBloomMode);
    GET_LOCATION(post.bloom, uBloomIntensity);

    USE_SHADER(post.bloom);

    SET_SAMPLER(post.bloom, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(post.bloom, uBloomTex, R3D_SHADER_SAMPLER_BUFFER_BLOOM);
}

void r3d_shader_load_post_fog(void)
{
    LOAD_SHADER(post.fog, SCREEN_VERT, FOG_FRAG);

    SET_UNIFORM_BUFFER(post.fog, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(post.fog, uFogMode);
    GET_LOCATION(post.fog, uFogColor);
    GET_LOCATION(post.fog, uFogStart);
    GET_LOCATION(post.fog, uFogEnd);
    GET_LOCATION(post.fog, uFogDensity);
    GET_LOCATION(post.fog, uSkyAffect);

    USE_SHADER(post.fog);

    SET_SAMPLER(post.fog, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(post.fog, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_post_dof(void)
{
    LOAD_SHADER(post.dof, SCREEN_VERT, DOF_FRAG);

    SET_UNIFORM_BUFFER(post.dof, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(post.dof, uFocusPoint);
    GET_LOCATION(post.dof, uFocusScale);
    GET_LOCATION(post.dof, uMaxBlurSize);
    GET_LOCATION(post.dof, uDebugMode);

    USE_SHADER(post.dof);

    SET_SAMPLER(post.dof, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(post.dof, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
}

void r3d_shader_load_post_output(void)
{
    LOAD_SHADER(post.output, SCREEN_VERT, OUTPUT_FRAG);

    GET_LOCATION(post.output, uTonemapExposure);
    GET_LOCATION(post.output, uTonemapWhite);
    GET_LOCATION(post.output, uTonemapMode);
    GET_LOCATION(post.output, uBrightness);
    GET_LOCATION(post.output, uContrast);
    GET_LOCATION(post.output, uSaturation);

    USE_SHADER(post.output);

    SET_SAMPLER(post.output, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
}

void r3d_shader_load_post_fxaa(void)
{
    LOAD_SHADER(post.fxaa, SCREEN_VERT, FXAA_FRAG);
    GET_LOCATION(post.fxaa, uSourceTexel);
    USE_SHADER(post.fxaa);
    SET_SAMPLER(post.fxaa, uSourceTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
}

void r3d_shader_load_post_visualizer(void)
{
    LOAD_SHADER(post.visualizer, SCREEN_VERT, VISUALIZER_FRAG);
    GET_LOCATION(post.visualizer, uOutputMode);
    USE_SHADER(post.visualizer);
    SET_SAMPLER(post.visualizer, uSourceTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_shader_init()
{
    memset(&R3D_MOD_SHADER, 0, sizeof(R3D_MOD_SHADER));

    const int UNIFORM_BUFFER_SIZES[R3D_SHADER_BLOCK_COUNT] = {
        [R3D_SHADER_BLOCK_VIEW] = sizeof(r3d_shader_block_view_t),
        [R3D_SHADER_BLOCK_ENV] = sizeof(r3d_shader_block_env_t),
        [R3D_SHADER_BLOCK_LIGHT] = sizeof(r3d_shader_block_light_t),
        [R3D_SHADER_BLOCK_LIGHT_ARRAY] = sizeof(r3d_shader_block_light_array_t),
    };

    glGenBuffers(R3D_SHADER_BLOCK_COUNT, R3D_MOD_SHADER.uniformBuffers);

    for (int i = 0; i < R3D_SHADER_BLOCK_COUNT; i++) {
        GLuint buffer = R3D_MOD_SHADER.uniformBuffers[i];
        glBindBuffer(GL_UNIFORM_BUFFER, R3D_MOD_SHADER.uniformBuffers[i]);
        glBufferData(GL_UNIFORM_BUFFER, UNIFORM_BUFFER_SIZES[i], NULL, GL_DYNAMIC_DRAW);
    }

    return true;
}

void r3d_shader_quit()
{
    glDeleteBuffers(R3D_SHADER_BLOCK_COUNT, R3D_MOD_SHADER.uniformBuffers);

    UNLOAD_SHADER(prepare.bufferDown);
    UNLOAD_SHADER(prepare.atrousWavelet);
    UNLOAD_SHADER(prepare.bicubicUp);
    UNLOAD_SHADER(prepare.lanczosUp);
    UNLOAD_SHADER(prepare.blurDown);
    UNLOAD_SHADER(prepare.blurUp);
    UNLOAD_SHADER(prepare.ssao);
    UNLOAD_SHADER(prepare.ssil);
    UNLOAD_SHADER(prepare.ssr);
    UNLOAD_SHADER(prepare.bloomDown);
    UNLOAD_SHADER(prepare.bloomUp);
    UNLOAD_SHADER(prepare.cubemapFromEquirectangular);
    UNLOAD_SHADER(prepare.cubemapIrradiance);
    UNLOAD_SHADER(prepare.cubemapPrefilter);
    UNLOAD_SHADER(prepare.cubemapSkybox);

    UNLOAD_SHADER(scene.geometry);
    UNLOAD_SHADER(scene.forward);
    UNLOAD_SHADER(scene.background);
    UNLOAD_SHADER(scene.skybox);
    UNLOAD_SHADER(scene.depth);
    UNLOAD_SHADER(scene.depthCube);
    UNLOAD_SHADER(scene.probe);
    UNLOAD_SHADER(scene.decal);

    UNLOAD_SHADER(deferred.ambient);
    UNLOAD_SHADER(deferred.lighting);
    UNLOAD_SHADER(deferred.compose);

    UNLOAD_SHADER(post.bloom);
    UNLOAD_SHADER(post.fog);
    UNLOAD_SHADER(post.dof);
    UNLOAD_SHADER(post.output);
    UNLOAD_SHADER(post.fxaa);
    UNLOAD_SHADER(post.visualizer);
}

void r3d_shader_bind_sampler(r3d_shader_sampler_t sampler, GLuint texture)
{
    assert(R3D_MOD_SHADER_SAMPLER_TYPES[sampler] != GL_NONE);

    if (texture != R3D_MOD_SHADER.samplerBindings[sampler]) {
        glActiveTexture(GL_TEXTURE0 + sampler);
        glBindTexture(R3D_MOD_SHADER_SAMPLER_TYPES[sampler], texture);
        R3D_MOD_SHADER.samplerBindings[sampler] = texture;
        glActiveTexture(GL_TEXTURE0);
    }
}

void r3d_shader_unbind_samplers(void)
{
    for (int iSampler = 0; iSampler < R3D_SHADER_SAMPLER_COUNT; iSampler++) {
        if (R3D_MOD_SHADER.samplerBindings[iSampler] != 0) {
            glActiveTexture(GL_TEXTURE0 + iSampler);
            glBindTexture(R3D_MOD_SHADER_SAMPLER_TYPES[iSampler], 0);
            R3D_MOD_SHADER.samplerBindings[iSampler] = 0;
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

void r3d_shader_set_uniform_block(r3d_shader_block_t block, const void* data)
{
    int blockSlot = 0;
    int blockSize = 0;

    switch (block) {
    case R3D_SHADER_BLOCK_VIEW:
        blockSlot = R3D_SHADER_BLOCK_VIEW_SLOT;
        blockSize = sizeof(r3d_shader_block_view_t);
        break;
    case R3D_SHADER_BLOCK_ENV:
        blockSlot = R3D_SHADER_BLOCK_ENV_SLOT;
        blockSize = sizeof(r3d_shader_block_env_t);
        break;
    case R3D_SHADER_BLOCK_LIGHT:
        blockSlot = R3D_SHADER_BLOCK_LIGHT_SLOT;
        blockSize = sizeof(r3d_shader_block_light_t);
        break;
    case R3D_SHADER_BLOCK_LIGHT_ARRAY:
        blockSlot = R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT;
        blockSize = sizeof(r3d_shader_block_light_array_t);
        break;
    case R3D_SHADER_BLOCK_COUNT:
        return;
    }

    GLuint ubo = R3D_MOD_SHADER.uniformBuffers[block];

    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, blockSize, data);
    glBindBufferBase(GL_UNIFORM_BUFFER, blockSlot, ubo);
}

// ========================================
// HELPER FUNCTIONS DEFINITIONS
// ========================================

char* inject_defines_to_shader_code(const char* code, const char* defines[], int count)
{
    if (!code || count < 0) return NULL;

    // Find the end of the #version line
    const char* versionStart = strstr(code, "#version");
    assert(versionStart && "Shader must have version");

    const char* versionEnd = strchr(versionStart, '\n');
    if (!versionEnd) versionEnd = versionStart + strlen(versionStart);
    else versionEnd++; // Include the \n

    // Calculate sizes
    static const char DEFINE_PREFIX[] = "#define ";
    static const size_t DEFINE_PREFIX_LEN = sizeof(DEFINE_PREFIX) - 1; // -1 to exclude '\0'
    
    size_t prefixLen = versionEnd - code;
    size_t definesLen = 0;
    for (int i = 0; i < count; i++) {
        if (defines[i]) {
            definesLen += DEFINE_PREFIX_LEN + strlen(defines[i]) + 1; // +1 for \n
        }
    }
    size_t suffixLen = strlen(versionEnd);
    
    // Allocate and build the new shader
    char* newShader = (char*)RL_MALLOC(prefixLen + definesLen + suffixLen + 1);
    if (!newShader) return NULL;

    char* dest = newShader;
    
    // Copy the part before defines (up to after #version)
    memcpy(dest, code, prefixLen);
    dest += prefixLen;

    // Add the defines
    for (int i = 0; i < count; i++) {
        if (defines[i]) {
            memcpy(dest, DEFINE_PREFIX, DEFINE_PREFIX_LEN);
            dest += DEFINE_PREFIX_LEN;
            
            size_t defineLen = strlen(defines[i]);
            memcpy(dest, defines[i], defineLen);
            dest += defineLen;
            
            *dest++ = '\n';
        }
    }

    // Copy the rest of the shader
    memcpy(dest, versionEnd, suffixLen);
    dest[suffixLen] = '\0';

    return newShader;
}
