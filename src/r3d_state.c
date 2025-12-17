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

/* === Global state definition === */

struct R3D_State R3D = { 0 };

/* === Internal Functions === */

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
