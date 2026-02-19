/* r3d_shader.c -- Internal R3D shader module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_shader.h"
#include <r3d_config.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <rlgl.h>

#include "../common/r3d_helper.h"
#include "r3d/r3d_core.h"

// ========================================
// SHADER CODE INCLUDES
// ========================================

#include <shaders/color.frag.h>
#include <shaders/screen.vert.h>
#include <shaders/cubemap.vert.h>
#include <shaders/atrous_wavelet.frag.h>
#include <shaders/bicubic_up.frag.h>
#include <shaders/lanczos_up.frag.h>
#include <shaders/blur_down.frag.h>
#include <shaders/blur_up.frag.h>
#include <shaders/ssao_in_down.frag.h>
#include <shaders/ssao.frag.h>
#include <shaders/ssao_blur.frag.h>
#include <shaders/ssil_in_down.frag.h>
#include <shaders/ssil.frag.h>
#include <shaders/ssgi_in_down.frag.h>
#include <shaders/ssgi.frag.h>
#include <shaders/ssr_in_down.frag.h>
#include <shaders/ssr.frag.h>
#include <shaders/bloom_down.frag.h>
#include <shaders/bloom_up.frag.h>
#include <shaders/dof_coc.frag.h>
#include <shaders/dof_down.frag.h>
#include <shaders/dof_blur.frag.h>
#include <shaders/smaa_blending_weigths.vert.h>
#include <shaders/smaa_blending_weigths.frag.h>
#include <shaders/smaa_edge_detection.vert.h>
#include <shaders/smaa_edge_detection.frag.h>
#include <shaders/cubemap_from_equirectangular.frag.h>
#include <shaders/cubemap_irradiance.frag.h>
#include <shaders/cubemap_prefilter.frag.h>
#include <shaders/cubemap_skybox.frag.h>
#include <shaders/scene.vert.h>
#include <shaders/geometry.frag.h>
#include <shaders/forward.frag.h>
#include <shaders/unlit.frag.h>
#include <shaders/depth.frag.h>
#include <shaders/depth_cube.frag.h>
#include <shaders/decal.frag.h>
#include <shaders/skybox.vert.h>
#include <shaders/skybox.frag.h>
#include <shaders/ambient.frag.h>
#include <shaders/lighting.frag.h>
#include <shaders/compose.frag.h>
#include <shaders/fog.frag.h>
#include <shaders/bloom.frag.h>
#include <shaders/dof.frag.h>
#include <shaders/screen.frag.h>
#include <shaders/output.frag.h>
#include <shaders/fxaa.frag.h>
#include <shaders/smaa.vert.h>
#include <shaders/smaa.frag.h>
#include <shaders/visualizer.frag.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_mod_shader R3D_MOD_SHADER;

// ========================================
// INTERNAL MACROS
// ========================================

#define DECL_SHADER(type, category, shader_name) \
    type* shader_name = &R3D_MOD_SHADER.category.shader_name

#define DECL_SHADER_INDEXED(type, category, shader_name, index) \
    type* shader_name = &R3D_MOD_SHADER.category.shader_name[index]

#define DECL_SHADER_SELECT(type, category, shader_name, custom)                 \
    type* shader_name = ((custom) == NULL)                                      \
        ? &R3D_MOD_SHADER.category.shader_name                                  \
        : &(custom)->category.shader_name

#define LOAD_SHADER(shader_name, vsCode, fsCode) do {                           \
    shader_name->id = load_shader(vsCode, fsCode);                              \
    if (shader_name->id == 0) {                                                 \
        R3D_TRACELOG(LOG_ERROR, "Failed to load shader '" #shader_name "'");    \
        return false;                                                           \
    }                                                                           \
} while(0)

#define USE_SHADER(shader_name) do {                                            \
    glUseProgram(shader_name->id);                                              \
} while(0)                                                                      \

#define GET_LOCATION(shader_name, uniform) do {                                 \
    shader_name->uniform.loc = glGetUniformLocation(                            \
        shader_name->id, #uniform                                               \
    );                                                                          \
} while(0)

#define SET_SAMPLER(shader_name, uniform, value) do {                           \
    GLint loc = glGetUniformLocation(shader_name->id, #uniform);                \
    glUniform1i(loc, (int)(value));                                             \
    shader_name->uniform.slot = (int)(value);                                   \
} while(0)

#define SET_UNIFORM_BUFFER(shader_name, uniform, slot) do {                     \
    GLuint idx = glGetUniformBlockIndex(shader_name->id, #uniform);             \
    glUniformBlockBinding(shader_name->id, idx, slot);                          \
} while(0)                                                                      \

#define UNLOAD_SHADER(shader_name) do {                                         \
    if (R3D_MOD_SHADER.shader_name.id != 0) {                                   \
        glDeleteProgram(R3D_MOD_SHADER.shader_name.id);                         \
    }                                                                           \
} while(0)

#define UNLOAD_SHADERS(shader_name, count) do {                                 \
    for (int i = 0; i < count; i++) {                                           \
        if (R3D_MOD_SHADER.shader_name[i].id != 0) {                            \
            glDeleteProgram(R3D_MOD_SHADER.shader_name[i].id);                  \
        }                                                                       \
    }                                                                           \
} while(0)

// ========================================
// INTERNAL FUNCTIONS
// ========================================

/**
 * Inject content into a string at a marker position
 * Returns newly allocated string with injected content
 * Or NULL on failure (caller must free the returned string)
 * source: Source string to modify
 * content: Content to inject
 * marker: String marker to find in source
 * mode: Injection mode: <0 = before marker, 0 = replace marker, >0 = after marker
 */
static char* inject_content(const char* source, const char* content, const char* marker, int mode);

/**
 * Inject the provided list of definitions into the code
 * Returns newly allocated string with injected content
 * Or NULL on failure (caller must free the returned string)
 */
static char* inject_defines(const char* code, const char* defines[], int count);

/*
 * Modifies each input stage by injecting user code if it contains the corresponding stages.
 */
static void inject_user_code(char** vsCode, char** fsCode, const char* userCode);

/**
 * Initializes the sampler locations for the given shader program.
 */
static void set_custom_samplers(GLuint id, r3d_shader_custom_t* custom);

// ========================================
// SHADER COMPLING / LINKING FUNCTIONS
// ========================================

static GLuint compile_shader(const char* source, GLenum shaderType)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader == 0) {
        R3D_TRACELOG(LOG_ERROR, "Failed to create shader object");
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
        R3D_TRACELOG(LOG_ERROR, "%s shader compilation failed: %s", type_str, infoLog);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint link_shader(GLuint vertShader, GLuint fragShader)
{
    GLuint program = glCreateProgram();
    if (program == 0) {
        R3D_TRACELOG(LOG_ERROR, "Failed to create shader program");
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
        R3D_TRACELOG(LOG_ERROR, "Shader program linking failed: %s", infoLog);
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

bool r3d_shader_load_prepare_atrous_wavelet(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_atrous_wavelet_t, prepare, atrousWavelet);
    LOAD_SHADER(atrousWavelet, SCREEN_VERT, ATROUS_WAVELET_FRAG);

    SET_UNIFORM_BUFFER(atrousWavelet, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(atrousWavelet, uInvNormalSharp);
    GET_LOCATION(atrousWavelet, uInvDepthSharp);
    GET_LOCATION(atrousWavelet, uInvStepWidth2);
    GET_LOCATION(atrousWavelet, uStepWidth);

    USE_SHADER(atrousWavelet);
    SET_SAMPLER(atrousWavelet, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);
    SET_SAMPLER(atrousWavelet, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(atrousWavelet, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_bicubic_up(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_bicubic_up_t, prepare, bicubicUp);
    LOAD_SHADER(bicubicUp, SCREEN_VERT, BICUBIC_UP_FRAG);

    GET_LOCATION(bicubicUp, uSourceTexel);

    USE_SHADER(bicubicUp);
    SET_SAMPLER(bicubicUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);

    return true;
}

bool r3d_shader_load_prepare_lanczos_up(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_lanczos_up_t, prepare, lanczosUp);
    LOAD_SHADER(lanczosUp, SCREEN_VERT, LANCZOS_UP_FRAG);

    GET_LOCATION(lanczosUp, uSourceTexel);

    USE_SHADER(lanczosUp);
    SET_SAMPLER(lanczosUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);

    return true;
}

bool r3d_shader_load_prepare_blur_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_blur_down_t, prepare, blurDown);
    LOAD_SHADER(blurDown, SCREEN_VERT, BLUR_DOWN_FRAG);

    GET_LOCATION(blurDown, uSourceLod);

    USE_SHADER(blurDown);
    SET_SAMPLER(blurDown, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);

    return true;
}

bool r3d_shader_load_prepare_blur_up(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_blur_up_t, prepare, blurUp);
    LOAD_SHADER(blurUp, SCREEN_VERT, BLUR_UP_FRAG);

    GET_LOCATION(blurUp, uSourceLod);

    USE_SHADER(blurUp);
    SET_SAMPLER(blurUp, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);

    return true;
}

bool r3d_shader_load_prepare_ssao_in_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssao_in_down_t, prepare, ssaoInDown);
    LOAD_SHADER(ssaoInDown, SCREEN_VERT, SSAO_IN_DOWN_FRAG);

    USE_SHADER(ssaoInDown);
    SET_SAMPLER(ssaoInDown, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssaoInDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssao(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssao_t, prepare, ssao);
    LOAD_SHADER(ssao, SCREEN_VERT, SSAO_FRAG);

    SET_UNIFORM_BUFFER(ssao, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(ssao, uSampleCount);
    GET_LOCATION(ssao, uRadius);
    GET_LOCATION(ssao, uBias);
    GET_LOCATION(ssao, uIntensity);

    USE_SHADER(ssao);

    SET_SAMPLER(ssao, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssao, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssao_blur(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssao_blur_t, prepare, ssaoBlur);
    LOAD_SHADER(ssaoBlur, SCREEN_VERT, SSAO_BLUR_FRAG);

    GET_LOCATION(ssaoBlur, uDirection);

    USE_SHADER(ssaoBlur);

    SET_SAMPLER(ssaoBlur, uSsaoTex, R3D_SHADER_SAMPLER_BUFFER_SSAO);
    SET_SAMPLER(ssaoBlur, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssil_in_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssil_in_down_t, prepare, ssilInDown);
    LOAD_SHADER(ssilInDown, SCREEN_VERT, SSIL_IN_DOWN_FRAG);

    USE_SHADER(ssilInDown);
    SET_SAMPLER(ssilInDown, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssilInDown, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssilInDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssil(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssil_t, prepare, ssil);
    LOAD_SHADER(ssil, SCREEN_VERT, SSIL_FRAG);

    SET_UNIFORM_BUFFER(ssil, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(ssil, uSampleCount);
    GET_LOCATION(ssil, uSliceCount);
    GET_LOCATION(ssil, uRadius);
    GET_LOCATION(ssil, uThickness);

    USE_SHADER(ssil);

    SET_SAMPLER(ssil, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssil, uHistoryTex, R3D_SHADER_SAMPLER_BUFFER_SSIL);
    SET_SAMPLER(ssil, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssil, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssgi_in_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssgi_in_down_t, prepare, ssgiInDown);
    LOAD_SHADER(ssgiInDown, SCREEN_VERT, SSGI_IN_DOWN_FRAG);

    USE_SHADER(ssgiInDown);
    SET_SAMPLER(ssgiInDown, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssgiInDown, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssgiInDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssgi(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssgi_t, prepare, ssgi);
    LOAD_SHADER(ssgi, SCREEN_VERT, SSGI_FRAG);

    SET_UNIFORM_BUFFER(ssgi, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(ssgi, uSampleCount);
    GET_LOCATION(ssgi, uMaxRaySteps);
    GET_LOCATION(ssgi, uStepSize);
    GET_LOCATION(ssgi, uThickness);
    GET_LOCATION(ssgi, uMaxDistance);
    GET_LOCATION(ssgi, uFadeStart);
    GET_LOCATION(ssgi, uFadeEnd);

    USE_SHADER(ssgi);

    SET_SAMPLER(ssgi, uHistoryTex, R3D_SHADER_SAMPLER_BUFFER_SSGI);
    SET_SAMPLER(ssgi, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssgi, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssgi, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(ssgi, uLutTex, R3D_SHADER_SAMPLER_SOURCE_1D_0);

    return true;
}

bool r3d_shader_load_prepare_ssr_in_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssr_in_down_t, prepare, ssrInDown);
    LOAD_SHADER(ssrInDown, SCREEN_VERT, SSR_IN_DOWN_FRAG);

    USE_SHADER(ssrInDown);
    SET_SAMPLER(ssrInDown, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssrInDown, uSpecularTex, R3D_SHADER_SAMPLER_BUFFER_SPECULAR);
    SET_SAMPLER(ssrInDown, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssrInDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_ssr(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_ssr_t, prepare, ssr);
    LOAD_SHADER(ssr, SCREEN_VERT, SSR_FRAG);

    SET_UNIFORM_BUFFER(ssr, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(ssr, uMaxRaySteps);
    GET_LOCATION(ssr, uBinarySteps);
    GET_LOCATION(ssr, uStepSize);
    GET_LOCATION(ssr, uThickness);
    GET_LOCATION(ssr, uMaxDistance);
    GET_LOCATION(ssr, uEdgeFade);

    USE_SHADER(ssr);

    SET_SAMPLER(ssr, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(ssr, uSpecularTex, R3D_SHADER_SAMPLER_BUFFER_SPECULAR);
    SET_SAMPLER(ssr, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ssr, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_bloom_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_bloom_down_t, prepare, bloomDown);
    LOAD_SHADER(bloomDown, SCREEN_VERT, BLOOM_DOWN_FRAG);

    GET_LOCATION(bloomDown, uTexelSize);
    GET_LOCATION(bloomDown, uPrefilter);
    GET_LOCATION(bloomDown, uDstLevel);

    USE_SHADER(bloomDown);
    SET_SAMPLER(bloomDown, uTexture, R3D_SHADER_SAMPLER_BUFFER_BLOOM);

    return true;
}

bool r3d_shader_load_prepare_bloom_up(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_bloom_up_t, prepare, bloomUp);
    LOAD_SHADER(bloomUp, SCREEN_VERT, BLOOM_UP_FRAG);

    GET_LOCATION(bloomUp, uFilterRadius);
    GET_LOCATION(bloomUp, uSrcLevel);

    USE_SHADER(bloomUp);
    SET_SAMPLER(bloomUp, uTexture, R3D_SHADER_SAMPLER_BUFFER_BLOOM);

    return true;
}

bool r3d_shader_load_prepare_dof_coc(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_dof_coc_t, prepare, dofCoc);
    LOAD_SHADER(dofCoc, SCREEN_VERT, DOF_COC_FRAG);

    GET_LOCATION(dofCoc, uFocusPoint);
    GET_LOCATION(dofCoc, uFocusScale);

    USE_SHADER(dofCoc);
    SET_SAMPLER(dofCoc, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_prepare_dof_down(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_dof_down_t, prepare, dofDown);
    LOAD_SHADER(dofDown, SCREEN_VERT, DOF_DOWN_FRAG);

    USE_SHADER(dofDown);
    SET_SAMPLER(dofDown, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(dofDown, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(dofDown, uCoCTex, R3D_SHADER_SAMPLER_BUFFER_DOF_COC);

    return true;
}

bool r3d_shader_load_prepare_dof_blur(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_dof_blur_t, prepare, dofBlur);
    LOAD_SHADER(dofBlur, SCREEN_VERT, DOF_BLUR_FRAG);

    GET_LOCATION(dofBlur, uMaxBlurSize);

    USE_SHADER(dofBlur);
    SET_SAMPLER(dofBlur, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_DOF);     //< RGB: Color | A: CoC
    SET_SAMPLER(dofBlur, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

static bool load_prepare_smaa_edge_detection(r3d_shader_custom_t* custom, int index)
{
    char defQualityPreset[32] = {0};
    r3d_string_format(defQualityPreset, sizeof(defQualityPreset), "QUALITY_PRESET %i", index);

    const char* VS_DEFINES[] = {defQualityPreset};
    const char* FS_DEFINES[] = {defQualityPreset};

    char* vsCode = inject_defines(SMAA_EDGE_DETECTION_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(SMAA_EDGE_DETECTION_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    DECL_SHADER_INDEXED(r3d_shader_prepare_smaa_edge_detection_t, prepare, smaaEdgeDetection, index);
    LOAD_SHADER(smaaEdgeDetection, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    GET_LOCATION(smaaEdgeDetection, uMetrics);

    USE_SHADER(smaaEdgeDetection);
    SET_SAMPLER(smaaEdgeDetection, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);

    return true;
}

bool r3d_shader_load_prepare_smaa_edge_detection_low(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_edge_detection(custom, R3D_ANTI_ALIASING_PRESET_LOW);
}

bool r3d_shader_load_prepare_smaa_edge_detection_medium(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_edge_detection(custom, R3D_ANTI_ALIASING_PRESET_MEDIUM);
}

bool r3d_shader_load_prepare_smaa_edge_detection_high(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_edge_detection(custom, R3D_ANTI_ALIASING_PRESET_HIGH);
}

bool r3d_shader_load_prepare_smaa_edge_detection_ultra(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_edge_detection(custom, R3D_ANTI_ALIASING_PRESET_ULTRA);
}

static bool load_prepare_smaa_blending_weights(r3d_shader_custom_t* custom, int index)
{
    char defQualityPreset[32] = {0};
    r3d_string_format(defQualityPreset, sizeof(defQualityPreset), "QUALITY_PRESET %i", index);

    const char* VS_DEFINES[] = {defQualityPreset};
    const char* FS_DEFINES[] = {defQualityPreset};

    char* vsCode = inject_defines(SMAA_BLENDING_WEIGTHS_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(SMAA_BLENDING_WEIGTHS_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    DECL_SHADER_INDEXED(r3d_shader_prepare_smaa_blending_weights_t, prepare, smaaBlendingWeights, index);
    LOAD_SHADER(smaaBlendingWeights, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    GET_LOCATION(smaaBlendingWeights, uMetrics);

    USE_SHADER(smaaBlendingWeights);
    SET_SAMPLER(smaaBlendingWeights, uEdgesTex, R3D_SHADER_SAMPLER_BUFFER_SMAA_EDGES);
    SET_SAMPLER(smaaBlendingWeights, uAreaTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);
    SET_SAMPLER(smaaBlendingWeights, uSearchTex, R3D_SHADER_SAMPLER_SOURCE_2D_1);

    return true;
}

bool r3d_shader_load_prepare_smaa_blending_weights_low(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_blending_weights(custom, R3D_ANTI_ALIASING_PRESET_LOW);
}

bool r3d_shader_load_prepare_smaa_blending_weights_medium(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_blending_weights(custom, R3D_ANTI_ALIASING_PRESET_MEDIUM);
}

bool r3d_shader_load_prepare_smaa_blending_weights_high(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_blending_weights(custom, R3D_ANTI_ALIASING_PRESET_HIGH);
}

bool r3d_shader_load_prepare_smaa_blending_weights_ultra(r3d_shader_custom_t* custom)
{
    return load_prepare_smaa_blending_weights(custom, R3D_ANTI_ALIASING_PRESET_ULTRA);
}

bool r3d_shader_load_prepare_cubemap_from_equirectangular(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_cubemap_from_equirectangular_t, prepare, cubemapFromEquirectangular);
    LOAD_SHADER(cubemapFromEquirectangular, CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG);

    GET_LOCATION(cubemapFromEquirectangular, uMatProj);
    GET_LOCATION(cubemapFromEquirectangular, uMatView);

    USE_SHADER(cubemapFromEquirectangular);
    SET_SAMPLER(cubemapFromEquirectangular, uPanoramaTex, R3D_SHADER_SAMPLER_SOURCE_2D_0);

    return true;
}

bool r3d_shader_load_prepare_cubemap_irradiance(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_cubemap_irradiance_t, prepare, cubemapIrradiance);
    LOAD_SHADER(cubemapIrradiance, CUBEMAP_VERT, CUBEMAP_IRRADIANCE_FRAG);

    GET_LOCATION(cubemapIrradiance, uMatProj);
    GET_LOCATION(cubemapIrradiance, uMatView);

    USE_SHADER(cubemapIrradiance);
    SET_SAMPLER(cubemapIrradiance, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_CUBE_0);

    return true;
}

bool r3d_shader_load_prepare_cubemap_prefilter(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_cubemap_prefilter_t, prepare, cubemapPrefilter);
    LOAD_SHADER(cubemapPrefilter, CUBEMAP_VERT, CUBEMAP_PREFILTER_FRAG);

    GET_LOCATION(cubemapPrefilter, uMatProj);
    GET_LOCATION(cubemapPrefilter, uMatView);
    GET_LOCATION(cubemapPrefilter, uSourceNumLevels);
    GET_LOCATION(cubemapPrefilter, uSourceFaceSize);
    GET_LOCATION(cubemapPrefilter, uRoughness);

    USE_SHADER(cubemapPrefilter);
    SET_SAMPLER(cubemapPrefilter, uSourceTex, R3D_SHADER_SAMPLER_SOURCE_CUBE_0);

    return true;
}

bool r3d_shader_load_prepare_cubemap_skybox(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_prepare_cubemap_skybox_t, prepare, cubemapSkybox);
    LOAD_SHADER(cubemapSkybox, CUBEMAP_VERT, CUBEMAP_SKYBOX_FRAG);

    GET_LOCATION(cubemapSkybox, uMatProj);
    GET_LOCATION(cubemapSkybox, uMatView);
    GET_LOCATION(cubemapSkybox, uSkyTopColor);
    GET_LOCATION(cubemapSkybox, uSkyHorizonColor);
    GET_LOCATION(cubemapSkybox, uSkyHorizonCurve);
    GET_LOCATION(cubemapSkybox, uSkyEnergy);
    GET_LOCATION(cubemapSkybox, uGroundBottomColor);
    GET_LOCATION(cubemapSkybox, uGroundHorizonColor);
    GET_LOCATION(cubemapSkybox, uGroundHorizonCurve);
    GET_LOCATION(cubemapSkybox, uGroundEnergy);
    GET_LOCATION(cubemapSkybox, uSunDirection);
    GET_LOCATION(cubemapSkybox, uSunColor);
    GET_LOCATION(cubemapSkybox, uSunSize);
    GET_LOCATION(cubemapSkybox, uSunCurve);
    GET_LOCATION(cubemapSkybox, uSunEnergy);

    USE_SHADER(cubemapSkybox);

    return true;
}

bool r3d_shader_load_scene_geometry(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_geometry_t, scene, geometry, custom);

    const char* VS_DEFINES[] = {"STAGE_VERT", "GEOMETRY"};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "GEOMETRY"};

    char* vsCode = inject_defines(SCENE_VERT,    VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(GEOMETRY_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(geometry, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(geometry, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(geometry, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(geometry, uMatNormal);
    GET_LOCATION(geometry, uMatModel);
    GET_LOCATION(geometry, uAlbedoColor);
    GET_LOCATION(geometry, uEmissionEnergy);
    GET_LOCATION(geometry, uEmissionColor);
    GET_LOCATION(geometry, uTexCoordOffset);
    GET_LOCATION(geometry, uTexCoordScale);
    GET_LOCATION(geometry, uInstancing);
    GET_LOCATION(geometry, uSkinning);
    GET_LOCATION(geometry, uBillboard);
    GET_LOCATION(geometry, uAlphaCutoff);
    GET_LOCATION(geometry, uNormalScale);
    GET_LOCATION(geometry, uOcclusion);
    GET_LOCATION(geometry, uRoughness);
    GET_LOCATION(geometry, uMetalness);

    USE_SHADER(geometry);

    SET_SAMPLER(geometry, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(geometry, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(geometry, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(geometry, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(geometry, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);

    if (custom != NULL) {
        set_custom_samplers(geometry->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_forward(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_forward_t, scene, forward, custom);

    char defNumForwardLights[32] = {0};
    char defNumProbes[32] = {0};

    r3d_string_format(defNumForwardLights, sizeof(defNumForwardLights), "NUM_FORWARD_LIGHTS %i", R3D_MAX_LIGHT_FORWARD_PER_MESH);
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_MAX_PROBE_ON_SCREEN);

    const char* VS_DEFINES[] = {"STAGE_VERT", "FORWARD", defNumForwardLights};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "FORWARD", defNumForwardLights, defNumProbes};

    char* vsCode = inject_defines(SCENE_VERT,   VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(FORWARD_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(forward, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(forward, LightArrayBlock, R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT);
    SET_UNIFORM_BUFFER(forward, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(forward, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(forward, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(forward, uMatNormal);
    GET_LOCATION(forward, uMatModel);
    GET_LOCATION(forward, uAlbedoColor);
    GET_LOCATION(forward, uEmissionColor);
    GET_LOCATION(forward, uEmissionEnergy);
    GET_LOCATION(forward, uTexCoordOffset);
    GET_LOCATION(forward, uTexCoordScale);
    GET_LOCATION(forward, uInstancing);
    GET_LOCATION(forward, uSkinning);
    GET_LOCATION(forward, uBillboard);
    GET_LOCATION(forward, uNormalScale);
    GET_LOCATION(forward, uOcclusion);
    GET_LOCATION(forward, uRoughness);
    GET_LOCATION(forward, uMetalness);
    GET_LOCATION(forward, uViewPosition);

    USE_SHADER(forward);

    SET_SAMPLER(forward, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(forward, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(forward, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(forward, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(forward, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(forward, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(forward, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(forward, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);
    SET_SAMPLER(forward, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(forward, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(forward, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);

    if (custom != NULL) {
        set_custom_samplers(forward->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_unlit(r3d_shader_custom_t *custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_unlit_t, scene, unlit, custom);

    const char* VS_DEFINES[] = {"STAGE_VERT", "UNLIT"};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "UNLIT"};

    char* vsCode = inject_defines(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(UNLIT_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(unlit, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(unlit, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(unlit, uMatNormal);
    GET_LOCATION(unlit, uMatModel);
    GET_LOCATION(unlit, uAlbedoColor);
    GET_LOCATION(unlit, uTexCoordOffset);
    GET_LOCATION(unlit, uTexCoordScale);
    GET_LOCATION(unlit, uInstancing);
    GET_LOCATION(unlit, uSkinning);
    GET_LOCATION(unlit, uBillboard);
    GET_LOCATION(unlit, uMatInvView);
    GET_LOCATION(unlit, uMatViewProj);
    GET_LOCATION(unlit, uAlphaCutoff);

    USE_SHADER(unlit);

    SET_SAMPLER(unlit, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(unlit, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);

    if (custom != NULL) {
        set_custom_samplers(unlit->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_background(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_scene_background_t, scene, background);
    LOAD_SHADER(background, SCREEN_VERT, COLOR_FRAG);
    GET_LOCATION(background, uColor);

    return true;
}

bool r3d_shader_load_scene_skybox(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_scene_skybox_t, scene, skybox);
    LOAD_SHADER(skybox, SKYBOX_VERT, SKYBOX_FRAG);

    GET_LOCATION(skybox, uMatInvView);
    GET_LOCATION(skybox, uMatInvProj);
    GET_LOCATION(skybox, uRotation);
    GET_LOCATION(skybox, uEnergy);
    GET_LOCATION(skybox, uLod);

    USE_SHADER(skybox);

    SET_SAMPLER(skybox, uSkyMap, R3D_SHADER_SAMPLER_SOURCE_CUBE_0);

    return true;
}

bool r3d_shader_load_scene_depth(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_depth_t, scene, depth, custom);

    const char* VS_DEFINES[] = {"STAGE_VERT", "DEPTH"};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "DEPTH"};

    char* vsCode = inject_defines(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(DEPTH_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(depth, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(depth, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(depth, uMatInvView);
    GET_LOCATION(depth, uMatModel);
    GET_LOCATION(depth, uMatViewProj);
    GET_LOCATION(depth, uAlbedoColor);
    GET_LOCATION(depth, uTexCoordOffset);
    GET_LOCATION(depth, uTexCoordScale);
    GET_LOCATION(depth, uInstancing);
    GET_LOCATION(depth, uSkinning);
    GET_LOCATION(depth, uBillboard);
    GET_LOCATION(depth, uAlphaCutoff);

    USE_SHADER(depth);

    SET_SAMPLER(depth, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(depth, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);

    if (custom != NULL) {
        set_custom_samplers(depth->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_depth_cube(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_depth_cube_t, scene, depthCube, custom);

    const char* VS_DEFINES[] = {"STAGE_VERT", "DEPTH_CUBE"};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "DEPTH_CUBE"};

    char* vsCode = inject_defines(SCENE_VERT,      VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(DEPTH_CUBE_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(depthCube, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(depthCube, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(depthCube, uMatInvView);
    GET_LOCATION(depthCube, uMatModel);
    GET_LOCATION(depthCube, uMatViewProj);
    GET_LOCATION(depthCube, uAlbedoColor);
    GET_LOCATION(depthCube, uTexCoordOffset);
    GET_LOCATION(depthCube, uTexCoordScale);
    GET_LOCATION(depthCube, uInstancing);
    GET_LOCATION(depthCube, uSkinning);
    GET_LOCATION(depthCube, uBillboard);
    GET_LOCATION(depthCube, uAlphaCutoff);
    GET_LOCATION(depthCube, uViewPosition);
    GET_LOCATION(depthCube, uFar);

    USE_SHADER(depthCube);

    SET_SAMPLER(depthCube, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(depthCube, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);

    if (custom != NULL) {
        set_custom_samplers(depthCube->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_probe(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_probe_t, scene, probe, custom);

    char defNumForwardLights[32] = {0};
    char defNumProbes[32] = {0};

    r3d_string_format(defNumForwardLights, sizeof(defNumForwardLights), "NUM_FORWARD_LIGHTS %i", R3D_MAX_LIGHT_FORWARD_PER_MESH);
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_MAX_PROBE_ON_SCREEN);

    const char* VS_DEFINES[] = {"STAGE_VERT", "PROBE", defNumForwardLights};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "PROBE", defNumForwardLights, defNumProbes};

    char* vsCode = inject_defines(SCENE_VERT,   VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(FORWARD_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(probe, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(probe, LightArrayBlock, R3D_SHADER_BLOCK_LIGHT_ARRAY_SLOT);
    SET_UNIFORM_BUFFER(probe, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(probe, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(probe, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(probe, uMatInvView);
    GET_LOCATION(probe, uMatNormal);
    GET_LOCATION(probe, uMatModel);
    GET_LOCATION(probe, uMatViewProj);
    GET_LOCATION(probe, uAlbedoColor);
    GET_LOCATION(probe, uEmissionColor);
    GET_LOCATION(probe, uEmissionEnergy);
    GET_LOCATION(probe, uTexCoordOffset);
    GET_LOCATION(probe, uTexCoordScale);
    GET_LOCATION(probe, uInstancing);
    GET_LOCATION(probe, uSkinning);
    GET_LOCATION(probe, uBillboard);
    GET_LOCATION(probe, uNormalScale);
    GET_LOCATION(probe, uOcclusion);
    GET_LOCATION(probe, uRoughness);
    GET_LOCATION(probe, uMetalness);
    GET_LOCATION(probe, uViewPosition);
    GET_LOCATION(probe, uProbeInterior);

    USE_SHADER(probe);

    SET_SAMPLER(probe, uBoneMatricesTex, R3D_SHADER_SAMPLER_BONE_MATRICES);
    SET_SAMPLER(probe, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(probe, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(probe, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(probe, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(probe, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(probe, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(probe, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);
    SET_SAMPLER(probe, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(probe, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(probe, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);

    if (custom != NULL) {
        set_custom_samplers(probe->id, custom);
    }

    return true;
}

bool r3d_shader_load_scene_decal(r3d_shader_custom_t* custom)
{
    DECL_SHADER_SELECT(r3d_shader_scene_decal_t, scene, decal, custom);

    const char* VS_DEFINES[] = {"STAGE_VERT", "DECAL"};
    const char* FS_DEFINES[] = {"STAGE_FRAG", "DECAL"};

    char* vsCode = inject_defines(SCENE_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(DECAL_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    const char* userCode = custom ? custom->userCode : NULL;

    if (userCode != NULL) {
        inject_user_code(&vsCode, &fsCode, userCode);
    }

    LOAD_SHADER(decal, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(decal, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    if (userCode && strstr(userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(decal, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(decal, uMatNormal);
    GET_LOCATION(decal, uMatModel);
    GET_LOCATION(decal, uAlbedoColor);
    GET_LOCATION(decal, uEmissionEnergy);
    GET_LOCATION(decal, uEmissionColor);
    GET_LOCATION(decal, uTexCoordOffset);
    GET_LOCATION(decal, uTexCoordScale);
    GET_LOCATION(decal, uInstancing);
    GET_LOCATION(decal, uSkinning);
    GET_LOCATION(decal, uAlphaCutoff);
    GET_LOCATION(decal, uNormalScale);
    GET_LOCATION(decal, uOcclusion);
    GET_LOCATION(decal, uRoughness);
    GET_LOCATION(decal, uMetalness);
    GET_LOCATION(decal, uNormalThreshold);
    GET_LOCATION(decal, uFadeWidth);
    GET_LOCATION(decal, uApplyColor);

    USE_SHADER(decal);

    SET_SAMPLER(decal, uAlbedoMap, R3D_SHADER_SAMPLER_MAP_ALBEDO);
    SET_SAMPLER(decal, uNormalMap, R3D_SHADER_SAMPLER_MAP_NORMAL);
    SET_SAMPLER(decal, uEmissionMap, R3D_SHADER_SAMPLER_MAP_EMISSION);
    SET_SAMPLER(decal, uOrmMap, R3D_SHADER_SAMPLER_MAP_ORM);
    SET_SAMPLER(decal, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(decal, uGeomNormalTex, R3D_SHADER_SAMPLER_BUFFER_GEOM_NORMAL);

    if (custom != NULL) {
        set_custom_samplers(decal->id, custom);
    }

    return true;
}

bool r3d_shader_load_deferred_ambient(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_deferred_ambient_t, deferred, ambient);

    char defNumProbes[32] = {0};
    r3d_string_format(defNumProbes, sizeof(defNumProbes), "NUM_PROBES %i", R3D_MAX_PROBE_ON_SCREEN);

    const char* FS_DEFINES[] = {defNumProbes};
    char* fsCode = inject_defines(AMBIENT_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));
    LOAD_SHADER(ambient, SCREEN_VERT, fsCode);
    RL_FREE(fsCode);

    SET_UNIFORM_BUFFER(ambient, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);
    SET_UNIFORM_BUFFER(ambient, EnvBlock, R3D_SHADER_BLOCK_ENV_SLOT);

    GET_LOCATION(ambient, uSsaoPower);
    GET_LOCATION(ambient, uSsilIntensity);
    GET_LOCATION(ambient, uSsilAoPower);

    USE_SHADER(ambient);

    SET_SAMPLER(ambient, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(ambient, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(ambient, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(ambient, uSsaoTex, R3D_SHADER_SAMPLER_BUFFER_SSAO);
    SET_SAMPLER(ambient, uSsilTex, R3D_SHADER_SAMPLER_BUFFER_SSIL);
    SET_SAMPLER(ambient, uSsgiTex, R3D_SHADER_SAMPLER_BUFFER_SSGI);
    SET_SAMPLER(ambient, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);

    SET_SAMPLER(ambient, uIrradianceTex, R3D_SHADER_SAMPLER_IBL_IRRADIANCE);
    SET_SAMPLER(ambient, uPrefilterTex, R3D_SHADER_SAMPLER_IBL_PREFILTER);
    SET_SAMPLER(ambient, uBrdfLutTex, R3D_SHADER_SAMPLER_IBL_BRDF_LUT);

    return true;
}

bool r3d_shader_load_deferred_lighting(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_deferred_lighting_t, deferred, lighting);
    LOAD_SHADER(lighting, SCREEN_VERT, LIGHTING_FRAG);

    SET_UNIFORM_BUFFER(lighting, LightBlock, R3D_SHADER_BLOCK_LIGHT_SLOT);
    SET_UNIFORM_BUFFER(lighting, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    USE_SHADER(lighting);

    SET_SAMPLER(lighting, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(lighting, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(lighting, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);
    SET_SAMPLER(lighting, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);

    SET_SAMPLER(lighting, uShadowDirTex, R3D_SHADER_SAMPLER_SHADOW_DIR);
    SET_SAMPLER(lighting, uShadowSpotTex, R3D_SHADER_SAMPLER_SHADOW_SPOT);
    SET_SAMPLER(lighting, uShadowOmniTex, R3D_SHADER_SAMPLER_SHADOW_OMNI);

    return true;
}

bool r3d_shader_load_deferred_compose(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_deferred_compose_t, deferred, compose);
    LOAD_SHADER(compose, SCREEN_VERT, COMPOSE_FRAG);

    GET_LOCATION(compose, uSsrNumLevels);

    USE_SHADER(compose);

    SET_SAMPLER(compose, uAlbedoTex, R3D_SHADER_SAMPLER_BUFFER_ALBEDO);
    SET_SAMPLER(compose, uDiffuseTex, R3D_SHADER_SAMPLER_BUFFER_DIFFUSE);
    SET_SAMPLER(compose, uSpecularTex, R3D_SHADER_SAMPLER_BUFFER_SPECULAR);
    SET_SAMPLER(compose, uOrmTex, R3D_SHADER_SAMPLER_BUFFER_ORM);
    SET_SAMPLER(compose, uSsrTex, R3D_SHADER_SAMPLER_BUFFER_SSR);

    return true;
}

bool r3d_shader_load_post_fog(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_post_fog_t, post, fog);
    LOAD_SHADER(fog, SCREEN_VERT, FOG_FRAG);

    SET_UNIFORM_BUFFER(fog, ViewBlock, R3D_SHADER_BLOCK_VIEW_SLOT);

    GET_LOCATION(fog, uFogMode);
    GET_LOCATION(fog, uFogColor);
    GET_LOCATION(fog, uFogStart);
    GET_LOCATION(fog, uFogEnd);
    GET_LOCATION(fog, uFogDensity);
    GET_LOCATION(fog, uSkyAffect);

    USE_SHADER(fog);

    SET_SAMPLER(fog, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(fog, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    return true;
}

bool r3d_shader_load_post_bloom(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_post_bloom_t, post, bloom);
    LOAD_SHADER(bloom, SCREEN_VERT, BLOOM_FRAG);

    GET_LOCATION(bloom, uBloomMode);
    GET_LOCATION(bloom, uBloomIntensity);

    USE_SHADER(bloom);

    SET_SAMPLER(bloom, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(bloom, uBloomTex, R3D_SHADER_SAMPLER_BUFFER_BLOOM);

    return true;
}

bool r3d_shader_load_post_dof(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_post_dof_t, post, dof);
    LOAD_SHADER(dof, SCREEN_VERT, DOF_FRAG);

    USE_SHADER(dof);
    SET_SAMPLER(dof, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(dof, uBlurTex, R3D_SHADER_SAMPLER_BUFFER_DOF);

    return true;
}

bool r3d_shader_load_post_screen(r3d_shader_custom_t* custom)
{
    assert(custom != NULL);

    r3d_shader_post_screen_t* screen = &custom->post.screen;
    char* fragCode = inject_content(SCREEN_FRAG, custom->userCode, "#define fragment()", 0);
    LOAD_SHADER(screen, SCREEN_VERT, fragCode);
    RL_FREE(fragCode);

    if (custom->userCode && strstr(custom->userCode, "UserBlock") != NULL) {
        SET_UNIFORM_BUFFER(screen, UserBlock, R3D_SHADER_BLOCK_USER_SLOT);
    }

    GET_LOCATION(screen, uResolution);
    GET_LOCATION(screen, uTexelSize);

    USE_SHADER(screen);
    SET_SAMPLER(screen, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(screen, uNormalTex, R3D_SHADER_SAMPLER_BUFFER_NORMAL);
    SET_SAMPLER(screen, uDepthTex, R3D_SHADER_SAMPLER_BUFFER_DEPTH);

    if (custom != NULL) {
        set_custom_samplers(screen->id, custom);
    }

    return true;
}

bool r3d_shader_load_post_output(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_post_output_t, post, output);
    LOAD_SHADER(output, SCREEN_VERT, OUTPUT_FRAG);

    GET_LOCATION(output, uTonemapExposure);
    GET_LOCATION(output, uTonemapWhite);
    GET_LOCATION(output, uTonemapMode);
    GET_LOCATION(output, uBrightness);
    GET_LOCATION(output, uContrast);
    GET_LOCATION(output, uSaturation);

    USE_SHADER(output);
    SET_SAMPLER(output, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);

    return true;
}

static bool load_post_fxaa(r3d_shader_custom_t* custom, int index)
{
    char defQualityPreset[32] = {0};
    r3d_string_format(defQualityPreset, sizeof(defQualityPreset), "QUALITY_PRESET %i", index);

    const char* FS_DEFINES[] = {defQualityPreset};
    char* fsCode = inject_defines(FXAA_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    DECL_SHADER_INDEXED(r3d_shader_post_fxaa_t, post, fxaa, index);
    LOAD_SHADER(fxaa, SCREEN_VERT, fsCode);

    RL_FREE(fsCode);

    GET_LOCATION(fxaa, uSceneTexel);

    USE_SHADER(fxaa);
    SET_SAMPLER(fxaa, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);

    return true;
}

bool r3d_shader_load_post_fxaa_low(r3d_shader_custom_t* custom)
{
    return load_post_fxaa(custom, R3D_ANTI_ALIASING_PRESET_LOW);
}

bool r3d_shader_load_post_fxaa_medium(r3d_shader_custom_t* custom)
{
    return load_post_fxaa(custom, R3D_ANTI_ALIASING_PRESET_MEDIUM);
}

bool r3d_shader_load_post_fxaa_high(r3d_shader_custom_t* custom)
{
    return load_post_fxaa(custom, R3D_ANTI_ALIASING_PRESET_HIGH);
}

bool r3d_shader_load_post_fxaa_ultra(r3d_shader_custom_t* custom)
{
    return load_post_fxaa(custom, R3D_ANTI_ALIASING_PRESET_ULTRA);
}

static bool load_post_smaa(r3d_shader_custom_t* custom, int index)
{
    char defQualityPreset[32] = {0};
    r3d_string_format(defQualityPreset, sizeof(defQualityPreset), "QUALITY_PRESET %i", index);

    const char* VS_DEFINES[] = {defQualityPreset};
    const char* FS_DEFINES[] = {defQualityPreset};

    char* vsCode = inject_defines(SMAA_VERT, VS_DEFINES, ARRAY_SIZE(VS_DEFINES));
    char* fsCode = inject_defines(SMAA_FRAG, FS_DEFINES, ARRAY_SIZE(FS_DEFINES));

    DECL_SHADER_INDEXED(r3d_shader_post_smaa_t, post, smaa, index);
    LOAD_SHADER(smaa, vsCode, fsCode);

    RL_FREE(vsCode);
    RL_FREE(fsCode);

    GET_LOCATION(smaa, uMetrics);

    USE_SHADER(smaa);
    SET_SAMPLER(smaa, uSceneTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);
    SET_SAMPLER(smaa, uBlendTex, R3D_SHADER_SAMPLER_BUFFER_SMAA_BLEND);

    return true;
}

bool r3d_shader_load_post_smaa_low(r3d_shader_custom_t* custom)
{
    return load_post_smaa(custom, R3D_ANTI_ALIASING_PRESET_LOW);
}

bool r3d_shader_load_post_smaa_medium(r3d_shader_custom_t* custom)
{
    return load_post_smaa(custom, R3D_ANTI_ALIASING_PRESET_MEDIUM);
}

bool r3d_shader_load_post_smaa_high(r3d_shader_custom_t* custom)
{
    return load_post_smaa(custom, R3D_ANTI_ALIASING_PRESET_HIGH);
}

bool r3d_shader_load_post_smaa_ultra(r3d_shader_custom_t* custom)
{
    return load_post_smaa(custom, R3D_ANTI_ALIASING_PRESET_ULTRA);
}

bool r3d_shader_load_post_visualizer(r3d_shader_custom_t* custom)
{
    DECL_SHADER(r3d_shader_post_visualizer_t, post, visualizer);
    LOAD_SHADER(visualizer, SCREEN_VERT, VISUALIZER_FRAG);

    GET_LOCATION(visualizer, uOutputMode);

    USE_SHADER(visualizer);
    SET_SAMPLER(visualizer, uSourceTex, R3D_SHADER_SAMPLER_BUFFER_SCENE);

    return true;
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

    memcpy(R3D_MOD_SHADER.samplerTargets, R3D_MOD_SHADER_SAMPLER_TYPES, sizeof(R3D_MOD_SHADER_SAMPLER_TYPES));

    for (int i = 0; i < R3D_MAX_SHADER_SAMPLERS; ++i) {
        R3D_MOD_SHADER.samplerTargets[R3D_SHADER_SAMPLER_CUSTOM_1D + i] = GL_TEXTURE_1D;
        R3D_MOD_SHADER.samplerTargets[R3D_SHADER_SAMPLER_CUSTOM_2D + i] = GL_TEXTURE_2D;
        R3D_MOD_SHADER.samplerTargets[R3D_SHADER_SAMPLER_CUSTOM_3D + i] = GL_TEXTURE_3D;
        R3D_MOD_SHADER.samplerTargets[R3D_SHADER_SAMPLER_CUSTOM_CUBE + i] = GL_TEXTURE_CUBE_MAP;
    }

    return true;
}

void r3d_shader_quit()
{
    glDeleteBuffers(R3D_SHADER_BLOCK_COUNT, R3D_MOD_SHADER.uniformBuffers);

    UNLOAD_SHADER(prepare.atrousWavelet);
    UNLOAD_SHADER(prepare.bicubicUp);
    UNLOAD_SHADER(prepare.lanczosUp);
    UNLOAD_SHADER(prepare.blurDown);
    UNLOAD_SHADER(prepare.blurUp);
    UNLOAD_SHADER(prepare.ssaoInDown);
    UNLOAD_SHADER(prepare.ssao);
    UNLOAD_SHADER(prepare.ssaoBlur);
    UNLOAD_SHADER(prepare.ssilInDown);
    UNLOAD_SHADER(prepare.ssil);
    UNLOAD_SHADER(prepare.ssgiInDown);
    UNLOAD_SHADER(prepare.ssgi);
    UNLOAD_SHADER(prepare.ssrInDown);
    UNLOAD_SHADER(prepare.ssr);
    UNLOAD_SHADER(prepare.bloomDown);
    UNLOAD_SHADER(prepare.bloomUp);
    UNLOAD_SHADER(prepare.dofCoc);
    UNLOAD_SHADER(prepare.dofDown);
    UNLOAD_SHADER(prepare.dofBlur);
    UNLOAD_SHADERS(prepare.smaaEdgeDetection, R3D_ANTI_ALIASING_PRESET_COUNT);
    UNLOAD_SHADERS(prepare.smaaBlendingWeights, R3D_ANTI_ALIASING_PRESET_COUNT);
    UNLOAD_SHADER(prepare.cubemapFromEquirectangular);
    UNLOAD_SHADER(prepare.cubemapIrradiance);
    UNLOAD_SHADER(prepare.cubemapPrefilter);
    UNLOAD_SHADER(prepare.cubemapSkybox);

    UNLOAD_SHADER(scene.geometry);
    UNLOAD_SHADER(scene.forward);
    UNLOAD_SHADER(scene.unlit);
    UNLOAD_SHADER(scene.background);
    UNLOAD_SHADER(scene.skybox);
    UNLOAD_SHADER(scene.depth);
    UNLOAD_SHADER(scene.depthCube);
    UNLOAD_SHADER(scene.probe);
    UNLOAD_SHADER(scene.decal);

    UNLOAD_SHADER(deferred.ambient);
    UNLOAD_SHADER(deferred.lighting);
    UNLOAD_SHADER(deferred.compose);

    UNLOAD_SHADER(post.fog);
    UNLOAD_SHADER(post.bloom);
    UNLOAD_SHADER(post.dof);
    UNLOAD_SHADER(post.output);
    UNLOAD_SHADERS(post.fxaa, R3D_ANTI_ALIASING_PRESET_COUNT);
    UNLOAD_SHADERS(post.smaa, R3D_ANTI_ALIASING_PRESET_COUNT);
    UNLOAD_SHADER(post.visualizer);
}

void r3d_shader_bind_sampler(r3d_shader_sampler_t sampler, GLuint texture)
{
    assert(R3D_MOD_SHADER.samplerTargets[sampler] != GL_NONE);

    if (texture != R3D_MOD_SHADER.samplerBindings[sampler]) {
        glActiveTexture(GL_TEXTURE0 + sampler);
        glBindTexture(R3D_MOD_SHADER.samplerTargets[sampler], texture);
        R3D_MOD_SHADER.samplerBindings[sampler] = texture;
        glActiveTexture(GL_TEXTURE0);
    }
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

bool r3d_shader_set_custom_uniform(r3d_shader_custom_t* shader, const char* name, const void* value)
{
    assert(shader != NULL);

    for (int i = 0; i < R3D_MAX_SHADER_UNIFORMS && shader->uniforms.entries[i].name[0] != '\0'; i++) {
        if (strcmp(shader->uniforms.entries[i].name, name) == 0) {
            int offset = shader->uniforms.entries[i].offset;
            int size = shader->uniforms.entries[i].size;
            memcpy(shader->uniforms.buffer + offset, value, size);
            shader->uniforms.dirty = true;
            return true;
        }
    }
    return false;
}

bool r3d_shader_set_custom_sampler(r3d_shader_custom_t* shader, const char* name, Texture texture)
{
    assert(shader != NULL);

    for (int i = 0; i < R3D_MAX_SHADER_SAMPLERS && shader->samplers[i].name[0] != '\0'; i++) {
        if (strcmp(shader->samplers[i].name, name) == 0) {
            shader->samplers->texture = texture.id;
            return true;
        }
    }
    return false;
}

void r3d_shader_bind_custom_uniforms(r3d_shader_custom_t* shader)
{
    assert(shader != NULL);

    if (shader->uniforms.bufferId == 0) return;

    if (shader->uniforms.dirty) {
        glBindBuffer(GL_UNIFORM_BUFFER, shader->uniforms.bufferId);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, shader->uniforms.bufferSize, shader->uniforms.buffer);
        shader->uniforms.dirty = false;
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, R3D_SHADER_BLOCK_USER_SLOT, shader->uniforms.bufferId);
}

void r3d_shader_bind_custom_samplers(r3d_shader_custom_t* shader)
{
    assert(shader != NULL);

    for (int i = 0; i < R3D_MAX_SHADER_SAMPLERS && shader->samplers[i].name[0] != '\0'; i++)
    {
        r3d_shader_sampler_t sampler = R3D_SHADER_SAMPLER_CUSTOM_2D;

        switch (shader->samplers[i].target) {
        case GL_TEXTURE_1D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_1D;
            break;
        case GL_TEXTURE_2D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_2D;
            break;
        case GL_TEXTURE_3D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_3D;
            break;
        case GL_TEXTURE_CUBE_MAP:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_CUBE;
            break;
        default:
            assert(false);
            break;
        }

        r3d_shader_bind_sampler(sampler + i, shader->samplers[i].texture);
    }
}

void r3d_shader_invalidate_cache(void)
{
    R3D_MOD_SHADER.currentProgram = 0;
    glUseProgram(0);

    for (int iSampler = 0; iSampler < R3D_SHADER_SAMPLER_COUNT; iSampler++) {
        if (R3D_MOD_SHADER.samplerBindings[iSampler] != 0) {
            glActiveTexture(GL_TEXTURE0 + iSampler);
            glBindTexture(R3D_MOD_SHADER.samplerTargets[iSampler], 0);
            R3D_MOD_SHADER.samplerBindings[iSampler] = 0;
        }
    }
    glActiveTexture(GL_TEXTURE0);
}

// ========================================
// INTERNAL FUNCTIONS
// ========================================

char* inject_content(const char* source, const char* content, const char* marker, int mode)
{
    if (!source || !content || !marker) return NULL;

    // Find marker position
    const char* markerPos = strstr(source, marker);
    if (!markerPos) return NULL;

    size_t markerLen = strlen(marker);
    size_t contentLen = strlen(content);
    size_t sourceLen = strlen(source);

    // Calculate new string length
    size_t prefixLen = markerPos - source;
    size_t newLen;

    if (mode == 0) {
        // Replace mode: remove marker
        newLen = sourceLen - markerLen + contentLen;
    } else {
        // Before/after mode: keep marker
        newLen = sourceLen + contentLen;
    }

    // Allocate new string
    char* result = RL_MALLOC(newLen + 1);
    if (!result) return NULL;

    char* ptr = result;

    if (mode < 0) {
        // Insert BEFORE marker: [prefix][content][marker][suffix]
        memcpy(ptr, source, prefixLen);
        ptr += prefixLen;

        memcpy(ptr, content, contentLen);
        ptr += contentLen;
        
        memcpy(ptr, markerPos, sourceLen - prefixLen);
        ptr += sourceLen - prefixLen;
    }
    else if (mode == 0) {
        // REPLACE marker: [prefix][content][suffix]
        memcpy(ptr, source, prefixLen);
        ptr += prefixLen;
        
        memcpy(ptr, content, contentLen);
        ptr += contentLen;
        
        size_t suffixLen = sourceLen - prefixLen - markerLen;
        memcpy(ptr, markerPos + markerLen, suffixLen);
        ptr += suffixLen;
    }
    else {
        // Insert AFTER marker: [prefix][marker][content][suffix]
        size_t upToMarkerEnd = prefixLen + markerLen;
        memcpy(ptr, source, upToMarkerEnd);
        ptr += upToMarkerEnd;
        
        memcpy(ptr, content, contentLen);
        ptr += contentLen;
        
        size_t suffixLen = sourceLen - upToMarkerEnd;
        memcpy(ptr, markerPos + markerLen, suffixLen);
        ptr += suffixLen;
    }
    
    *ptr = '\0';
    return result;
}

char* inject_defines(const char* code, const char* defines[], int count)
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

void inject_user_code(char** vsCode, char** fsCode, const char* userCode)
{
    if (strstr(userCode, "void vertex()") != NULL) {
        char* vsUser = inject_content(*vsCode, userCode, "#define vertex()", 0);
        RL_FREE(*vsCode);
        *vsCode = vsUser;
    }

    if (strstr(userCode, "void fragment()")) {
        char* fsUser = inject_content(*fsCode, userCode, "#define fragment()", 0);
        RL_FREE(*fsCode);
        *fsCode = fsUser;
    }
}

void set_custom_samplers(GLuint id, r3d_shader_custom_t* custom)
{
    for (int i = 0; i < R3D_MAX_SHADER_SAMPLERS && custom->samplers[i].name[0] != '\0'; i++)
    {
        r3d_shader_sampler_t sampler = R3D_SHADER_SAMPLER_CUSTOM_2D;

        switch (custom->samplers[i].target) {
        case GL_TEXTURE_1D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_1D;
            break;
        case GL_TEXTURE_2D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_2D;
            break;
        case GL_TEXTURE_3D:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_3D;
            break;
        case GL_TEXTURE_CUBE_MAP:
            sampler = R3D_SHADER_SAMPLER_CUSTOM_CUBE;
            break;
        default:
            assert(false);
            break;
        }

        GLint loc = glGetUniformLocation(id, custom->samplers[i].name);
        glUniform1i(loc, sampler + i);
    }
}
