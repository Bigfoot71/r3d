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

#include "./r3d_state.h"

#include <assert.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include "./details/misc/r3d_half.h"

/* === Assets === */

#include <assets/brdf_lut_512_rg16_float.raw.h>

/* === Shaders === */

#include <shaders/color.frag.h>
#include <shaders/screen.vert.h>
#include <shaders/cubemap.vert.h>
#include <shaders/ssao.frag.h>
#include <shaders/ssao_blur.frag.h>
#include <shaders/bloom_down.frag.h>
#include <shaders/bloom_up.frag.h>
#include <shaders/cubemap_from_equirectangular.frag.h>
#include <shaders/cubemap_irradiance.frag.h>
#include <shaders/cubemap_prefilter.frag.h>
#include <shaders/geometry.vert.h>
#include <shaders/geometry.frag.h>
#include <shaders/forward.vert.h>
#include <shaders/forward.frag.h>
#include <shaders/skybox.vert.h>
#include <shaders/skybox.frag.h>
#include <shaders/depth_volume.vert.h>
#include <shaders/depth_volume.frag.h>
#include <shaders/depth.vert.h>
#include <shaders/depth.frag.h>
#include <shaders/depth_cube.vert.h>
#include <shaders/depth_cube.frag.h>
#include <shaders/decal.vert.h>
#include <shaders/decal.frag.h>
#include <shaders/ambient.frag.h>
#include <shaders/lighting.frag.h>
#include <shaders/compose.frag.h>
#include <shaders/bloom.frag.h>
#include <shaders/ssr.frag.h>
#include <shaders/fog.frag.h>
#include <shaders/dof.frag.h>
#include <shaders/output.frag.h>
#include <shaders/fxaa.frag.h>

/* === Global state definition === */

struct R3D_State R3D = { 0 };

/* === Internal Functions === */

static char* r3d_shader_inject_defines(const char* code, const char* defines[], int count)
{
    if (!code) return NULL;

    // Calculate the size of the final buffer
    size_t codeLen = strlen(code);
    size_t definesLen = 0;

    // Calculate the total size of the #define statements
    for (int i = 0; i < count; i++) {
        definesLen += strlen(defines[i]) + 1;  // +1 for '\n'
    }

    // Allocate memory for the new shader
    size_t newSize = codeLen + definesLen + 1;
    char* newShader = (char*)RL_MALLOC(newSize);
    if (!newShader) return NULL;

    const char* versionStart = strstr(code, "#version");
    assert(versionStart && "Shader must have version");

    // Copy everything up to the end of the `#version` line
    const char* afterVersion = strchr(versionStart, '\n');
    if (!afterVersion) afterVersion = versionStart + strlen(versionStart);

    size_t prefix_len = afterVersion - code + 1;
    strncpy(newShader, code, prefix_len);
    newShader[prefix_len] = '\0';

    // Add the `#define` statements
    for (int i = 0; i < count; i++) {
        strcat(newShader, defines[i]);
        strcat(newShader, "\n");
    }

    // Add the rest of the shader after `#version`
    strcat(newShader, afterVersion + 1);

    return newShader;
}

// Test if a format can be used as internal format and framebuffer attachment
static struct r3d_support_internal_format
r3d_test_internal_format(GLuint fbo, GLuint tex, GLenum internalFormat, GLenum format, GLenum type)
{
    struct r3d_support_internal_format result = { 0 };

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 4, 4, 0, format, type, NULL);

    result.internal = (glGetError() == GL_NO_ERROR);
    if (!result.internal) return result;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    result.attachment = (status == GL_FRAMEBUFFER_COMPLETE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    return result;
}

/* === Helper functions === */

bool r3d_texture_is_default(GLuint id)
{
    for (int i = 0; i < sizeof(R3D.texture) / sizeof(GLuint); i++) {
        if (id == ((GLuint*)(&R3D.texture))[i]) {
            return true;
        }
    }

    return false;
}

void r3d_calculate_bloom_prefilter_data(void)
{
    float knee = R3D.env.bloomThreshold * R3D.env.bloomSoftThreshold;
    R3D.env.bloomPrefilter.x = R3D.env.bloomThreshold;
    R3D.env.bloomPrefilter.y = R3D.env.bloomPrefilter.x - knee;
    R3D.env.bloomPrefilter.z = 2.0f * knee;
    R3D.env.bloomPrefilter.w = 0.25f / (knee + 0.00001f);
}

/* === Support functions === */

// Returns the best format in case of incompatibility
GLenum r3d_support_get_internal_format(GLenum internalFormat, bool asAttachment)
{
    // Macro to simplify the definition of supports
    #define SUPPORT(fmt) { GL_##fmt, &R3D.support.fmt, #fmt }
    #define END_ALTERNATIVES { GL_NONE, NULL, NULL }

    // Structure for defining format alternatives
    struct format_info {
        GLenum format;
        const struct r3d_support_internal_format* support;
        const char* name;
    };

    // Structure for defining fallbacks of a format
    struct format_fallback {
        GLenum requestedInternalFormat;
        struct format_info alternatives[8];
    };

    // Table of fallbacks for each format
    static const struct format_fallback fallbacks[] =
    {
        // Single Channel Formats
        { GL_R8, {
            SUPPORT(R8),
            END_ALTERNATIVES
        }},
        { GL_R16F, {
            SUPPORT(R16F),
            SUPPORT(R32F),
            SUPPORT(R8),
            END_ALTERNATIVES
        }},
        { GL_R32F, {
            SUPPORT(R32F),
            SUPPORT(R16F),
            SUPPORT(R8),
            END_ALTERNATIVES
        }},

        // Dual Channel Formats
        { GL_RG8, {
            SUPPORT(RG8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RG16F, {
            SUPPORT(RG16F),
            SUPPORT(RG32F),
            SUPPORT(RGBA16F),
            SUPPORT(RG8),
            END_ALTERNATIVES
        }},
        { GL_RG32F, {
            SUPPORT(RG32F),
            SUPPORT(RG16F),
            SUPPORT(RGBA32F),
            SUPPORT(RG8),
            END_ALTERNATIVES
        }},
        
        // Triple Channel Formats (RGB)
        { GL_RGB565, {
            SUPPORT(RGB565),
            SUPPORT(RGB8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGB8, {
            SUPPORT(RGB8),
            SUPPORT(SRGB8),
            SUPPORT(RGBA8),
            SUPPORT(RGB565),
            END_ALTERNATIVES
        }},
        { GL_SRGB8, {
            SUPPORT(SRGB8),
            SUPPORT(RGB8),
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGB12, {
            SUPPORT(RGB12),
            SUPPORT(RGB16),
            SUPPORT(RGBA12),
            SUPPORT(RGB8),
            END_ALTERNATIVES
        }},
        { GL_RGB16, {
            SUPPORT(RGB16),
            SUPPORT(RGB12),
            SUPPORT(RGBA16),
            SUPPORT(RGB8),
            END_ALTERNATIVES
        }},
        { GL_RGB9_E5, {
            SUPPORT(RGB9_E5),
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            END_ALTERNATIVES
        }},
        { GL_R11F_G11F_B10F, {
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB9_E5),
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            END_ALTERNATIVES
        }},
        { GL_RGB16F, {
            SUPPORT(RGB16F),
            SUPPORT(RGB32F),
            SUPPORT(RGBA16F),
            SUPPORT(R11F_G11F_B10F),
            SUPPORT(RGB9_E5),
            END_ALTERNATIVES
        }},
        { GL_RGB32F, {
            SUPPORT(RGB32F),
            SUPPORT(RGB16F),
            SUPPORT(RGBA32F),
            SUPPORT(R11F_G11F_B10F),
            END_ALTERNATIVES
        }},
        
        // Quad Channel Formats (RGBA)
        { GL_RGBA4, {
            SUPPORT(RGBA4),
            SUPPORT(RGB5_A1),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGB5_A1, {
            SUPPORT(RGB5_A1),
            SUPPORT(RGBA4),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGBA8, {
            SUPPORT(RGBA8),
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGB10_A2),
            SUPPORT(RGB5_A1),
            END_ALTERNATIVES
        }},
        { GL_SRGB8_ALPHA8, {
            SUPPORT(SRGB8_ALPHA8),
            SUPPORT(RGBA8),
            SUPPORT(SRGB8),
            END_ALTERNATIVES
        }},
        { GL_RGB10_A2, {
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA16),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGBA12, {
            SUPPORT(RGBA12),
            SUPPORT(RGBA16),
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGBA16, {
            SUPPORT(RGBA16),
            SUPPORT(RGBA12),
            SUPPORT(RGB10_A2),
            SUPPORT(RGBA8),
            END_ALTERNATIVES
        }},
        { GL_RGBA16F, {
            SUPPORT(RGBA16F),
            SUPPORT(RGBA32F),
            SUPPORT(RGB16F),
            SUPPORT(RGB10_A2),
            END_ALTERNATIVES
        }},
        { GL_RGBA32F, {
            SUPPORT(RGBA32F),
            SUPPORT(RGBA16F),
            SUPPORT(RGB32F),
            SUPPORT(RGB10_A2),
            END_ALTERNATIVES
        }},

        // Sentinel
        { GL_NONE, { END_ALTERNATIVES } }
    };

    // Search for format in table
    for (const struct format_fallback* fallback = fallbacks; fallback->requestedInternalFormat != GL_NONE; fallback++) {
        if (fallback->requestedInternalFormat == internalFormat) {
            for (int i = 0; fallback->alternatives[i].format != GL_NONE; i++) {
                const struct format_info* alt = &fallback->alternatives[i];
                if ((asAttachment && alt->support->attachment) || (!asAttachment && alt->support->internal)) {
                    if (i > 0) TraceLog(LOG_WARNING, "R3D: %s not supported, using %s instead", fallback->alternatives[0].name, alt->name);
                    return alt->format;
                }
            }

            // No alternatives found
            TraceLog(LOG_FATAL, "R3D: Texture format %s is not supported and no fallback could be found", fallback->alternatives[0].name);
            return GL_NONE;
        }
    }

    // Unknown format...
    assert(false && "Unknown or unsupported texture format requested");
    return GL_NONE;

    #undef SUPPORT
    #undef END_ALTERNATIVES
}

/* === Storage functions === */

void r3d_storage_bind_and_upload_matrices(const Matrix* matrices, int count, int slot)
{
    assert(count <= R3D_STORAGE_MATRIX_CAPACITY);

    static const int texCount = sizeof(R3D.storage.texMatrices) / sizeof(*R3D.storage.texMatrices);
    static int texIndex = 0;

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_1D, R3D.storage.texMatrices[texIndex]);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4 * count, GL_RGBA, GL_FLOAT, matrices);

    texIndex = (texIndex + 1) % texCount;
}

/* === Main loading functions === */

void r3d_supports_check(void)
{
    memset(&R3D.support, 0, sizeof(R3D.support));

    /* --- Generate objects only once for all tests --- */

    GLuint fbo, tex;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);

    /* --- Test each internal format and framebuffer attachment --- */

    struct probe {
        GLenum internal;
        GLenum format;
        GLenum type;
        struct r3d_support_internal_format* outFlag;
        const char* name;
    } probes[] = {
        // Single Channel Formats
        { GL_R8,                 GL_RED,   GL_UNSIGNED_BYTE,                &R3D.support.R8,              "R8" },
        { GL_R16F,               GL_RED,   GL_HALF_FLOAT,                   &R3D.support.R16F,            "R16F" },
        { GL_R32F,               GL_RED,   GL_FLOAT,                        &R3D.support.R32F,            "R32F" },

        // Dual Channel Formats
        { GL_RG8,                GL_RG,    GL_UNSIGNED_BYTE,                &R3D.support.RG8,             "RG8" },
        { GL_RG16F,              GL_RG,    GL_HALF_FLOAT,                   &R3D.support.RG16F,           "RG16F" },
        { GL_RG32F,              GL_RG,    GL_FLOAT,                        &R3D.support.RG32F,           "RG32F" },

        // Triple Channel Formats (RGB)
        { GL_RGB565,             GL_RGB,   GL_UNSIGNED_SHORT_5_6_5,         &R3D.support.RGB565,          "RGB565" },
        { GL_RGB8,               GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.RGB8,            "RGB8" },
        { GL_SRGB8,              GL_RGB,   GL_UNSIGNED_BYTE,                &R3D.support.SRGB8,           "SRGB8" },
        { GL_RGB12,              GL_RGB,   GL_UNSIGNED_SHORT,               &R3D.support.RGB12,           "RGB12" },
        { GL_RGB16,              GL_RGB,   GL_UNSIGNED_SHORT,               &R3D.support.RGB16,           "RGB16" },
        { GL_RGB9_E5,            GL_RGB,   GL_UNSIGNED_INT_5_9_9_9_REV,     &R3D.support.RGB9_E5,         "RGB9_E5" },
        { GL_R11F_G11F_B10F,     GL_RGB,   GL_UNSIGNED_INT_10F_11F_11F_REV, &R3D.support.R11F_G11F_B10F,  "R11F_G11F_B10F" },
        { GL_RGB16F,             GL_RGB,   GL_HALF_FLOAT,                   &R3D.support.RGB16F,          "RGB16F" },
        { GL_RGB32F,             GL_RGB,   GL_FLOAT,                        &R3D.support.RGB32F,          "RGB32F" },

        // Quad Channel Formats (RGBA)
        { GL_RGBA4,              GL_RGBA,  GL_UNSIGNED_SHORT_4_4_4_4,       &R3D.support.RGBA4,           "RGBA4" },
        { GL_RGB5_A1,            GL_RGBA,  GL_UNSIGNED_SHORT_5_5_5_1,       &R3D.support.RGB5_A1,         "RGB5_A1" },
        { GL_RGBA8,              GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.RGBA8,           "RGBA8" },
        { GL_SRGB8_ALPHA8,       GL_RGBA,  GL_UNSIGNED_BYTE,                &R3D.support.SRGB8_ALPHA8,    "SRGB8_ALPHA8" },
        { GL_RGB10_A2,           GL_RGBA,  GL_UNSIGNED_INT_10_10_10_2,      &R3D.support.RGB10_A2,        "RGB10_A2" },
        { GL_RGBA12,             GL_RGBA,  GL_UNSIGNED_SHORT,               &R3D.support.RGBA12,          "RGBA12" },
        { GL_RGBA16,             GL_RGBA,  GL_UNSIGNED_SHORT,               &R3D.support.RGBA16,          "RGBA16" },
        { GL_RGBA16F,            GL_RGBA,  GL_HALF_FLOAT,                   &R3D.support.RGBA16F,         "RGBA16F" },
        { GL_RGBA32F,            GL_RGBA,  GL_FLOAT,                        &R3D.support.RGBA32F,         "RGBA32F" },
    };

    for (int i = 0; i < (int)(sizeof(probes)/sizeof(probes[0])); ++i) {
        *probes[i].outFlag = r3d_test_internal_format(fbo, tex, probes[i].internal, probes[i].format, probes[i].type);
        if (!probes[i].outFlag->internal) {
            TraceLog(LOG_WARNING, "R3D: Texture format %s is not supported", probes[i].name);
        }
        if (!probes[i].outFlag->attachment) {
            TraceLog(LOG_WARNING, "R3D: Texture format %s cannot be used as a color attachment", probes[i].name);
        }
    }

    /* --- Clean up objects and residual errors --- */

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glGetError();
}

void r3d_framebuffers_load(int width, int height)
{
    r3d_framebuffer_load_gbuffer(width, height);
    r3d_framebuffer_load_deferred(width, height);
    r3d_framebuffer_load_scene(width, height);

    if (R3D.env.ssaoEnabled) {
        r3d_framebuffer_load_ssao(width, height);
    }

    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_framebuffer_load_bloom(width, height);
    }
}

void r3d_framebuffers_unload(void)
{
    /* --- Unload framebuffers --- */

    if (R3D.framebuffer.gBuffer > 0) {
        glDeleteFramebuffers(1, &R3D.framebuffer.gBuffer);
    }
    if (R3D.framebuffer.deferred > 0) {
        glDeleteFramebuffers(1, &R3D.framebuffer.deferred);
    }
    if (R3D.framebuffer.scene > 0) {
        glDeleteFramebuffers(1, &R3D.framebuffer.scene);
    }
    if (R3D.framebuffer.ssao > 0) {
        glDeleteFramebuffers(1, &R3D.framebuffer.ssao);
    }
    if (R3D.framebuffer.bloom > 0) {
        glDeleteFramebuffers(1, &R3D.framebuffer.bloom);
    }

    memset(&R3D.framebuffer, 0, sizeof(R3D.framebuffer));

    /* --- Unload targets --- */

    if (R3D.target.albedo > 0) {
        glDeleteTextures(1, &R3D.target.albedo);
    }
    if (R3D.target.emission > 0) {
        glDeleteTextures(1, &R3D.target.emission);
    }
    if (R3D.target.normal > 0) {
        glDeleteTextures(1, &R3D.target.normal);
    }
    if (R3D.target.orm > 0) {
        glDeleteTextures(1, &R3D.target.orm);
    }
    if (R3D.target.depth > 0) {
        glDeleteTextures(1, &R3D.target.depth);
    }
    if (R3D.target.diffuse > 0) {
        glDeleteTextures(1, &R3D.target.diffuse);
    }
    if (R3D.target.specular > 0) {
        glDeleteTextures(1, &R3D.target.specular);
    }
    if (R3D.target.ssaoPpHs[0] > 0) {
        glDeleteTextures(2, R3D.target.ssaoPpHs);
    }
    if (R3D.target.scenePp[0] > 0) {
        glDeleteTextures(2, R3D.target.scenePp);
    }
    if (R3D.target.mipChainHs.chain != NULL) {
        r3d_target_unload_mip_chain_hs();
    }

    memset(&R3D.target, 0, sizeof(R3D.target));
}

void r3d_textures_load(void)
{
    r3d_texture_load_white();
    r3d_texture_load_black();
    r3d_texture_load_normal();
    r3d_texture_load_ibl_brdf_lut();

    if (R3D.env.ssaoEnabled) {
        r3d_texture_load_ssao_noise();
        r3d_texture_load_ssao_kernel();
    }
}

void r3d_textures_unload(void)
{
    rlUnloadTexture(R3D.texture.white);
    rlUnloadTexture(R3D.texture.black);
    rlUnloadTexture(R3D.texture.normal);
    rlUnloadTexture(R3D.texture.iblBrdfLut);

    if (R3D.texture.ssaoNoise != 0) {
        rlUnloadTexture(R3D.texture.ssaoNoise);
    }

    if (R3D.texture.ssaoKernel != 0) {
        rlUnloadTexture(R3D.texture.ssaoKernel);
    }
}

void r3d_storages_load(void)
{
    r3d_storage_load_tex_matrices();
}

void r3d_storages_unload(void)
{
    if (R3D.storage.texMatrices[0] != 0) {
        static const int count = sizeof(R3D.storage.texMatrices) / sizeof(*R3D.storage.texMatrices);
        glDeleteTextures(count, R3D.storage.texMatrices);
    }
}

void r3d_shaders_load(void)
{
    /* --- Generation shader passes --- */

    r3d_shader_load_prepare_cubemap_from_equirectangular();
    r3d_shader_load_prepare_cubemap_irradiance();
    r3d_shader_load_prepare_cubemap_prefilter();

    /* --- Scene shader passes --- */

    r3d_shader_load_scene_geometry();
    r3d_shader_load_scene_forward();
    r3d_shader_load_scene_decal();
    r3d_shader_load_scene_background();
    r3d_shader_load_scene_skybox();
    r3d_shader_load_scene_depth_volume();
    r3d_shader_load_scene_depth();
    r3d_shader_load_scene_depth_cube();

    /* --- deferred shader passes --- */

    r3d_shader_load_deferred_ambient_ibl();
    r3d_shader_load_deferred_ambient();
    r3d_shader_load_deferred_lighting();
    r3d_shader_load_deferred_compose();

    // NOTE: Don't load the output shader here to avoid keeping an unused tonemap mode
    //       in memory in case the tonemap mode changes after initialization.
    //       It is loaded on demand during `R3D_End()`

    // TODO: Revisit the shader loading mechanism. Constantly checking and loading
    //       it during `R3D_End()` doesn't feel like the cleanest approach

    //r3d_shader_load_post_output(R3D.env.tonemapMode);

    /* --- Additional screen shader passes --- */

    if (R3D.env.ssaoEnabled) {
        r3d_shader_load_prepare_ssao_blur();
        r3d_shader_load_prepare_ssao();
    }
    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_shader_load_prepare_bloom_down();
        r3d_shader_load_prepare_bloom_up();
        r3d_shader_load_post_bloom();
    }
    if (R3D.env.ssrEnabled) {
        r3d_shader_load_post_ssr();
    }
    if (R3D.env.fogMode != R3D_FOG_DISABLED) {
        r3d_shader_load_post_fog();
    }
    if (R3D.env.dofMode != R3D_DOF_DISABLED) {
        r3d_shader_load_post_dof();
    }
    if (R3D.state.flags & R3D_FLAG_FXAA) {
        r3d_shader_load_post_fxaa();
    }
}

void r3d_shaders_unload(void)
{
    // Unload prepare shaders
    if (R3D.shader.prepare.ssao.id != 0) {
        rlUnloadShaderProgram(R3D.shader.prepare.ssao.id);
    }
    if (R3D.shader.prepare.ssaoBlur.id != 0) {
        rlUnloadShaderProgram(R3D.shader.prepare.ssaoBlur.id);
    }
    if (R3D.shader.prepare.bloomDown.id != 0) {
        rlUnloadShaderProgram(R3D.shader.prepare.bloomDown.id);
    }
    if (R3D.shader.prepare.bloomUp.id != 0) {
        rlUnloadShaderProgram(R3D.shader.prepare.bloomUp.id);
    }
    rlUnloadShaderProgram(R3D.shader.prepare.cubemapFromEquirectangular.id);
    rlUnloadShaderProgram(R3D.shader.prepare.cubemapIrradiance.id);
    rlUnloadShaderProgram(R3D.shader.prepare.cubemapPrefilter.id);

    // Unload scene shaders
    rlUnloadShaderProgram(R3D.shader.scene.geometry.id);
    rlUnloadShaderProgram(R3D.shader.scene.forward.id);
    rlUnloadShaderProgram(R3D.shader.scene.decal.id);
    rlUnloadShaderProgram(R3D.shader.scene.background.id);
    rlUnloadShaderProgram(R3D.shader.scene.skybox.id);
    rlUnloadShaderProgram(R3D.shader.scene.depthVolume.id);
    rlUnloadShaderProgram(R3D.shader.scene.depth.id);
    rlUnloadShaderProgram(R3D.shader.scene.depthCube.id);

    // Unload deferred shaders
    rlUnloadShaderProgram(R3D.shader.deferred.ambientIbl.id);
    rlUnloadShaderProgram(R3D.shader.deferred.ambient.id);
    rlUnloadShaderProgram(R3D.shader.deferred.lighting.id);
    rlUnloadShaderProgram(R3D.shader.deferred.compose.id);

    // Unload post shaders
    for (int i = 0; i < R3D_TONEMAP_COUNT; i++) {
        if (R3D.shader.post.output[i].id != 0) {
            rlUnloadShaderProgram(R3D.shader.post.output[i].id);
        }
    }
    if (R3D.shader.post.bloom.id != 0) {
        rlUnloadShaderProgram(R3D.shader.post.bloom.id);
    }
    if (R3D.shader.post.ssr.id != 0) {
        rlUnloadShaderProgram(R3D.shader.post.ssr.id);
    }
    if (R3D.shader.post.fog.id != 0) {
        rlUnloadShaderProgram(R3D.shader.post.fog.id);
    }
    if (R3D.shader.post.dof.id != 0) {
        rlUnloadShaderProgram(R3D.shader.post.dof.id);
    }
    if (R3D.shader.post.fxaa.id != 0) {
        rlUnloadShaderProgram(R3D.shader.post.fxaa.id);
    }
}

/* === Target loading functions === */

static void r3d_target_load_albedo(int width, int height)
{
    assert(R3D.target.albedo == 0);

    glGenTextures(1, &R3D.target.albedo);
    glBindTexture(GL_TEXTURE_2D, R3D.target.albedo);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_emission(int width, int height)
{
    assert(R3D.target.emission == 0);

    GLenum internalFormat = r3d_support_get_internal_format(GL_R11F_G11F_B10F, true);

    glGenTextures(1, &R3D.target.emission);
    glBindTexture(GL_TEXTURE_2D, R3D.target.emission);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_normal(int width, int height)
{
    assert(R3D.target.normal == 0);

    glGenTextures(1, &R3D.target.normal);
    glBindTexture(GL_TEXTURE_2D, R3D.target.normal);

    if ((R3D.state.flags & R3D_FLAG_8_BIT_NORMALS) || !R3D.support.RG16F.attachment) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, width, height, 0, GL_RG, GL_UNSIGNED_BYTE, NULL);
    }
    else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_FLOAT, NULL);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_orm(int width, int height)
{
    assert(R3D.target.orm == 0);

    glGenTextures(1, &R3D.target.orm);
    glBindTexture(GL_TEXTURE_2D, R3D.target.orm);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_depth(int width, int height)
{
    assert(R3D.target.depth == 0);

    glGenTextures(1, &R3D.target.depth);
    glBindTexture(GL_TEXTURE_2D, R3D.target.depth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_diffuse(int width, int height)
{
    assert(R3D.target.diffuse == 0);

    GLenum internalFormat;
    if (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        internalFormat = r3d_support_get_internal_format(GL_R11F_G11F_B10F, true);
    }
    else {
        internalFormat = r3d_support_get_internal_format(GL_RGB16F, true);
    }

    glGenTextures(1, &R3D.target.diffuse);
    glBindTexture(GL_TEXTURE_2D, R3D.target.diffuse);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_specular(int width, int height)
{
    assert(R3D.target.specular == 0);

    GLenum internalFormat;
    if (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        internalFormat = r3d_support_get_internal_format(GL_R11F_G11F_B10F, true);
    }
    else {
        internalFormat = r3d_support_get_internal_format(GL_RGB16F, true);
    }

    glGenTextures(1, &R3D.target.specular);
    glBindTexture(GL_TEXTURE_2D, R3D.target.specular);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_ssao_pp_hs(int width, int height)
{
    assert(R3D.target.ssaoPpHs[0] == 0);

    width /= 2, height /= 2;

    glGenTextures(2, R3D.target.ssaoPpHs);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, R3D.target.ssaoPpHs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

static void r3d_target_load_scene_pp(int width, int height)
{
    assert(R3D.target.scenePp[0] == 0);

    GLenum internalFormat;
    if (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        internalFormat = r3d_support_get_internal_format(GL_R11F_G11F_B10F, true);
    }
    else {
        internalFormat = r3d_support_get_internal_format(GL_RGB16F, true);
    }

    glGenTextures(2, R3D.target.scenePp);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, R3D.target.scenePp[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void r3d_target_load_mip_chain_hs(int width, int height, int count)
{
    assert(R3D.target.mipChainHs.chain == NULL);

    width /= 2, height /= 2; // Half resolution

    GLenum internalFormat;
    if (R3D.state.flags & R3D_FLAG_LOW_PRECISION_BUFFERS) {
        internalFormat = r3d_support_get_internal_format(GL_R11F_G11F_B10F, true);
    }
    else {
        internalFormat = r3d_support_get_internal_format(GL_RGB16F, true);
    }

    // Calculate the maximum mip levels based on smallest dimension
    int maxDimension = (width > height) ? width : height;
    int maxLevels = 1 + (int)floorf(log2f((float)maxDimension));

    // Use maximum level if count is too large or not specified
    if (count <= 0 || count > maxLevels) {
        R3D.target.mipChainHs.count = maxLevels;
    }
    else {
        R3D.target.mipChainHs.count = count;
    }

    // Allocate the array containing the mipmaps
    R3D.target.mipChainHs.chain = RL_MALLOC(R3D.target.mipChainHs.count * sizeof(struct r3d_mip));
    if (R3D.target.mipChainHs.chain == NULL) {
        TraceLog(LOG_ERROR, "R3D: Failed to allocate memory to store mip chain");
        return;
    }

    // Dynamic value copy
    uint32_t wMip = (uint32_t)width;
    uint32_t hMip = (uint32_t)height;

    // Create the mip chain
    for (int i = 0; i < R3D.target.mipChainHs.count; i++, wMip /= 2, hMip /= 2) {
        struct r3d_mip* mip = &R3D.target.mipChainHs.chain[i];

        mip->w = wMip;
        mip->h = hMip;
        mip->tx = 1.0f / (float)wMip;
        mip->ty = 1.0f / (float)hMip;

        glGenTextures(1, &mip->id);
        glBindTexture(GL_TEXTURE_2D, mip->id);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, wMip, hMip, 0, GL_RGB, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

/* === Target unloading functions === */

void r3d_target_unload_mip_chain_hs(void)
{
    assert(R3D.target.mipChainHs.chain != NULL);
    for (int i = 0; i < R3D.target.mipChainHs.count; i++) {
        glDeleteTextures(1, &R3D.target.mipChainHs.chain[i].id);
    }
    RL_FREE(R3D.target.mipChainHs.chain);
    R3D.target.mipChainHs.chain = NULL;
}

/* === Framebuffer loading functions === */

void r3d_framebuffer_load_gbuffer(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (!R3D.target.albedo)     r3d_target_load_albedo(width, height);
    if (!R3D.target.emission)   r3d_target_load_emission(width, height);
    if (!R3D.target.normal)     r3d_target_load_normal(width, height);
    if (!R3D.target.orm)        r3d_target_load_orm(width, height);
    if (!R3D.target.depth)      r3d_target_load_depth(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.gBuffer);

    glDrawBuffers(4, (GLenum[]) {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.albedo, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, R3D.target.emission, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, R3D.target.normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, R3D.target.orm, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depth, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The G-Buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_ssao(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (!R3D.target.ssaoPpHs[0]) r3d_target_load_ssao_pp_hs(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.ssao);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.ssao);

    glDrawBuffers(1, (GLenum[]) { GL_COLOR_ATTACHMENT0 });
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.ssaoPpHs[0], 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The SSAO ping-pong buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_deferred(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (!R3D.target.diffuse)    r3d_target_load_diffuse(width, height);
    if (!R3D.target.specular)   r3d_target_load_specular(width, height);
    if (!R3D.target.depth)      r3d_target_load_depth(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.deferred);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.deferred);

    glDrawBuffers(2, (GLenum[]) {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.diffuse, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, R3D.target.specular, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depth, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The deferred buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_bloom(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (R3D.target.mipChainHs.chain == NULL) {
        r3d_target_load_mip_chain_hs(width, height, R3D.env.bloomLevels);
    }

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.bloom);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.bloom);

    glDrawBuffers(1, (GLenum[]) {
        GL_COLOR_ATTACHMENT0
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.mipChainHs.chain[0].id, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The bloom buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_scene(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (!R3D.target.scenePp[0]) r3d_target_load_scene_pp(width, height);
    if (!R3D.target.albedo)     r3d_target_load_albedo(width, height);
    if (!R3D.target.normal)     r3d_target_load_normal(width, height);
    if (!R3D.target.orm)        r3d_target_load_orm(width, height);
    if (!R3D.target.depth)      r3d_target_load_depth(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.scene);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.scene);

    // By default, only attachment 0 (the ping-pong buffer) is enabled.
    // The additional attachments 'normal' and 'orm' will only be enabled
    // when needed, for example during forward rendering.
    glDrawBuffers(1, (GLenum[]) {
        GL_COLOR_ATTACHMENT0
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.scenePp[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, R3D.target.albedo, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, R3D.target.normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, R3D.target.orm, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depth, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The deferred buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* === Shader loading functions === */

#define R3D_SHADER_VALIDATION(program) do { \
    if (R3D.shader.program.id == 0) { \
        TraceLog(LOG_ERROR, "R3D: Failed to validate '" #program "'"); \
        return; \
    } \
} while(0) \

void r3d_shader_load_prepare_ssao(void)
{
    R3D.shader.prepare.ssao.id = rlLoadShaderCode(
        SCREEN_VERT, SSAO_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.ssao);

    r3d_shader_get_location(prepare.ssao, uTexDepth);
    r3d_shader_get_location(prepare.ssao, uTexNormal);
    r3d_shader_get_location(prepare.ssao, uTexKernel);
    r3d_shader_get_location(prepare.ssao, uTexNoise);
    r3d_shader_get_location(prepare.ssao, uMatInvProj);
    r3d_shader_get_location(prepare.ssao, uMatProj);
    r3d_shader_get_location(prepare.ssao, uMatView);
    r3d_shader_get_location(prepare.ssao, uRadius);
    r3d_shader_get_location(prepare.ssao, uBias);
    r3d_shader_get_location(prepare.ssao, uIntensity);

    r3d_shader_enable(prepare.ssao);
    r3d_shader_set_sampler2D_slot(prepare.ssao, uTexDepth, 0);
    r3d_shader_set_sampler2D_slot(prepare.ssao, uTexNormal, 1);
    r3d_shader_set_sampler1D_slot(prepare.ssao, uTexKernel, 2);
    r3d_shader_set_sampler2D_slot(prepare.ssao, uTexNoise, 3);
    r3d_shader_disable();
}
    
void r3d_shader_load_prepare_ssao_blur(void)
{
    R3D.shader.prepare.ssaoBlur.id = rlLoadShaderCode(
        SCREEN_VERT, SSAO_BLUR_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.ssaoBlur);

    r3d_shader_get_location(prepare.ssaoBlur, uTexOcclusion);
    r3d_shader_get_location(prepare.ssaoBlur, uTexNormal);
    r3d_shader_get_location(prepare.ssaoBlur, uTexDepth);
    r3d_shader_get_location(prepare.ssaoBlur, uMatInvProj);
    r3d_shader_get_location(prepare.ssaoBlur, uDirection);

    r3d_shader_enable(prepare.ssaoBlur);
    r3d_shader_set_sampler2D_slot(prepare.ssaoBlur, uTexOcclusion, 0);
    r3d_shader_set_sampler2D_slot(prepare.ssaoBlur, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(prepare.ssaoBlur, uTexDepth, 2);
    r3d_shader_disable();
}

void r3d_shader_load_prepare_bloom_down(void)
{
    R3D.shader.prepare.bloomDown.id = rlLoadShaderCode(
        SCREEN_VERT, BLOOM_DOWN_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.bloomDown);

    r3d_shader_get_location(prepare.bloomDown, uTexture);
    r3d_shader_get_location(prepare.bloomDown, uTexelSize);
    r3d_shader_get_location(prepare.bloomDown, uMipLevel);
    r3d_shader_get_location(prepare.bloomDown, uPrefilter);

    r3d_shader_enable(prepare.bloomDown);
    r3d_shader_set_sampler2D_slot(prepare.bloomDown, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_prepare_bloom_up(void)
{
    R3D.shader.prepare.bloomUp.id = rlLoadShaderCode(
        SCREEN_VERT, BLOOM_UP_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.bloomUp);

    r3d_shader_get_location(prepare.bloomUp, uTexture);
    r3d_shader_get_location(prepare.bloomUp, uFilterRadius);

    r3d_shader_enable(prepare.bloomUp);
    r3d_shader_set_sampler2D_slot(prepare.bloomUp, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_prepare_cubemap_from_equirectangular(void)
{
    R3D.shader.prepare.cubemapFromEquirectangular.id = rlLoadShaderCode(
        CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.cubemapFromEquirectangular);

    r3d_shader_get_location(prepare.cubemapFromEquirectangular, uMatProj);
    r3d_shader_get_location(prepare.cubemapFromEquirectangular, uMatView);
    r3d_shader_get_location(prepare.cubemapFromEquirectangular, uTexEquirectangular);

    r3d_shader_enable(prepare.cubemapFromEquirectangular);
    r3d_shader_set_sampler2D_slot(prepare.cubemapFromEquirectangular, uTexEquirectangular, 0);
    r3d_shader_disable();
}

void r3d_shader_load_prepare_cubemap_irradiance(void)
{
    R3D.shader.prepare.cubemapIrradiance.id = rlLoadShaderCode(
        CUBEMAP_VERT, CUBEMAP_IRRADIANCE_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.cubemapIrradiance);

    r3d_shader_get_location(prepare.cubemapIrradiance, uMatProj);
    r3d_shader_get_location(prepare.cubemapIrradiance, uMatView);
    r3d_shader_get_location(prepare.cubemapIrradiance, uCubemap);

    r3d_shader_enable(prepare.cubemapIrradiance);
    r3d_shader_set_samplerCube_slot(prepare.cubemapIrradiance, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_prepare_cubemap_prefilter(void)
{
    R3D.shader.prepare.cubemapPrefilter.id = rlLoadShaderCode(
        CUBEMAP_VERT, CUBEMAP_PREFILTER_FRAG
    );

    R3D_SHADER_VALIDATION(prepare.cubemapPrefilter);

    r3d_shader_get_location(prepare.cubemapPrefilter, uMatProj);
    r3d_shader_get_location(prepare.cubemapPrefilter, uMatView);
    r3d_shader_get_location(prepare.cubemapPrefilter, uCubemap);
    r3d_shader_get_location(prepare.cubemapPrefilter, uResolution);
    r3d_shader_get_location(prepare.cubemapPrefilter, uRoughness);

    r3d_shader_enable(prepare.cubemapPrefilter);
    r3d_shader_set_samplerCube_slot(prepare.cubemapPrefilter, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_scene_geometry(void)
{
    R3D.shader.scene.geometry.id = rlLoadShaderCode(
        GEOMETRY_VERT, GEOMETRY_FRAG
    );

    R3D_SHADER_VALIDATION(scene.geometry);

    r3d_shader_get_location(scene.geometry, uTexBoneMatrices);
    r3d_shader_get_location(scene.geometry, uMatInvView);
    r3d_shader_get_location(scene.geometry, uMatNormal);
    r3d_shader_get_location(scene.geometry, uMatModel);
    r3d_shader_get_location(scene.geometry, uMatVP);
    r3d_shader_get_location(scene.geometry, uAlbedoColor);
    r3d_shader_get_location(scene.geometry, uEmissionEnergy);
    r3d_shader_get_location(scene.geometry, uEmissionColor);
    r3d_shader_get_location(scene.geometry, uTexCoordOffset);
    r3d_shader_get_location(scene.geometry, uTexCoordScale);
    r3d_shader_get_location(scene.geometry, uInstancing);
    r3d_shader_get_location(scene.geometry, uSkinning);
    r3d_shader_get_location(scene.geometry, uBillboard);
    r3d_shader_get_location(scene.geometry, uTexAlbedo);
    r3d_shader_get_location(scene.geometry, uTexNormal);
    r3d_shader_get_location(scene.geometry, uTexEmission);
    r3d_shader_get_location(scene.geometry, uTexORM);
    r3d_shader_get_location(scene.geometry, uAlphaCutoff);
    r3d_shader_get_location(scene.geometry, uNormalScale);
    r3d_shader_get_location(scene.geometry, uOcclusion);
    r3d_shader_get_location(scene.geometry, uRoughness);
    r3d_shader_get_location(scene.geometry, uMetalness);

    r3d_shader_enable(scene.geometry);
    r3d_shader_set_sampler1D_slot(scene.geometry, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(scene.geometry, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(scene.geometry, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(scene.geometry, uTexEmission, 3);
    r3d_shader_set_sampler2D_slot(scene.geometry, uTexORM, 4);
    r3d_shader_disable();
}

void r3d_shader_load_scene_forward(void)
{
    R3D.shader.scene.forward.id = rlLoadShaderCode(
        FORWARD_VERT, FORWARD_FRAG
    );

    R3D_SHADER_VALIDATION(scene.forward);

    r3d_shader_scene_forward_t* shader = &R3D.shader.scene.forward;

    r3d_shader_get_location(scene.forward, uTexBoneMatrices);
    r3d_shader_get_location(scene.forward, uMatInvView);
    r3d_shader_get_location(scene.forward, uMatNormal);
    r3d_shader_get_location(scene.forward, uMatModel);
    r3d_shader_get_location(scene.forward, uMatVP);
    r3d_shader_get_location(scene.forward, uAlbedoColor);
    r3d_shader_get_location(scene.forward, uTexCoordOffset);
    r3d_shader_get_location(scene.forward, uTexCoordScale);
    r3d_shader_get_location(scene.forward, uInstancing);
    r3d_shader_get_location(scene.forward, uSkinning);
    r3d_shader_get_location(scene.forward, uBillboard);
    r3d_shader_get_location(scene.forward, uTexAlbedo);
    r3d_shader_get_location(scene.forward, uTexEmission);
    r3d_shader_get_location(scene.forward, uTexNormal);
    r3d_shader_get_location(scene.forward, uTexORM);
    r3d_shader_get_location(scene.forward, uEmissionEnergy);
    r3d_shader_get_location(scene.forward, uNormalScale);
    r3d_shader_get_location(scene.forward, uOcclusion);
    r3d_shader_get_location(scene.forward, uRoughness);
    r3d_shader_get_location(scene.forward, uMetalness);
    r3d_shader_get_location(scene.forward, uAmbientLight);
    r3d_shader_get_location(scene.forward, uEmissionColor);
    r3d_shader_get_location(scene.forward, uCubeIrradiance);
    r3d_shader_get_location(scene.forward, uCubePrefilter);
    r3d_shader_get_location(scene.forward, uTexBrdfLut);
    r3d_shader_get_location(scene.forward, uQuatSkybox);
    r3d_shader_get_location(scene.forward, uHasSkybox);
    r3d_shader_get_location(scene.forward, uSkyboxAmbientIntensity);
    r3d_shader_get_location(scene.forward, uSkyboxReflectIntensity);
    r3d_shader_get_location(scene.forward, uAlphaCutoff);
    r3d_shader_get_location(scene.forward, uViewPosition);
    r3d_shader_get_location(scene.forward, uFar);

    r3d_shader_enable(scene.forward);

    r3d_shader_set_sampler1D_slot(scene.forward, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(scene.forward, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(scene.forward, uTexEmission, 2);
    r3d_shader_set_sampler2D_slot(scene.forward, uTexNormal, 3);
    r3d_shader_set_sampler2D_slot(scene.forward, uTexORM, 4);
    r3d_shader_set_samplerCube_slot(scene.forward, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(scene.forward, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(scene.forward, uTexBrdfLut, 7);

    int shadowMapSlot = 10;
    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++) {
        shader->uMatLightVP[i].loc = rlGetLocationUniform(shader->id, TextFormat("uMatLightVP[%i]", i));
        shader->uShadowMapCube[i].loc = rlGetLocationUniform(shader->id, TextFormat("uShadowMapCube[%i]", i));
        shader->uShadowMap2D[i].loc = rlGetLocationUniform(shader->id, TextFormat("uShadowMap2D[%i]", i));
        shader->uLights[i].color.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].color", i));
        shader->uLights[i].position.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].position", i));
        shader->uLights[i].direction.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].direction", i));
        shader->uLights[i].specular.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].specular", i));
        shader->uLights[i].energy.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].energy", i));
        shader->uLights[i].range.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].range", i));
        shader->uLights[i].near.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].near", i));
        shader->uLights[i].far.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].far", i));
        shader->uLights[i].attenuation.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].attenuation", i));
        shader->uLights[i].innerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].innerCutOff", i));
        shader->uLights[i].outerCutOff.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].outerCutOff", i));
        shader->uLights[i].shadowSoftness.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowSoftness", i));
        shader->uLights[i].shadowMapTxlSz.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowMapTxlSz", i));
        shader->uLights[i].shadowDepthBias.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowDepthBias", i));
        shader->uLights[i].shadowSlopeBias.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadowSlopeBias", i));
        shader->uLights[i].type.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].type", i));
        shader->uLights[i].enabled.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].enabled", i));
        shader->uLights[i].shadow.loc = rlGetLocationUniform(shader->id, TextFormat("uLights[%i].shadow", i));

        r3d_shader_set_samplerCube_slot(scene.forward, uShadowMapCube[i], shadowMapSlot++);
        r3d_shader_set_sampler2D_slot(scene.forward, uShadowMap2D[i], shadowMapSlot++);
    }

    r3d_shader_disable();
}

void r3d_shader_load_scene_background(void)
{
    R3D.shader.scene.background.id = rlLoadShaderCode(
        SCREEN_VERT, COLOR_FRAG
    );
    R3D_SHADER_VALIDATION(scene.background);

    r3d_shader_get_location(scene.background, uColor);
}

void r3d_shader_load_scene_skybox(void)
{
    R3D.shader.scene.skybox.id = rlLoadShaderCode(
        SKYBOX_VERT, SKYBOX_FRAG
    );

    R3D_SHADER_VALIDATION(scene.skybox);

    r3d_shader_get_location(scene.skybox, uMatProj);
    r3d_shader_get_location(scene.skybox, uMatView);
    r3d_shader_get_location(scene.skybox, uRotation);
    r3d_shader_get_location(scene.skybox, uSkyIntensity);
    r3d_shader_get_location(scene.skybox, uCubeSky);

    r3d_shader_enable(scene.skybox);
    r3d_shader_set_samplerCube_slot(scene.skybox, uCubeSky, 0);
    r3d_shader_disable();
}

void r3d_shader_load_scene_depth_volume(void)
{
    R3D.shader.scene.depthVolume.id = rlLoadShaderCode(
        DEPTH_VOLUME_VERT, DEPTH_VOLUME_FRAG
    );

    R3D_SHADER_VALIDATION(scene.depthVolume);

    r3d_shader_get_location(scene.depthVolume, uMatMVP);
}

void r3d_shader_load_scene_depth(void)
{
    R3D.shader.scene.depth.id = rlLoadShaderCode(
        DEPTH_VERT, DEPTH_FRAG
    );

    R3D_SHADER_VALIDATION(scene.depth);

    r3d_shader_get_location(scene.depth, uTexBoneMatrices);
    r3d_shader_get_location(scene.depth, uMatInvView);
    r3d_shader_get_location(scene.depth, uMatModel);
    r3d_shader_get_location(scene.depth, uMatVP);
    r3d_shader_get_location(scene.depth, uTexCoordOffset);
    r3d_shader_get_location(scene.depth, uTexCoordScale);
    r3d_shader_get_location(scene.depth, uAlpha);
    r3d_shader_get_location(scene.depth, uInstancing);
    r3d_shader_get_location(scene.depth, uSkinning);
    r3d_shader_get_location(scene.depth, uBillboard);
    r3d_shader_get_location(scene.depth, uTexAlbedo);
    r3d_shader_get_location(scene.depth, uAlphaCutoff);

    r3d_shader_enable(scene.depth);
    r3d_shader_set_sampler1D_slot(scene.depth, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(scene.depth, uTexAlbedo, 1);
    r3d_shader_disable();
}

void r3d_shader_load_scene_depth_cube(void)
{
    R3D.shader.scene.depthCube.id = rlLoadShaderCode(
        DEPTH_CUBE_VERT, DEPTH_CUBE_FRAG
    );

    R3D_SHADER_VALIDATION(scene.depthCube);

    r3d_shader_get_location(scene.depthCube, uTexBoneMatrices);
    r3d_shader_get_location(scene.depthCube, uMatInvView);
    r3d_shader_get_location(scene.depthCube, uMatModel);
    r3d_shader_get_location(scene.depthCube, uMatVP);
    r3d_shader_get_location(scene.depthCube, uTexCoordOffset);
    r3d_shader_get_location(scene.depthCube, uTexCoordScale);
    r3d_shader_get_location(scene.depthCube, uAlpha);
    r3d_shader_get_location(scene.depthCube, uInstancing);
    r3d_shader_get_location(scene.depthCube, uSkinning);
    r3d_shader_get_location(scene.depthCube, uBillboard);
    r3d_shader_get_location(scene.depthCube, uTexAlbedo);
    r3d_shader_get_location(scene.depthCube, uAlphaCutoff);
    r3d_shader_get_location(scene.depthCube, uViewPosition);
    r3d_shader_get_location(scene.depthCube, uFar);

    r3d_shader_enable(scene.depthCube);
    r3d_shader_set_sampler1D_slot(scene.depthCube, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(scene.depthCube, uTexAlbedo, 1);
    r3d_shader_disable();
}

void r3d_shader_load_scene_decal(void)
{
    R3D.shader.scene.decal.id = rlLoadShaderCode(
        DECAL_VERT, DECAL_FRAG
    );

    R3D_SHADER_VALIDATION(scene.decal);
    r3d_shader_get_location(scene.decal, uMatInvProj);
    r3d_shader_get_location(scene.decal, uMatProj);
    r3d_shader_get_location(scene.decal, uMatInvView);
    r3d_shader_get_location(scene.decal, uMatNormal);
    r3d_shader_get_location(scene.decal, uMatModel);
    r3d_shader_get_location(scene.decal, uMatVP);
    r3d_shader_get_location(scene.decal, uAlbedoColor);
    r3d_shader_get_location(scene.decal, uEmissionEnergy);
    r3d_shader_get_location(scene.decal, uEmissionColor);
    r3d_shader_get_location(scene.decal, uTexCoordOffset);
    r3d_shader_get_location(scene.decal, uTexCoordScale);
    r3d_shader_get_location(scene.decal, uInstancing);
    r3d_shader_get_location(scene.decal, uTexAlbedo);
    r3d_shader_get_location(scene.decal, uTexNormal);
    r3d_shader_get_location(scene.decal, uTexEmission);
    r3d_shader_get_location(scene.decal, uTexORM);
    r3d_shader_get_location(scene.decal, uTexDepth);
    r3d_shader_get_location(scene.decal, uAlphaCutoff);
    r3d_shader_get_location(scene.decal, uNormalScale);
    r3d_shader_get_location(scene.decal, uOcclusion);
    r3d_shader_get_location(scene.decal, uRoughness);
    r3d_shader_get_location(scene.decal, uMetalness);

    r3d_shader_enable(scene.decal);
    r3d_shader_set_sampler2D_slot(scene.decal, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(scene.decal, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(scene.decal, uTexEmission, 2);
    r3d_shader_set_sampler2D_slot(scene.decal, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(scene.decal, uTexDepth, 4);
    r3d_shader_disable();
}

void r3d_shader_load_deferred_ambient_ibl(void)
{
    const char* defines[] = { "#define IBL" };
    char* fsCode = r3d_shader_inject_defines(AMBIENT_FRAG, defines, 1);
    R3D.shader.deferred.ambientIbl.id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    R3D_SHADER_VALIDATION(deferred.ambientIbl);

    RL_FREE(fsCode);

    r3d_shader_deferred_ambient_ibl_t* shader = &R3D.shader.deferred.ambientIbl;

    r3d_shader_get_location(deferred.ambientIbl, uTexAlbedo);
    r3d_shader_get_location(deferred.ambientIbl, uTexNormal);
    r3d_shader_get_location(deferred.ambientIbl, uTexDepth);
    r3d_shader_get_location(deferred.ambientIbl, uTexSSAO);
    r3d_shader_get_location(deferred.ambientIbl, uTexORM);
    r3d_shader_get_location(deferred.ambientIbl, uCubeIrradiance);
    r3d_shader_get_location(deferred.ambientIbl, uCubePrefilter);
    r3d_shader_get_location(deferred.ambientIbl, uTexBrdfLut);
    r3d_shader_get_location(deferred.ambientIbl, uQuatSkybox);
    r3d_shader_get_location(deferred.ambientIbl, uSkyboxAmbientIntensity);
    r3d_shader_get_location(deferred.ambientIbl, uSkyboxReflectIntensity);
    r3d_shader_get_location(deferred.ambientIbl, uViewPosition);
    r3d_shader_get_location(deferred.ambientIbl, uMatInvProj);
    r3d_shader_get_location(deferred.ambientIbl, uMatInvView);
    r3d_shader_get_location(deferred.ambientIbl, uSSAOPower);

    r3d_shader_enable(deferred.ambientIbl);

    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexSSAO, 3);
    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexORM, 4);

    r3d_shader_set_samplerCube_slot(deferred.ambientIbl, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(deferred.ambientIbl, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(deferred.ambientIbl, uTexBrdfLut, 7);

    r3d_shader_disable();
}

void r3d_shader_load_deferred_ambient(void)
{
    R3D.shader.deferred.ambient.id = rlLoadShaderCode(
        SCREEN_VERT, AMBIENT_FRAG
    );

    R3D_SHADER_VALIDATION(deferred.ambient);

    r3d_shader_get_location(deferred.ambient, uTexAlbedo);
    r3d_shader_get_location(deferred.ambient, uTexSSAO);
    r3d_shader_get_location(deferred.ambient, uTexORM);
    r3d_shader_get_location(deferred.ambient, uAmbientLight);
    r3d_shader_get_location(deferred.ambient, uSSAOPower);

    r3d_shader_enable(deferred.ambient);
    r3d_shader_set_sampler2D_slot(deferred.ambient, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(deferred.ambient, uTexSSAO, 1);
    r3d_shader_set_sampler2D_slot(deferred.ambient, uTexORM, 2);
    r3d_shader_disable();
}

void r3d_shader_load_deferred_lighting(void)
{
    R3D.shader.deferred.lighting.id = rlLoadShaderCode(SCREEN_VERT, LIGHTING_FRAG);

    R3D_SHADER_VALIDATION(deferred.lighting);

    r3d_shader_deferred_lighting_t* shader = &R3D.shader.deferred.lighting;

    r3d_shader_get_location(deferred.lighting, uTexAlbedo);
    r3d_shader_get_location(deferred.lighting, uTexNormal);
    r3d_shader_get_location(deferred.lighting, uTexDepth);
    r3d_shader_get_location(deferred.lighting, uTexORM);
    r3d_shader_get_location(deferred.lighting, uViewPosition);
    r3d_shader_get_location(deferred.lighting, uMatInvProj);
    r3d_shader_get_location(deferred.lighting, uMatInvView);

    r3d_shader_get_location(deferred.lighting, uLight.matVP);
    r3d_shader_get_location(deferred.lighting, uLight.shadowMap);
    r3d_shader_get_location(deferred.lighting, uLight.shadowCubemap);
    r3d_shader_get_location(deferred.lighting, uLight.color);
    r3d_shader_get_location(deferred.lighting, uLight.position);
    r3d_shader_get_location(deferred.lighting, uLight.direction);
    r3d_shader_get_location(deferred.lighting, uLight.specular);
    r3d_shader_get_location(deferred.lighting, uLight.energy);
    r3d_shader_get_location(deferred.lighting, uLight.range);
    r3d_shader_get_location(deferred.lighting, uLight.near);
    r3d_shader_get_location(deferred.lighting, uLight.far);
    r3d_shader_get_location(deferred.lighting, uLight.attenuation);
    r3d_shader_get_location(deferred.lighting, uLight.innerCutOff);
    r3d_shader_get_location(deferred.lighting, uLight.outerCutOff);
    r3d_shader_get_location(deferred.lighting, uLight.shadowSoftness);
    r3d_shader_get_location(deferred.lighting, uLight.shadowMapTxlSz);
    r3d_shader_get_location(deferred.lighting, uLight.shadowDepthBias);
    r3d_shader_get_location(deferred.lighting, uLight.shadowSlopeBias);
    r3d_shader_get_location(deferred.lighting, uLight.type);
    r3d_shader_get_location(deferred.lighting, uLight.shadow);

    r3d_shader_enable(deferred.lighting);

    r3d_shader_set_sampler2D_slot(deferred.lighting, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(deferred.lighting, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(deferred.lighting, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(deferred.lighting, uTexORM, 3);

    r3d_shader_set_sampler2D_slot(deferred.lighting, uLight.shadowMap, 4);
    r3d_shader_set_samplerCube_slot(deferred.lighting, uLight.shadowCubemap, 5);

    r3d_shader_disable();
}

void r3d_shader_load_deferred_compose(void)
{
    R3D.shader.deferred.compose.id = rlLoadShaderCode(SCREEN_VERT, COMPOSE_FRAG);

    R3D_SHADER_VALIDATION(deferred.compose);

    r3d_shader_deferred_compose_t* shader = &R3D.shader.deferred.compose;

    r3d_shader_get_location(deferred.compose, uTexAlbedo);
    r3d_shader_get_location(deferred.compose, uTexEmission);
    r3d_shader_get_location(deferred.compose, uTexDiffuse);
    r3d_shader_get_location(deferred.compose, uTexSpecular);
    r3d_shader_get_location(deferred.compose, uTexSSAO);
    r3d_shader_get_location(deferred.compose, uSSAOPower);
    r3d_shader_get_location(deferred.compose, uSSAOLightAffect);

    r3d_shader_enable(deferred.compose);

    r3d_shader_set_sampler2D_slot(deferred.compose, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(deferred.compose, uTexEmission, 1);
    r3d_shader_set_sampler2D_slot(deferred.compose, uTexDiffuse, 2);
    r3d_shader_set_sampler2D_slot(deferred.compose, uTexSpecular, 3);

    r3d_shader_set_sampler2D_slot(deferred.compose, uTexSSAO, 4);

    r3d_shader_disable();
}

void r3d_shader_load_post_bloom(void)
{
    R3D.shader.post.bloom.id = rlLoadShaderCode(
        SCREEN_VERT, BLOOM_FRAG
    );

    R3D_SHADER_VALIDATION(post.bloom);

    r3d_shader_get_location(post.bloom, uTexColor);
    r3d_shader_get_location(post.bloom, uTexBloomBlur);
    r3d_shader_get_location(post.bloom, uBloomMode);
    r3d_shader_get_location(post.bloom, uBloomIntensity);

    r3d_shader_enable(post.bloom);
    r3d_shader_set_sampler2D_slot(post.bloom, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(post.bloom, uTexBloomBlur, 1);
    r3d_shader_disable();
}

void r3d_shader_load_post_ssr(void)
{
    R3D.shader.post.ssr.id = rlLoadShaderCode(
        SCREEN_VERT, SSR_FRAG
    );

    R3D_SHADER_VALIDATION(post.ssr);

    r3d_shader_get_location(post.ssr, uTexColor);
    r3d_shader_get_location(post.ssr, uTexAlbedo);
    r3d_shader_get_location(post.ssr, uTexNormal);
    r3d_shader_get_location(post.ssr, uTexORM);
    r3d_shader_get_location(post.ssr, uTexDepth);
    r3d_shader_get_location(post.ssr, uMatView);
    r3d_shader_get_location(post.ssr, uMaxRaySteps);
    r3d_shader_get_location(post.ssr, uBinarySearchSteps);
    r3d_shader_get_location(post.ssr, uRayMarchLength);
    r3d_shader_get_location(post.ssr, uDepthThickness);
    r3d_shader_get_location(post.ssr, uDepthTolerance);
    r3d_shader_get_location(post.ssr, uEdgeFadeStart);
    r3d_shader_get_location(post.ssr, uEdgeFadeEnd);
    r3d_shader_get_location(post.ssr, uMatInvProj);
    r3d_shader_get_location(post.ssr, uMatInvView);
    r3d_shader_get_location(post.ssr, uMatViewProj);
    r3d_shader_get_location(post.ssr, uViewPosition);

    r3d_shader_enable(post.ssr);
    r3d_shader_set_sampler2D_slot(post.ssr, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(post.ssr, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(post.ssr, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(post.ssr, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(post.ssr, uTexDepth, 4);
    r3d_shader_disable();
}

void r3d_shader_load_post_fog(void)
{
    R3D.shader.post.fog.id = rlLoadShaderCode(
        SCREEN_VERT, FOG_FRAG
    );

    R3D_SHADER_VALIDATION(post.fog);

    r3d_shader_get_location(post.fog, uTexColor);
    r3d_shader_get_location(post.fog, uTexDepth);
    r3d_shader_get_location(post.fog, uNear);
    r3d_shader_get_location(post.fog, uFar);
    r3d_shader_get_location(post.fog, uFogMode);
    r3d_shader_get_location(post.fog, uFogColor);
    r3d_shader_get_location(post.fog, uFogStart);
    r3d_shader_get_location(post.fog, uFogEnd);
    r3d_shader_get_location(post.fog, uFogDensity);
    r3d_shader_get_location(post.fog, uSkyAffect);

    r3d_shader_enable(post.fog);
    r3d_shader_set_sampler2D_slot(post.fog, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(post.fog, uTexDepth, 1);
    r3d_shader_disable();
}

void r3d_shader_load_post_dof(void)
{
    R3D.shader.post.dof.id = rlLoadShaderCode(
        SCREEN_VERT, DOF_FRAG
    );

    r3d_shader_get_location(post.dof, uTexColor);
    r3d_shader_get_location(post.dof, uTexDepth);
    r3d_shader_get_location(post.dof, uTexelSize);
    r3d_shader_get_location(post.dof, uNear);
    r3d_shader_get_location(post.dof, uFar);
    r3d_shader_get_location(post.dof, uFocusPoint);
    r3d_shader_get_location(post.dof, uFocusScale);
    r3d_shader_get_location(post.dof, uMaxBlurSize);
    r3d_shader_get_location(post.dof, uDebugMode);

    r3d_shader_enable(post.dof);
    r3d_shader_set_sampler2D_slot(post.dof, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(post.dof, uTexDepth, 1);
    r3d_shader_disable();
}

void r3d_shader_load_post_output(R3D_Tonemap tonemap)
{
    assert(R3D.shader.post.output[tonemap].id == 0);

    const char* defines[] = {
        TextFormat("#define TONEMAPPER %i", tonemap)
    };

    char* fsCode = r3d_shader_inject_defines(OUTPUT_FRAG, defines, 1);
    R3D.shader.post.output[tonemap].id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    R3D_SHADER_VALIDATION(post.output[tonemap]);

    RL_FREE(fsCode);

    r3d_shader_get_location(post.output[tonemap], uTexColor);
    r3d_shader_get_location(post.output[tonemap], uTonemapExposure);
    r3d_shader_get_location(post.output[tonemap], uTonemapWhite);
    r3d_shader_get_location(post.output[tonemap], uBrightness);
    r3d_shader_get_location(post.output[tonemap], uContrast);
    r3d_shader_get_location(post.output[tonemap], uSaturation);

    r3d_shader_enable(post.output[tonemap]);
    r3d_shader_set_sampler2D_slot(post.output[tonemap], uTexColor, 0);
    r3d_shader_disable();
}

void r3d_shader_load_post_fxaa(void)
{
    R3D.shader.post.fxaa.id = rlLoadShaderCode(
        SCREEN_VERT, FXAA_FRAG
    );

    R3D_SHADER_VALIDATION(post.fxaa);

    r3d_shader_get_location(post.fxaa, uTexture);
    r3d_shader_get_location(post.fxaa, uTexelSize);

    r3d_shader_enable(post.fxaa);
    r3d_shader_set_sampler2D_slot(post.fxaa, uTexture, 0);
    r3d_shader_disable();
}

/* === Texture loading functions === */

void r3d_texture_load_white(void)
{
    static const char DATA = 0xFF;
    R3D.texture.white = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1);
}

void r3d_texture_load_black(void)
{
    static const char DATA = 0x00;
    R3D.texture.black = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE, 1);
}

void r3d_texture_load_normal(void)
{
    static const unsigned char DATA[3] = { 127, 127, 255 };
    R3D.texture.normal = rlLoadTexture(&DATA, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8, 1);
}

void r3d_texture_load_ssao_noise(void)
{
#   define R3D_RAND_NOISE_RESOLUTION 4

    r3d_half_t noise[2 * R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION] = { 0 };

    for (int i = 0; i < R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION; i++) {
        noise[i * 2 + 0] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
        noise[i * 2 + 1] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
    }

    glGenTextures(1, &R3D.texture.ssaoNoise);
    glBindTexture(GL_TEXTURE_2D, R3D.texture.ssaoNoise);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, R3D_RAND_NOISE_RESOLUTION, R3D_RAND_NOISE_RESOLUTION, 0, GL_RG, GL_HALF_FLOAT, noise);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void r3d_texture_load_ssao_kernel(void)
{
#   define R3D_SSAO_KERNEL_SIZE 32

    r3d_half_t kernel[3 * R3D_SSAO_KERNEL_SIZE] = { 0 };

    for (int i = 0; i < R3D_SSAO_KERNEL_SIZE; i++)
    {
        Vector3 sample = { 0 };

        sample.x = ((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f;
        sample.y = ((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f;
        sample.z = (float)GetRandomValue(0, INT16_MAX) / INT16_MAX;

        sample = Vector3Normalize(sample);
        sample = Vector3Scale(sample, (float)GetRandomValue(0, INT16_MAX) / INT16_MAX);

        float scale = (float)i / R3D_SSAO_KERNEL_SIZE;
        scale = Lerp(0.1f, 1.0f, scale * scale);
        sample = Vector3Scale(sample, scale);

        kernel[i * 3 + 0] = r3d_cvt_fh(sample.x);
        kernel[i * 3 + 1] = r3d_cvt_fh(sample.y);
        kernel[i * 3 + 2] = r3d_cvt_fh(sample.z);
    }

    glGenTextures(1, &R3D.texture.ssaoKernel);
    glBindTexture(GL_TEXTURE_1D, R3D.texture.ssaoKernel);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16F, R3D_SSAO_KERNEL_SIZE, 0, GL_RGB, GL_HALF_FLOAT, kernel);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
}

void r3d_texture_load_ibl_brdf_lut(void)
{
    GLenum format = r3d_support_get_internal_format(GL_RG16F, false);

    glGenTextures(1, &R3D.texture.iblBrdfLut);
    glBindTexture(GL_TEXTURE_2D, R3D.texture.iblBrdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, format, 512, 512, 0, GL_RG, GL_HALF_FLOAT, BRDF_LUT_512_RG16_FLOAT_RAW);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

/* === Storage loading functions === */

void r3d_storage_load_tex_matrices(void)
{
    assert(R3D.storage.texMatrices[0] == 0);

    static const int count = sizeof(R3D.storage.texMatrices) / sizeof(*R3D.storage.texMatrices);

    glGenTextures(count, R3D.storage.texMatrices);

    for (int i = 0; i < count; i++) {
        glBindTexture(GL_TEXTURE_1D, R3D.storage.texMatrices[i]);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, 4 * R3D_STORAGE_MATRIX_CAPACITY, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    }

    glBindTexture(GL_TEXTURE_1D, 0);
}
