/* r3d_target.c -- Internal R3D render target module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_target.h"
#include <r3d_config.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "../common/r3d_helper.h"

// ========================================
// MODULE STATE
// ========================================

struct r3d_mod_target R3D_MOD_TARGET;

// ========================================
// INTERNAL OPENGL FORMAT TABLE
// ========================================

typedef struct {
    GLenum internal;
    GLenum format;
    GLenum type;
} target_format_t;

typedef enum {
    FORMAT_R8U,  FORMAT_RG8U,  FORMAT_RGB8U,  FORMAT_RGBA8U,
    FORMAT_R16U, FORMAT_RG16U, FORMAT_RGB16U, FORMAT_RGBA16U,
    FORMAT_R16F, FORMAT_RG16F, FORMAT_RGB16F, FORMAT_RGBA16F,
    FORMAT_R32F, FORMAT_RG32F, FORMAT_RGB32F, FORMAT_RGBA32F,
} target_format_enum_t;

static const target_format_t TARGET_FORMAT[] = {
    [FORMAT_R8U]     = { GL_R8,        GL_RED,  GL_UNSIGNED_BYTE },
    [FORMAT_RG8U]    = { GL_RG8,       GL_RG,   GL_UNSIGNED_BYTE },
    [FORMAT_RGB8U]   = { GL_RGB8,      GL_RGB,  GL_UNSIGNED_BYTE },
    [FORMAT_RGBA8U]  = { GL_RGBA8,     GL_RGBA, GL_UNSIGNED_BYTE },
    [FORMAT_R16U]    = { GL_R16,       GL_RED,  GL_UNSIGNED_SHORT },
    [FORMAT_RG16U]   = { GL_RG16,      GL_RG,   GL_UNSIGNED_SHORT },
    [FORMAT_RGB16U]  = { GL_RGB16,     GL_RGB,  GL_UNSIGNED_SHORT },
    [FORMAT_RGBA16U] = { GL_RGBA16,    GL_RGBA, GL_UNSIGNED_SHORT },
    [FORMAT_R16F]    = { GL_R16F,      GL_RED,  GL_HALF_FLOAT },
    [FORMAT_RG16F]   = { GL_RG16F,     GL_RG,   GL_HALF_FLOAT },
    [FORMAT_RGB16F]  = { GL_RGB16F,    GL_RGB,  GL_HALF_FLOAT },
    [FORMAT_RGBA16F] = { GL_RGBA16F,   GL_RGBA, GL_HALF_FLOAT },
    [FORMAT_R32F]    = { GL_R32F,      GL_RED,  GL_FLOAT },
    [FORMAT_RG32F]   = { GL_RG32F,     GL_RG,   GL_FLOAT },
    [FORMAT_RGB32F]  = { GL_RGB32F,    GL_RGB,  GL_FLOAT },
    [FORMAT_RGBA32F] = { GL_RGBA32F,   GL_RGBA, GL_FLOAT },
};

// ========================================
// INTERNAL TARGET FUNCTIONS
// ========================================

typedef struct {
    target_format_enum_t format;
    float resolutionFactor;
    GLenum minFilter;
    GLenum magFilter;
    int numLevels;
    float clear[4];
} target_config_t;

static const target_config_t TARGET_CONFIG[] = {
    [R3D_TARGET_ALBEDO]      = { FORMAT_RGB8U,   1.0f, GL_NEAREST,              GL_NEAREST, 2, {0} },
    [R3D_TARGET_NORMAL]      = { FORMAT_RG16U,   1.0f, GL_NEAREST,              GL_NEAREST, 2, {0} },
    [R3D_TARGET_ORM]         = { FORMAT_RGB8U,   1.0f, GL_NEAREST,              GL_NEAREST, 2, {0} },
    [R3D_TARGET_DEPTH]       = { FORMAT_R16F,    1.0f, GL_NEAREST,              GL_NEAREST, 2, {65504.0f, 65504.0f, 65504.0f, 65504.0f} },
    [R3D_TARGET_DIFFUSE]     = { FORMAT_RGB16F,  1.0f, GL_NEAREST,              GL_NEAREST, 2, {0} },
    [R3D_TARGET_SPECULAR]    = { FORMAT_RGB16F,  1.0f, GL_NEAREST,              GL_NEAREST, 2, {0} },
    [R3D_TARGET_GEOM_NORMAL] = { FORMAT_RG16U,   1.0f, GL_NEAREST,              GL_NEAREST, 1, {0} },
    [R3D_TARGET_SSAO_0]      = { FORMAT_R8U,     0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SSAO_1]      = { FORMAT_R8U,     0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SSIL_0]      = { FORMAT_RGBA16F, 0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SSIL_1]      = { FORMAT_RGBA16F, 0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SSIL_2]      = { FORMAT_RGBA16F, 0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SSR]         = { FORMAT_RGBA16F, 0.5f, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,  0, {0} },
    [R3D_TARGET_DOF_COC]     = { FORMAT_RGBA16F, 1.0f, GL_LINEAR,               GL_LINEAR,  2, {0} },
    [R3D_TARGET_DOF_BLUR]    = { FORMAT_RGBA16F, 0.5f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_BLOOM]       = { FORMAT_RGB16F,  0.5f, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR,  0, {0} },
    [R3D_TARGET_SCENE_0]     = { FORMAT_RGB16F,  1.0f, GL_LINEAR,               GL_LINEAR,  1, {0} },
    [R3D_TARGET_SCENE_1]     = { FORMAT_RGB16F,  1.0f, GL_LINEAR,               GL_LINEAR,  1, {0} },
};

static void alloc_target_texture(r3d_target_t target)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TARGET.targetTextures[target]);

    const target_config_t* config = &TARGET_CONFIG[target];
    const target_format_t* format = &TARGET_FORMAT[config->format];

    int numLevels = r3d_target_get_num_levels(target);

    for (int i = 0; i < numLevels; ++i) {
        int wLevel = 0, hLevel = 0;
        r3d_target_get_resolution(&wLevel, &hLevel, target, i);
        glTexImage2D(GL_TEXTURE_2D, i, format->internal, wLevel, hLevel, 0, format->format, format->type, NULL);
    }

    // NOTE: By default, sampling is blocked at the first level
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    R3D_MOD_TARGET.targetStates[target] = (r3d_target_state_t) {0};
    R3D_MOD_TARGET.targetLoaded[target] = true;
}

static void alloc_depth_stencil_renderbuffer(int resW, int resH)
{
    glBindRenderbuffer(GL_RENDERBUFFER, R3D_MOD_TARGET.depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, resW, resH);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

/*
 * Returns the index of the FBO in the cache.
 * If the combination doesn't exist, creates a new FBO and caches it.
 */
static int get_or_create_fbo(const r3d_target_t* targets, int count, bool depth)
{
    assert(targets || (!targets && count == 0));
    assert(count <= R3D_TARGET_MAX_ATTACHMENTS);
    assert(count > 0 || (count == 0 && depth));

    /* --- Search if the combination is already cached --- */

    for (int i = 0; i < R3D_MOD_TARGET.fboCount; i++) {
        const r3d_target_fbo_t* fbo = &R3D_MOD_TARGET.fbo[i];
        if (fbo->targetCount == count && fbo->hasDepth == depth) {
            if (count == 0 || memcmp(fbo->targets, targets, count * sizeof(*targets)) == 0) {
                return i;
            }
        }
    }

    /* --- Create the FBO and cache it --- */

    assert(R3D_MOD_TARGET.fboCount < R3D_TARGET_MAX_FRAMEBUFFERS);

    int newIndex = R3D_MOD_TARGET.fboCount++;
    r3d_target_fbo_t* fbo = &R3D_MOD_TARGET.fbo[newIndex];

    glGenFramebuffers(1, &fbo->id);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->id);

    GLenum glColor[R3D_TARGET_MAX_ATTACHMENTS];
    int locCount = 0;

    for (int i = 0; i < count; ++i) {
        if (!R3D_MOD_TARGET.targetLoaded[targets[i]]) {
            alloc_target_texture(targets[i]);
        }

        GLuint texture = R3D_MOD_TARGET.targetTextures[targets[i]];
        fbo->targetStates[i] = (r3d_target_attachment_state_t) {0};
        fbo->targets[i] = targets[i];

        GLenum attachment = GL_COLOR_ATTACHMENT0 + locCount;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
        glColor[locCount++] = attachment;
    }

    if (depth) {
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
            GL_RENDERBUFFER, R3D_MOD_TARGET.depthRenderbuffer
        );
    }

    fbo->targetCount = count;
    fbo->hasDepth = depth;

    if (locCount > 0) {
        glDrawBuffers(locCount, glColor);
    }
    else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        R3D_TRACELOG(LOG_ERROR, "Framebuffer incomplete (status: 0x%04x)", status);
    }

    return newIndex;
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_target_init(int resW, int resH)
{
    memset(&R3D_MOD_TARGET, 0, sizeof(R3D_MOD_TARGET));

    glGenTextures(R3D_TARGET_COUNT, R3D_MOD_TARGET.targetTextures);
    glGenRenderbuffers(1, &R3D_MOD_TARGET.depthRenderbuffer);
    alloc_depth_stencil_renderbuffer(resW, resH);

    R3D_MOD_TARGET.currentFbo = -1;

    R3D_MOD_TARGET.resW = resW;
    R3D_MOD_TARGET.resH = resH;
    R3D_MOD_TARGET.txlW = 1.0f / resW;
    R3D_MOD_TARGET.txlH = 1.0f / resH;

    return true;
}

void r3d_target_quit(void)
{
    glDeleteTextures(R3D_TARGET_COUNT, R3D_MOD_TARGET.targetTextures);
    glDeleteRenderbuffers(1, &R3D_MOD_TARGET.depthRenderbuffer);

    for (int i = 0; i < R3D_MOD_TARGET.fboCount; i++) {
        if (R3D_MOD_TARGET.fbo[i].id != 0) {
            glDeleteFramebuffers(1, &R3D_MOD_TARGET.fbo[i].id);
        }
    }
}

void r3d_target_resize(int resW, int resH)
{
    assert(resW > 0 && resH > 0);

    if (R3D_MOD_TARGET.resW == resW && R3D_MOD_TARGET.resH == resH) {
        return;
    }

    R3D_MOD_TARGET.resW = resW;
    R3D_MOD_TARGET.resH = resH;
    R3D_MOD_TARGET.txlW = 1.0f / resW;
    R3D_MOD_TARGET.txlH = 1.0f / resH;

    // TODO: Avoid reallocating targets if the new dimensions
    //       are smaller than the allocated dimensions?

    alloc_depth_stencil_renderbuffer(resW, resH);

    for (int i = 0; i < R3D_TARGET_COUNT; i++) {
        if (R3D_MOD_TARGET.targetLoaded[i]) {
            alloc_target_texture(i);
        }
    }
}

int r3d_target_get_num_levels(r3d_target_t target)
{
    const target_config_t* config = &TARGET_CONFIG[target];
    if (config->numLevels > 0) return config->numLevels;

    int w = (int)((float)R3D_MOD_TARGET.resW * config->resolutionFactor);
    int h = (int)((float)R3D_MOD_TARGET.resH * config->resolutionFactor);
    return r3d_get_mip_levels_2d(w, h);
}

void r3d_target_get_resolution(int* w, int* h, r3d_target_t target, int level)
{
    const target_config_t* config = &TARGET_CONFIG[target];

    if (w) *w = (int)((float)R3D_MOD_TARGET.resW * config->resolutionFactor);
    if (h) *h = (int)((float)R3D_MOD_TARGET.resH * config->resolutionFactor);

    if (level > 0) {
        if (w) *w = *w >> level, *w = *w > 1 ? *w : 1;
        if (h) *h = *h >> level, *h = *h > 1 ? *h : 1;
    }
}

void r3d_target_get_texel_size(float* w, float* h, r3d_target_t target, int level)
{
    const target_config_t* config = &TARGET_CONFIG[target];

    if (w) *w = R3D_MOD_TARGET.txlW / config->resolutionFactor;
    if (h) *h = R3D_MOD_TARGET.txlH / config->resolutionFactor;

    if (level > 0) {
        float scale = (float)(1 << level);
        if (w) *w *= scale;
        if (h) *h *= scale;
    }
}

r3d_target_t r3d_target_swap_ssao(r3d_target_t ssao)
{
    if (ssao == R3D_TARGET_SSAO_0) {
        return R3D_TARGET_SSAO_1;
    }
    return R3D_TARGET_SSAO_0;
}

r3d_target_t r3d_target_swap_scene(r3d_target_t scene)
{
    if (scene == R3D_TARGET_SCENE_0) {
        return R3D_TARGET_SCENE_1;
    }
    return R3D_TARGET_SCENE_0;
}

void r3d_target_clear(const r3d_target_t* targets, int count, int level, bool depth)
{
    assert((!depth || level == 0) && "If depth buffer bind, always bind at level zero");
    assert(count > 0 || depth);

    int fboIndex = get_or_create_fbo(targets, count, depth);
    if (fboIndex != R3D_MOD_TARGET.currentFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);
        R3D_MOD_TARGET.currentFbo = fboIndex;
    }

    for (int i = 0; i < count; i++) {
        r3d_target_set_write_level(i, level);
    }

    if (count > 0) r3d_target_set_viewport(targets[0], level);
    else glViewport(0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH);

    for (int i = 0; i < count; i++) {
        glClearBufferfv(GL_COLOR, i, TARGET_CONFIG[targets[i]].clear);
    }

    if (depth) {
        glClearBufferfi(GL_DEPTH_STENCIL, 0, 1.0f, 0);
    }
}

void r3d_target_bind(const r3d_target_t* targets, int count, int level, bool depth)
{
    assert((!depth || level == 0) && "If depth buffer bind, always bind at level zero");
    assert(count > 0 || depth);

    int fboIndex = get_or_create_fbo(targets, count, depth);
    if (fboIndex != R3D_MOD_TARGET.currentFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);
        R3D_MOD_TARGET.currentFbo = fboIndex;
    }

    for (int i = 0; i < count; i++) {
        r3d_target_set_write_level(i, level);
    }

    if (count > 0) r3d_target_set_viewport(targets[0], level);
    else glViewport(0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH);
}

void r3d_target_set_viewport(r3d_target_t target, int level)
{
    int vpW = 0, vpH = 0;
    r3d_target_get_resolution(&vpW, &vpH, target, level);
    glViewport(0, 0, vpW, vpH);
}

void r3d_target_set_write_level(int attachment, int level)
{
    assert(R3D_MOD_TARGET.currentFbo >= 0);

    r3d_target_fbo_t* fbo = &R3D_MOD_TARGET.fbo[R3D_MOD_TARGET.currentFbo];
    assert(fbo->targetCount > 0 && attachment < fbo->targetCount);

    r3d_target_t target = fbo->targets[attachment];
    assert(level < r3d_target_get_num_levels(target));
    r3d_target_attachment_state_t* state = &fbo->targetStates[attachment];

    if (state->writeLevel != level) {
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment,
            GL_TEXTURE_2D, R3D_MOD_TARGET.targetTextures[target], level
        );
        state->writeLevel = level;
    }
}

void r3d_target_set_read_level(r3d_target_t target, int level)
{
    return r3d_target_set_read_levels(target, level, level);
}

void r3d_target_set_read_levels(r3d_target_t target, int baseLevel, int maxLevel)
{
    assert(R3D_MOD_TARGET.targetLoaded[target]);
    assert(baseLevel < r3d_target_get_num_levels(target));
    assert(maxLevel < r3d_target_get_num_levels(target));

    r3d_target_state_t* state = &R3D_MOD_TARGET.targetStates[target];

    if (state->baseLevel != baseLevel || state->maxLevel != maxLevel) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, R3D_MOD_TARGET.targetTextures[target]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, baseLevel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL,  maxLevel);
        glBindTexture(GL_TEXTURE_2D, 0);
        state->baseLevel = baseLevel;
        state->maxLevel = maxLevel;
    }
}

void r3d_target_gen_mipmap(r3d_target_t target)
{
    GLuint id = r3d_target_get(target);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint r3d_target_get(r3d_target_t target)
{
    assert(target > R3D_TARGET_INVALID && target < R3D_TARGET_COUNT);
    assert(R3D_MOD_TARGET.targetLoaded[target]);
    return R3D_MOD_TARGET.targetTextures[target];
}

GLuint r3d_target_get_level(r3d_target_t target, int level)
{
    return r3d_target_get_levels(target, level, level);
}

GLuint r3d_target_get_levels(r3d_target_t target, int baseLevel, int maxLevel)
{
    assert(target > R3D_TARGET_INVALID && target < R3D_TARGET_COUNT);
    r3d_target_set_read_levels(target, baseLevel, maxLevel);
    return R3D_MOD_TARGET.targetTextures[target];
}

GLuint r3d_target_get_all_levels(r3d_target_t target)
{
    assert(target > R3D_TARGET_INVALID && target < R3D_TARGET_COUNT);
    int maxLevel = r3d_target_get_num_levels(target) - 1;
    r3d_target_set_read_levels(target, 0, maxLevel);
    return R3D_MOD_TARGET.targetTextures[target];
}

GLuint r3d_target_get_or_null(r3d_target_t target)
{
    if (target <= R3D_TARGET_INVALID || target >= R3D_TARGET_COUNT) return 0;
    if (!R3D_MOD_TARGET.targetLoaded[target]) return 0;
    return R3D_MOD_TARGET.targetTextures[target];
}

void r3d_target_blit(r3d_target_t target, bool depth, GLuint dstFbo, int dstX, int dstY, int dstW, int dstH, bool linear)
{
    int fboIndex = get_or_create_fbo(&target, 1, depth);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);

    if (linear) {
        if (target > 0) {
            glBlitFramebuffer(
                0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH,
                dstX, dstY, dstX + dstW, dstY + dstH, GL_COLOR_BUFFER_BIT,
                GL_LINEAR
            );
        }
        if (depth) {
            glBlitFramebuffer(
                0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH,
                dstX, dstY, dstX + dstW, dstY + dstH, GL_DEPTH_BUFFER_BIT,
                GL_NEAREST
            );
        }
    }
    else {
        GLbitfield mask = GL_NONE;
        if (target > 0) mask |= GL_COLOR_BUFFER_BIT;
        if (depth) mask |= GL_DEPTH_BUFFER_BIT;
        glBlitFramebuffer(
            0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH,
            dstX, dstY, dstX + dstW, dstY + dstH, mask,
            GL_NEAREST
        );
    }
}

void r3d_target_reset(void)
{
    R3D_MOD_TARGET.currentFbo = -1;
}
