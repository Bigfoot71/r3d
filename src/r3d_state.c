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

#include <shaders/ambient.frag.h>
#include <shaders/bloom.frag.h>
#include <shaders/cubemap_from_equirectangular.frag.h>
#include <shaders/cubemap.vert.h>
#include <shaders/depth_cube.frag.h>
#include <shaders/depth_cube.vert.h>
#include <shaders/depth.frag.h>
#include <shaders/depth.vert.h>
#include <shaders/depth_volume.frag.h>
#include <shaders/depth_volume.vert.h>
#include <shaders/dof.frag.h>
#include <shaders/downsampling.frag.h>
#include <shaders/fog.frag.h>
#include <shaders/forward.frag.h>
#include <shaders/forward.vert.h>
#include <shaders/fxaa.frag.h>
#include <shaders/gaussian_blur_dual_pass.frag.h>
#include <shaders/geometry.frag.h>
#include <shaders/geometry.vert.h>
#include <shaders/irradiance_convolution.frag.h>
#include <shaders/lighting.frag.h>
#include <shaders/output.frag.h>
#include <shaders/prefilter.frag.h>
#include <shaders/scene.frag.h>
#include <shaders/screen.vert.h>
#include <shaders/skybox.frag.h>
#include <shaders/skybox.vert.h>
#include <shaders/ssao.frag.h>
#include <shaders/ssr.frag.h>
#include <shaders/upsampling.frag.h>

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
    if (R3D.target.depthStencil > 0) {
        glDeleteTextures(1, &R3D.target.depthStencil);
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

    r3d_shader_load_generate_cubemap_from_equirectangular();
    r3d_shader_load_generate_irradiance_convolution();
    r3d_shader_load_generate_prefilter();

    /* --- Scene shader passes --- */

    r3d_shader_load_raster_geometry();
    r3d_shader_load_raster_forward();
    r3d_shader_load_raster_skybox();
    r3d_shader_load_raster_depth_volume();
    r3d_shader_load_raster_depth();
    r3d_shader_load_raster_depth_cube();

    /* --- Screen shader passes --- */

    r3d_shader_load_screen_ambient_ibl();
    r3d_shader_load_screen_ambient();
    r3d_shader_load_screen_lighting();
    r3d_shader_load_screen_scene();

    // NOTE: Don't load the output shader here to avoid keeping an unused tonemap mode
    //       in memory in case the tonemap mode changes after initialization.
    //       It is loaded on demand during `R3D_End()`

    // TODO: Revisit the shader loading mechanism. Constantly checking and loading
    //       it during `R3D_End()` doesn't feel like the cleanest approach

    //r3d_shader_load_screen_output(R3D.env.tonemapMode);

    /* --- Additional screen shader passes --- */

    if (R3D.env.ssaoEnabled) {
        r3d_shader_load_generate_gaussian_blur_dual_pass();
        r3d_shader_load_screen_ssao();
    }
    if (R3D.env.bloomMode != R3D_BLOOM_DISABLED) {
        r3d_shader_load_generate_downsampling();
        r3d_shader_load_generate_upsampling();
        r3d_shader_load_screen_bloom();
    }
    if (R3D.env.ssrEnabled) {
        r3d_shader_load_screen_ssr();
    }
    if (R3D.env.fogMode != R3D_FOG_DISABLED) {
        r3d_shader_load_screen_fog();
    }
    if (R3D.env.dofMode != R3D_DOF_DISABLED) {
        r3d_shader_load_screen_dof();
    }
    if (R3D.state.flags & R3D_FLAG_FXAA) {
        r3d_shader_load_screen_fxaa();
    }
}

void r3d_shaders_unload(void)
{
    // Unload generation shaders
    rlUnloadShaderProgram(R3D.shader.generate.gaussianBlurDualPass.id);
    rlUnloadShaderProgram(R3D.shader.generate.cubemapFromEquirectangular.id);
    rlUnloadShaderProgram(R3D.shader.generate.irradianceConvolution.id);
    rlUnloadShaderProgram(R3D.shader.generate.prefilter.id);

    // Unload raster shaders
    rlUnloadShaderProgram(R3D.shader.raster.geometry.id);
    rlUnloadShaderProgram(R3D.shader.raster.forward.id);
    rlUnloadShaderProgram(R3D.shader.raster.skybox.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthVolume.id);
    rlUnloadShaderProgram(R3D.shader.raster.depth.id);
    rlUnloadShaderProgram(R3D.shader.raster.depthCube.id);

    // Unload screen shaders
    rlUnloadShaderProgram(R3D.shader.screen.ambientIbl.id);
    rlUnloadShaderProgram(R3D.shader.screen.ambient.id);
    rlUnloadShaderProgram(R3D.shader.screen.lighting.id);
    rlUnloadShaderProgram(R3D.shader.screen.scene.id);

    for (int i = 0; i < R3D_TONEMAP_COUNT; i++) {
        if (R3D.shader.screen.output[i].id != 0) {
            rlUnloadShaderProgram(R3D.shader.screen.output[i].id);
        }
    }

    if (R3D.shader.screen.ssao.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.ssao.id);
    }
    if (R3D.shader.screen.bloom.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.bloom.id);
    }
    if (R3D.shader.screen.ssr.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.ssr.id);
    }
    if (R3D.shader.screen.fog.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.fog.id);
    }
    if (R3D.shader.screen.dof.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.dof.id);
    }
    if (R3D.shader.screen.fxaa.id != 0) {
        rlUnloadShaderProgram(R3D.shader.screen.fxaa.id);
    }
}

void r3d_shader_load_screen_dof(void)
{
    R3D.shader.screen.dof.id = rlLoadShaderCode(
        SCREEN_VERT, DOF_FRAG
    );

    r3d_shader_get_location(screen.dof, uTexColor);
    r3d_shader_get_location(screen.dof, uTexDepth);
    r3d_shader_get_location(screen.dof, uTexelSize);
    r3d_shader_get_location(screen.dof, uNear);
    r3d_shader_get_location(screen.dof, uFar);
    r3d_shader_get_location(screen.dof, uFocusPoint);
    r3d_shader_get_location(screen.dof, uFocusScale);
    r3d_shader_get_location(screen.dof, uMaxBlurSize);
    r3d_shader_get_location(screen.dof, uDebugMode);

    r3d_shader_enable(screen.dof);
    r3d_shader_set_sampler2D_slot(screen.dof, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.dof, uTexDepth, 1);
    r3d_shader_disable();
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

static void r3d_target_load_depth_stencil(int width, int height)
{
    assert(R3D.target.depthStencil == 0);

    glGenTextures(1, &R3D.target.depthStencil);
    glBindTexture(GL_TEXTURE_2D, R3D.target.depthStencil);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
    int maxLevels = 1 + (int)floor(log2((float)maxDimension));

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
    for (GLuint i = 0; i < R3D.target.mipChainHs.count; i++, wMip /= 2, hMip /= 2) {
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

    if (!R3D.target.albedo)         r3d_target_load_albedo(width, height);
    if (!R3D.target.emission)       r3d_target_load_emission(width, height);
    if (!R3D.target.normal)         r3d_target_load_normal(width, height);
    if (!R3D.target.orm)            r3d_target_load_orm(width, height);
    if (!R3D.target.depthStencil)   r3d_target_load_depth_stencil(width, height);

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
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depthStencil, 0);

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
    if (!R3D.target.depthStencil) r3d_target_load_depth_stencil(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.ssao);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.ssao);

    glDrawBuffers(1, (GLenum[]) {
        GL_COLOR_ATTACHMENT0
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.ssaoPpHs[0], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depthStencil, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_WARNING, "R3D: The SSAO ping-pong buffer is not complete (status: 0x%4x)", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void r3d_framebuffer_load_deferred(int width, int height)
{
    /* --- Ensures that targets exist --- */

    if (!R3D.target.diffuse)        r3d_target_load_diffuse(width, height);
    if (!R3D.target.specular)       r3d_target_load_specular(width, height);
    if (!R3D.target.depthStencil)   r3d_target_load_depth_stencil(width, height);

    /* --- Create and configure the framebuffer --- */

    glGenFramebuffers(1, &R3D.framebuffer.deferred);
    glBindFramebuffer(GL_FRAMEBUFFER, R3D.framebuffer.deferred);

    glDrawBuffers(2, (GLenum[]) {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
    });

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R3D.target.diffuse, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, R3D.target.specular, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depthStencil, 0);

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

    if (!R3D.target.scenePp[0])     r3d_target_load_scene_pp(width, height);
    if (!R3D.target.albedo)         r3d_target_load_albedo(width, height);
    if (!R3D.target.normal)         r3d_target_load_normal(width, height);
    if (!R3D.target.orm)            r3d_target_load_orm(width, height);
    if (!R3D.target.depthStencil)   r3d_target_load_depth_stencil(width, height);

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
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, R3D.target.depthStencil, 0);

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
    
void r3d_shader_load_generate_gaussian_blur_dual_pass(void)
{
    R3D.shader.generate.gaussianBlurDualPass.id = rlLoadShaderCode(
        SCREEN_VERT, GAUSSIAN_BLUR_DUAL_PASS_FRAG
    );

    R3D_SHADER_VALIDATION(generate.gaussianBlurDualPass);

    r3d_shader_get_location(generate.gaussianBlurDualPass, uTexture);
    r3d_shader_get_location(generate.gaussianBlurDualPass, uTexelDir);

    r3d_shader_enable(generate.gaussianBlurDualPass);
    r3d_shader_set_sampler2D_slot(generate.gaussianBlurDualPass, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_downsampling(void)
{
    R3D.shader.generate.downsampling.id = rlLoadShaderCode(
        SCREEN_VERT, DOWNSAMPLING_FRAG
    );

    R3D_SHADER_VALIDATION(generate.downsampling);

    r3d_shader_get_location(generate.downsampling, uTexture);
    r3d_shader_get_location(generate.downsampling, uTexelSize);
    r3d_shader_get_location(generate.downsampling, uMipLevel);
    r3d_shader_get_location(generate.downsampling, uPrefilter);

    r3d_shader_enable(generate.downsampling);
    r3d_shader_set_sampler2D_slot(generate.downsampling, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_upsampling(void)
{
    R3D.shader.generate.upsampling.id = rlLoadShaderCode(
        SCREEN_VERT, UPSAMPLING_FRAG
    );

    R3D_SHADER_VALIDATION(generate.upsampling);

    r3d_shader_get_location(generate.upsampling, uTexture);
    r3d_shader_get_location(generate.upsampling, uFilterRadius);

    r3d_shader_enable(generate.upsampling);
    r3d_shader_set_sampler2D_slot(generate.upsampling, uTexture, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_cubemap_from_equirectangular(void)
{
    R3D.shader.generate.cubemapFromEquirectangular.id = rlLoadShaderCode(
        CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG
    );

    R3D_SHADER_VALIDATION(generate.cubemapFromEquirectangular);

    r3d_shader_get_location(generate.cubemapFromEquirectangular, uMatProj);
    r3d_shader_get_location(generate.cubemapFromEquirectangular, uMatView);
    r3d_shader_get_location(generate.cubemapFromEquirectangular, uTexEquirectangular);

    r3d_shader_enable(generate.cubemapFromEquirectangular);
    r3d_shader_set_sampler2D_slot(generate.cubemapFromEquirectangular, uTexEquirectangular, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_irradiance_convolution(void)
{
    R3D.shader.generate.irradianceConvolution.id = rlLoadShaderCode(
        CUBEMAP_VERT, IRRADIANCE_CONVOLUTION_FRAG
    );

    R3D_SHADER_VALIDATION(generate.irradianceConvolution);

    r3d_shader_get_location(generate.irradianceConvolution, uMatProj);
    r3d_shader_get_location(generate.irradianceConvolution, uMatView);
    r3d_shader_get_location(generate.irradianceConvolution, uCubemap);

    r3d_shader_enable(generate.irradianceConvolution);
    r3d_shader_set_samplerCube_slot(generate.irradianceConvolution, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_generate_prefilter(void)
{
    R3D.shader.generate.prefilter.id = rlLoadShaderCode(
        CUBEMAP_VERT, PREFILTER_FRAG
    );

    R3D_SHADER_VALIDATION(generate.prefilter);

    r3d_shader_get_location(generate.prefilter, uMatProj);
    r3d_shader_get_location(generate.prefilter, uMatView);
    r3d_shader_get_location(generate.prefilter, uCubemap);
    r3d_shader_get_location(generate.prefilter, uResolution);
    r3d_shader_get_location(generate.prefilter, uRoughness);

    r3d_shader_enable(generate.prefilter);
    r3d_shader_set_samplerCube_slot(generate.prefilter, uCubemap, 0);
    r3d_shader_disable();
}

void r3d_shader_load_raster_geometry(void)
{
    R3D.shader.raster.geometry.id = rlLoadShaderCode(
        GEOMETRY_VERT, GEOMETRY_FRAG
    );

    R3D_SHADER_VALIDATION(raster.geometry);

    r3d_shader_get_location(raster.geometry, uTexBoneMatrices);
    r3d_shader_get_location(raster.geometry, uMatInvView);
    r3d_shader_get_location(raster.geometry, uMatNormal);
    r3d_shader_get_location(raster.geometry, uMatModel);
    r3d_shader_get_location(raster.geometry, uMatVP);
    r3d_shader_get_location(raster.geometry, uAlbedoColor);
    r3d_shader_get_location(raster.geometry, uEmissionEnergy);
    r3d_shader_get_location(raster.geometry, uEmissionColor);
    r3d_shader_get_location(raster.geometry, uTexCoordOffset);
    r3d_shader_get_location(raster.geometry, uTexCoordScale);
    r3d_shader_get_location(raster.geometry, uInstancing);
    r3d_shader_get_location(raster.geometry, uSkinning);
    r3d_shader_get_location(raster.geometry, uBillboard);
    r3d_shader_get_location(raster.geometry, uTexAlbedo);
    r3d_shader_get_location(raster.geometry, uTexNormal);
    r3d_shader_get_location(raster.geometry, uTexEmission);
    r3d_shader_get_location(raster.geometry, uTexORM);
    r3d_shader_get_location(raster.geometry, uAlphaCutoff);
    r3d_shader_get_location(raster.geometry, uNormalScale);
    r3d_shader_get_location(raster.geometry, uOcclusion);
    r3d_shader_get_location(raster.geometry, uRoughness);
    r3d_shader_get_location(raster.geometry, uMetalness);

    r3d_shader_enable(raster.geometry);
    r3d_shader_set_sampler1D_slot(raster.geometry, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexEmission, 3);
    r3d_shader_set_sampler2D_slot(raster.geometry, uTexORM, 4);
    r3d_shader_disable();
}

void r3d_shader_load_raster_forward(void)
{
    R3D.shader.raster.forward.id = rlLoadShaderCode(
        FORWARD_VERT, FORWARD_FRAG
    );

    R3D_SHADER_VALIDATION(raster.forward);

    r3d_shader_raster_forward_t* shader = &R3D.shader.raster.forward;

    r3d_shader_get_location(raster.forward, uTexBoneMatrices);
    r3d_shader_get_location(raster.forward, uMatInvView);
    r3d_shader_get_location(raster.forward, uMatNormal);
    r3d_shader_get_location(raster.forward, uMatModel);
    r3d_shader_get_location(raster.forward, uMatVP);
    r3d_shader_get_location(raster.forward, uAlbedoColor);
    r3d_shader_get_location(raster.forward, uTexCoordOffset);
    r3d_shader_get_location(raster.forward, uTexCoordScale);
    r3d_shader_get_location(raster.forward, uInstancing);
    r3d_shader_get_location(raster.forward, uSkinning);
    r3d_shader_get_location(raster.forward, uBillboard);
    r3d_shader_get_location(raster.forward, uTexAlbedo);
    r3d_shader_get_location(raster.forward, uTexEmission);
    r3d_shader_get_location(raster.forward, uTexNormal);
    r3d_shader_get_location(raster.forward, uTexORM);
    r3d_shader_get_location(raster.forward, uEmissionEnergy);
    r3d_shader_get_location(raster.forward, uNormalScale);
    r3d_shader_get_location(raster.forward, uOcclusion);
    r3d_shader_get_location(raster.forward, uRoughness);
    r3d_shader_get_location(raster.forward, uMetalness);
    r3d_shader_get_location(raster.forward, uAmbientColor);
    r3d_shader_get_location(raster.forward, uEmissionColor);
    r3d_shader_get_location(raster.forward, uCubeIrradiance);
    r3d_shader_get_location(raster.forward, uCubePrefilter);
    r3d_shader_get_location(raster.forward, uTexBrdfLut);
    r3d_shader_get_location(raster.forward, uQuatSkybox);
    r3d_shader_get_location(raster.forward, uHasSkybox);
    r3d_shader_get_location(raster.forward, uSkyboxAmbientIntensity);
    r3d_shader_get_location(raster.forward, uSkyboxReflectIntensity);
    r3d_shader_get_location(raster.forward, uAlphaCutoff);
    r3d_shader_get_location(raster.forward, uViewPosition);
    r3d_shader_get_location(raster.forward, uFar);

    r3d_shader_enable(raster.forward);

    r3d_shader_set_sampler1D_slot(raster.forward, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexEmission, 2);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexNormal, 3);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexORM, 4);
    r3d_shader_set_samplerCube_slot(raster.forward, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(raster.forward, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(raster.forward, uTexBrdfLut, 7);

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

        r3d_shader_set_samplerCube_slot(raster.forward, uShadowMapCube[i], shadowMapSlot++);
        r3d_shader_set_sampler2D_slot(raster.forward, uShadowMap2D[i], shadowMapSlot++);
    }

    r3d_shader_disable();
}

void r3d_shader_load_raster_skybox(void)
{
    R3D.shader.raster.skybox.id = rlLoadShaderCode(
        SKYBOX_VERT, SKYBOX_FRAG
    );

    R3D_SHADER_VALIDATION(raster.skybox);

    r3d_shader_get_location(raster.skybox, uMatProj);
    r3d_shader_get_location(raster.skybox, uMatView);
    r3d_shader_get_location(raster.skybox, uRotation);
    r3d_shader_get_location(raster.skybox, uSkyIntensity);
    r3d_shader_get_location(raster.skybox, uCubeSky);

    r3d_shader_enable(raster.skybox);
    r3d_shader_set_samplerCube_slot(raster.skybox, uCubeSky, 0);
    r3d_shader_disable();
}

void r3d_shader_load_raster_depth_volume(void)
{
    R3D.shader.raster.depthVolume.id = rlLoadShaderCode(
        DEPTH_VOLUME_VERT, DEPTH_VOLUME_FRAG
    );

    R3D_SHADER_VALIDATION(raster.depthVolume);

    r3d_shader_get_location(raster.depthVolume, uMatMVP);
}

void r3d_shader_load_raster_depth(void)
{
    R3D.shader.raster.depth.id = rlLoadShaderCode(
        DEPTH_VERT, DEPTH_FRAG
    );

    R3D_SHADER_VALIDATION(raster.depth);

    r3d_shader_get_location(raster.depth, uTexBoneMatrices);
    r3d_shader_get_location(raster.depth, uMatInvView);
    r3d_shader_get_location(raster.depth, uMatModel);
    r3d_shader_get_location(raster.depth, uMatVP);
    r3d_shader_get_location(raster.depth, uTexCoordOffset);
    r3d_shader_get_location(raster.depth, uTexCoordScale);
    r3d_shader_get_location(raster.depth, uAlpha);
    r3d_shader_get_location(raster.depth, uInstancing);
    r3d_shader_get_location(raster.depth, uSkinning);
    r3d_shader_get_location(raster.depth, uBillboard);
    r3d_shader_get_location(raster.depth, uTexAlbedo);
    r3d_shader_get_location(raster.depth, uAlphaCutoff);

    r3d_shader_enable(raster.depth);
    r3d_shader_set_sampler1D_slot(raster.depth, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(raster.depth, uTexAlbedo, 1);
    r3d_shader_disable();
}

void r3d_shader_load_raster_depth_cube(void)
{
    R3D.shader.raster.depthCube.id = rlLoadShaderCode(
        DEPTH_CUBE_VERT, DEPTH_CUBE_FRAG
    );

    R3D_SHADER_VALIDATION(raster.depthCube);

    r3d_shader_get_location(raster.depthCube, uTexBoneMatrices);
    r3d_shader_get_location(raster.depthCube, uMatInvView);
    r3d_shader_get_location(raster.depthCube, uMatModel);
    r3d_shader_get_location(raster.depthCube, uMatVP);
    r3d_shader_get_location(raster.depthCube, uTexCoordOffset);
    r3d_shader_get_location(raster.depthCube, uTexCoordScale);
    r3d_shader_get_location(raster.depthCube, uAlpha);
    r3d_shader_get_location(raster.depthCube, uInstancing);
    r3d_shader_get_location(raster.depthCube, uSkinning);
    r3d_shader_get_location(raster.depthCube, uBillboard);
    r3d_shader_get_location(raster.depthCube, uTexAlbedo);
    r3d_shader_get_location(raster.depthCube, uAlphaCutoff);
    r3d_shader_get_location(raster.depthCube, uViewPosition);
    r3d_shader_get_location(raster.depthCube, uFar);

    r3d_shader_enable(raster.depthCube);
    r3d_shader_set_sampler1D_slot(raster.depthCube, uTexBoneMatrices, 0);
    r3d_shader_set_sampler2D_slot(raster.depthCube, uTexAlbedo, 1);
    r3d_shader_disable();
}

void r3d_shader_load_screen_ssao(void)
{
    R3D.shader.screen.ssao.id = rlLoadShaderCode(
        SCREEN_VERT, SSAO_FRAG
    );

    R3D_SHADER_VALIDATION(screen.ssao);

    r3d_shader_get_location(screen.ssao, uTexDepth);
    r3d_shader_get_location(screen.ssao, uTexNormal);
    r3d_shader_get_location(screen.ssao, uTexKernel);
    r3d_shader_get_location(screen.ssao, uTexNoise);
    r3d_shader_get_location(screen.ssao, uMatInvProj);
    r3d_shader_get_location(screen.ssao, uMatProj);
    r3d_shader_get_location(screen.ssao, uMatView);
    r3d_shader_get_location(screen.ssao, uRadius);
    r3d_shader_get_location(screen.ssao, uBias);
    r3d_shader_get_location(screen.ssao, uIntensity);

    r3d_shader_enable(screen.ssao);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexDepth, 0);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexNormal, 1);
    r3d_shader_set_sampler1D_slot(screen.ssao, uTexKernel, 2);
    r3d_shader_set_sampler2D_slot(screen.ssao, uTexNoise, 3);
    r3d_shader_disable();
}

void r3d_shader_load_screen_ambient_ibl(void)
{
    const char* defines[] = { "#define IBL" };
    char* fsCode = r3d_shader_inject_defines(AMBIENT_FRAG, defines, 1);
    R3D.shader.screen.ambientIbl.id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    R3D_SHADER_VALIDATION(screen.ambientIbl);

    RL_FREE(fsCode);

    r3d_shader_screen_ambient_ibl_t* shader = &R3D.shader.screen.ambientIbl;

    r3d_shader_get_location(screen.ambientIbl, uTexAlbedo);
    r3d_shader_get_location(screen.ambientIbl, uTexNormal);
    r3d_shader_get_location(screen.ambientIbl, uTexDepth);
    r3d_shader_get_location(screen.ambientIbl, uTexSSAO);
    r3d_shader_get_location(screen.ambientIbl, uTexORM);
    r3d_shader_get_location(screen.ambientIbl, uCubeIrradiance);
    r3d_shader_get_location(screen.ambientIbl, uCubePrefilter);
    r3d_shader_get_location(screen.ambientIbl, uTexBrdfLut);
    r3d_shader_get_location(screen.ambientIbl, uQuatSkybox);
    r3d_shader_get_location(screen.ambientIbl, uSkyboxAmbientIntensity);
    r3d_shader_get_location(screen.ambientIbl, uSkyboxReflectIntensity);
    r3d_shader_get_location(screen.ambientIbl, uViewPosition);
    r3d_shader_get_location(screen.ambientIbl, uMatInvProj);
    r3d_shader_get_location(screen.ambientIbl, uMatInvView);
    r3d_shader_get_location(screen.ambientIbl, uSSAOPower);

    r3d_shader_enable(screen.ambientIbl);

    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexSSAO, 3);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexORM, 4);

    r3d_shader_set_samplerCube_slot(screen.ambientIbl, uCubeIrradiance, 5);
    r3d_shader_set_samplerCube_slot(screen.ambientIbl, uCubePrefilter, 6);
    r3d_shader_set_sampler2D_slot(screen.ambientIbl, uTexBrdfLut, 7);

    r3d_shader_disable();
}

void r3d_shader_load_screen_ambient(void)
{
    R3D.shader.screen.ambient.id = rlLoadShaderCode(
        SCREEN_VERT, AMBIENT_FRAG
    );

    R3D_SHADER_VALIDATION(screen.ambient);

    r3d_shader_get_location(screen.ambient, uTexAlbedo);
    r3d_shader_get_location(screen.ambient, uTexSSAO);
    r3d_shader_get_location(screen.ambient, uTexORM);
    r3d_shader_get_location(screen.ambient, uAmbientColor);
    r3d_shader_get_location(screen.ambient, uSSAOPower);

    r3d_shader_enable(screen.ambient);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexSSAO, 1);
    r3d_shader_set_sampler2D_slot(screen.ambient, uTexORM, 2);
    r3d_shader_disable();
}

void r3d_shader_load_screen_lighting(void)
{
    R3D.shader.screen.lighting.id = rlLoadShaderCode(SCREEN_VERT, LIGHTING_FRAG);

    R3D_SHADER_VALIDATION(screen.lighting);

    r3d_shader_screen_lighting_t* shader = &R3D.shader.screen.lighting;

    r3d_shader_get_location(screen.lighting, uTexAlbedo);
    r3d_shader_get_location(screen.lighting, uTexNormal);
    r3d_shader_get_location(screen.lighting, uTexDepth);
    r3d_shader_get_location(screen.lighting, uTexORM);
    r3d_shader_get_location(screen.lighting, uViewPosition);
    r3d_shader_get_location(screen.lighting, uMatInvProj);
    r3d_shader_get_location(screen.lighting, uMatInvView);

    r3d_shader_get_location(screen.lighting, uLight.matVP);
    r3d_shader_get_location(screen.lighting, uLight.shadowMap);
    r3d_shader_get_location(screen.lighting, uLight.shadowCubemap);
    r3d_shader_get_location(screen.lighting, uLight.color);
    r3d_shader_get_location(screen.lighting, uLight.position);
    r3d_shader_get_location(screen.lighting, uLight.direction);
    r3d_shader_get_location(screen.lighting, uLight.specular);
    r3d_shader_get_location(screen.lighting, uLight.energy);
    r3d_shader_get_location(screen.lighting, uLight.range);
    r3d_shader_get_location(screen.lighting, uLight.near);
    r3d_shader_get_location(screen.lighting, uLight.far);
    r3d_shader_get_location(screen.lighting, uLight.attenuation);
    r3d_shader_get_location(screen.lighting, uLight.innerCutOff);
    r3d_shader_get_location(screen.lighting, uLight.outerCutOff);
    r3d_shader_get_location(screen.lighting, uLight.shadowSoftness);
    r3d_shader_get_location(screen.lighting, uLight.shadowMapTxlSz);
    r3d_shader_get_location(screen.lighting, uLight.shadowDepthBias);
    r3d_shader_get_location(screen.lighting, uLight.shadowSlopeBias);
    r3d_shader_get_location(screen.lighting, uLight.type);
    r3d_shader_get_location(screen.lighting, uLight.shadow);

    r3d_shader_enable(screen.lighting);

    r3d_shader_set_sampler2D_slot(screen.lighting, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexNormal, 1);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexDepth, 2);
    r3d_shader_set_sampler2D_slot(screen.lighting, uTexORM, 3);

    r3d_shader_set_sampler2D_slot(screen.lighting, uLight.shadowMap, 4);
    r3d_shader_set_samplerCube_slot(screen.lighting, uLight.shadowCubemap, 5);

    r3d_shader_disable();
}

void r3d_shader_load_screen_scene(void)
{
    R3D.shader.screen.scene.id = rlLoadShaderCode(SCREEN_VERT, SCENE_FRAG);

    R3D_SHADER_VALIDATION(screen.scene);

    r3d_shader_screen_scene_t* shader = &R3D.shader.screen.scene;

    r3d_shader_get_location(screen.scene, uTexAlbedo);
    r3d_shader_get_location(screen.scene, uTexEmission);
    r3d_shader_get_location(screen.scene, uTexDiffuse);
    r3d_shader_get_location(screen.scene, uTexSpecular);
    r3d_shader_get_location(screen.scene, uTexSSAO);
    r3d_shader_get_location(screen.scene, uSSAOPower);
    r3d_shader_get_location(screen.scene, uSSAOLightAffect);

    r3d_shader_enable(screen.scene);

    r3d_shader_set_sampler2D_slot(screen.scene, uTexAlbedo, 0);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexEmission, 1);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexDiffuse, 2);
    r3d_shader_set_sampler2D_slot(screen.scene, uTexSpecular, 3);

    r3d_shader_set_sampler2D_slot(screen.scene, uTexSSAO, 4);

    r3d_shader_disable();
}

void r3d_shader_load_screen_bloom(void)
{
    R3D.shader.screen.bloom.id = rlLoadShaderCode(
        SCREEN_VERT, BLOOM_FRAG
    );

    R3D_SHADER_VALIDATION(screen.bloom);

    r3d_shader_get_location(screen.bloom, uTexColor);
    r3d_shader_get_location(screen.bloom, uTexBloomBlur);
    r3d_shader_get_location(screen.bloom, uBloomMode);
    r3d_shader_get_location(screen.bloom, uBloomIntensity);

    r3d_shader_enable(screen.bloom);
    r3d_shader_set_sampler2D_slot(screen.bloom, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.bloom, uTexBloomBlur, 1);
    r3d_shader_disable();
}

void r3d_shader_load_screen_ssr(void)
{
    R3D.shader.screen.ssr.id = rlLoadShaderCode(
        SCREEN_VERT, SSR_FRAG
    );

    R3D_SHADER_VALIDATION(screen.ssr);

    r3d_shader_get_location(screen.ssr, uTexColor);
    r3d_shader_get_location(screen.ssr, uTexAlbedo);
    r3d_shader_get_location(screen.ssr, uTexNormal);
    r3d_shader_get_location(screen.ssr, uTexORM);
    r3d_shader_get_location(screen.ssr, uTexDepth);
    r3d_shader_get_location(screen.ssr, uMatView);
    r3d_shader_get_location(screen.ssr, uMaxRaySteps);
    r3d_shader_get_location(screen.ssr, uBinarySearchSteps);
    r3d_shader_get_location(screen.ssr, uRayMarchLength);
    r3d_shader_get_location(screen.ssr, uDepthThickness);
    r3d_shader_get_location(screen.ssr, uDepthTolerance);
    r3d_shader_get_location(screen.ssr, uEdgeFadeStart);
    r3d_shader_get_location(screen.ssr, uEdgeFadeEnd);
    r3d_shader_get_location(screen.ssr, uMatInvProj);
    r3d_shader_get_location(screen.ssr, uMatInvView);
    r3d_shader_get_location(screen.ssr, uMatViewProj);
    r3d_shader_get_location(screen.ssr, uViewPosition);

    r3d_shader_enable(screen.ssr);
    r3d_shader_set_sampler2D_slot(screen.ssr, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.ssr, uTexAlbedo, 1);
    r3d_shader_set_sampler2D_slot(screen.ssr, uTexNormal, 2);
    r3d_shader_set_sampler2D_slot(screen.ssr, uTexORM, 3);
    r3d_shader_set_sampler2D_slot(screen.ssr, uTexDepth, 4);
    r3d_shader_disable();
}

void r3d_shader_load_screen_fog(void)
{
    R3D.shader.screen.fog.id = rlLoadShaderCode(
        SCREEN_VERT, FOG_FRAG
    );

    R3D_SHADER_VALIDATION(screen.fog);

    r3d_shader_get_location(screen.fog, uTexColor);
    r3d_shader_get_location(screen.fog, uTexDepth);
    r3d_shader_get_location(screen.fog, uNear);
    r3d_shader_get_location(screen.fog, uFar);
    r3d_shader_get_location(screen.fog, uFogMode);
    r3d_shader_get_location(screen.fog, uFogColor);
    r3d_shader_get_location(screen.fog, uFogStart);
    r3d_shader_get_location(screen.fog, uFogEnd);
    r3d_shader_get_location(screen.fog, uFogDensity);
    r3d_shader_get_location(screen.fog, uSkyAffect);

    r3d_shader_enable(screen.fog);
    r3d_shader_set_sampler2D_slot(screen.fog, uTexColor, 0);
    r3d_shader_set_sampler2D_slot(screen.fog, uTexDepth, 1);
    r3d_shader_disable();
}

void r3d_shader_load_screen_output(R3D_Tonemap tonemap)
{
    assert(R3D.shader.screen.output[tonemap].id == 0);

    const char* defines[] = {
        TextFormat("#define TONEMAPPER %i", tonemap)
    };

    char* fsCode = r3d_shader_inject_defines(OUTPUT_FRAG, defines, 1);
    R3D.shader.screen.output[tonemap].id = rlLoadShaderCode(SCREEN_VERT, fsCode);

    R3D_SHADER_VALIDATION(screen.output[tonemap]);

    RL_FREE(fsCode);

    r3d_shader_get_location(screen.output[tonemap], uTexColor);
    r3d_shader_get_location(screen.output[tonemap], uTonemapExposure);
    r3d_shader_get_location(screen.output[tonemap], uTonemapWhite);
    r3d_shader_get_location(screen.output[tonemap], uBrightness);
    r3d_shader_get_location(screen.output[tonemap], uContrast);
    r3d_shader_get_location(screen.output[tonemap], uSaturation);

    r3d_shader_enable(screen.output[tonemap]);
    r3d_shader_set_sampler2D_slot(screen.output[tonemap], uTexColor, 0);
    r3d_shader_disable();
}

void r3d_shader_load_screen_fxaa(void)
{
    R3D.shader.screen.fxaa.id = rlLoadShaderCode(
        SCREEN_VERT, FXAA_FRAG
    );

    R3D_SHADER_VALIDATION(screen.fxaa);

    r3d_shader_get_location(screen.fxaa, uTexture);
    r3d_shader_get_location(screen.fxaa, uTexelSize);

    r3d_shader_enable(screen.fxaa);
    r3d_shader_set_sampler2D_slot(screen.fxaa, uTexture, 0);
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
