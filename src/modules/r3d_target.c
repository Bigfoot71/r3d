/* r3d_target.c -- Internal R3D render target module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_target.h"
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <math.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_target R3D_MOD_TARGET;

// ========================================
// HELPER FUNCTIONS
// ========================================

static int get_mip_count(int w, int h)
{
    return 1 + (int)floorf(log2f(w > h ? w : h));
}

// ========================================
// INTERNAL TARGET FUNCTIONS
// ========================================

typedef struct {
    GLenum internalFormat, format, type;
    float resolutionFactor;
    GLenum minFilter;
    GLenum magFilter;
    bool mipmaps;
} target_config_t;

static const target_config_t TARGET_CONFIG[] = {
    [R3D_TARGET_ALBEDO]     = { GL_RGB8,              GL_RGB,             GL_UNSIGNED_BYTE,  1.0f, GL_NEAREST,               GL_NEAREST, false },
    [R3D_TARGET_NORMAL]     = { GL_RG16F,             GL_RG,              GL_HALF_FLOAT,     1.0f, GL_NEAREST,               GL_NEAREST, false },
    [R3D_TARGET_ORM]        = { GL_RGB8,              GL_RGB,             GL_UNSIGNED_BYTE,  1.0f, GL_NEAREST,               GL_NEAREST, false },
    [R3D_TARGET_DIFFUSE]    = { GL_RGB16F,            GL_RGB,             GL_HALF_FLOAT,     1.0f, GL_NEAREST,               GL_NEAREST, false },
    [R3D_TARGET_SPECULAR]   = { GL_RGB16F,            GL_RGB,             GL_HALF_FLOAT,     1.0f, GL_NEAREST,               GL_NEAREST, false },
    [R3D_TARGET_SSAO_0]     = { GL_R8,                GL_RED,             GL_UNSIGNED_BYTE,  0.5f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_SSAO_1]     = { GL_R8,                GL_RED,             GL_UNSIGNED_BYTE,  0.5f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_SSIL_0]     = { GL_RGBA16F,           GL_RGBA,            GL_HALF_FLOAT,     0.5f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_SSIL_1]     = { GL_RGBA16F,           GL_RGBA,            GL_HALF_FLOAT,     0.5f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_SSIL_MIP]   = { GL_RGBA16F,           GL_RGBA,            GL_HALF_FLOAT,     0.5f, GL_LINEAR_MIPMAP_LINEAR,  GL_LINEAR,  true  },
    [R3D_TARGET_SSR]        = { GL_RGBA16F,           GL_RGBA,            GL_HALF_FLOAT,     0.5f, GL_LINEAR_MIPMAP_LINEAR,  GL_LINEAR,  true  },
    [R3D_TARGET_BLOOM]      = { GL_RGB16F,            GL_RGB,             GL_HALF_FLOAT,     0.5f, GL_LINEAR_MIPMAP_LINEAR,  GL_LINEAR,  true  },
    [R3D_TARGET_SCENE_0]    = { GL_RGB16F,            GL_RGB,             GL_HALF_FLOAT,     1.0f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_SCENE_1]    = { GL_RGB16F,            GL_RGB,             GL_HALF_FLOAT,     1.0f, GL_LINEAR,                GL_LINEAR,  false },
    [R3D_TARGET_DEPTH]      = { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,   1.0f, GL_NEAREST,               GL_NEAREST, false },
};

static void target_load(r3d_target_t target)
{
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TARGET.targets[target]);
    const target_config_t* config = &TARGET_CONFIG[target];
    int mipCount = r3d_target_get_mip_count(target);

    for (int i = 0; i < mipCount; ++i) {
        int wLevel = 0, hLevel = 0;
        r3d_target_get_resolution(&wLevel, &hLevel, target, i);
        glTexImage2D(GL_TEXTURE_2D, i, config->internalFormat, wLevel, hLevel, 0, config->format, config->type, NULL);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config->minFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config->magFilter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    R3D_MOD_TARGET.targetLoaded[target] = true;
}

/*
 * Returns the index of the FBO in the cache.
 * If the combination doesn't exist, creates a new FBO and caches it.
 */
static int get_or_create_fbo(const r3d_target_t* targets, int count)
{
    assert(count < R3D_TARGET_MAX_ATTACHMENTS);

    /* --- Search if the combination is already cached --- */

    for (int i = 0; i < R3D_MOD_TARGET.fboCount; i++) {
        const r3d_target_fbo_t* fbo = &R3D_MOD_TARGET.fbo[i];
        if (fbo->count == count && memcmp(fbo->targets, targets, count * sizeof(*targets)) == 0) {
            return i;
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

    for (int i = 0; i < count; ++i)
    {
        if (!R3D_MOD_TARGET.targetLoaded[targets[i]]) {
            target_load(targets[i]);
        }

        GLuint texture = R3D_MOD_TARGET.targets[targets[i]];
        fbo->targets[i] = targets[i];

        if (targets[i] != R3D_TARGET_DEPTH) {
            GLenum attachment = GL_COLOR_ATTACHMENT0 + locCount;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture, 0);
            glColor[locCount++] = attachment;
        }
        else {
            assert(i == count - 1); // Always provide the depth target last
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
        }
    }

    fbo->count = count;

    if (locCount > 0) {
        glDrawBuffers(locCount, glColor);
    }
    else {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        TraceLog(LOG_ERROR, "R3D: Framebuffer incomplete (status: 0x%04x)", status);
    }

    return newIndex;
}

static void set_viewport(r3d_target_t target)
{
    float viewportFactor = TARGET_CONFIG[target].resolutionFactor;
    int vpW = (int)((float)R3D_MOD_TARGET.resW * viewportFactor);
    int vpH = (int)((float)R3D_MOD_TARGET.resH * viewportFactor);
    glViewport(0, 0, vpW, vpH);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_target_init(int resW, int resH)
{
    memset(&R3D_MOD_TARGET, 0, sizeof(R3D_MOD_TARGET));

    glGenTextures(R3D_TARGET_COUNT, R3D_MOD_TARGET.targets);

    R3D_MOD_TARGET.currentFbo = -1;

    R3D_MOD_TARGET.resW = resW;
    R3D_MOD_TARGET.resH = resH;

    R3D_MOD_TARGET.txlW = 1.0f / resW;
    R3D_MOD_TARGET.txlH = 1.0f / resH;

    return true;
}

void r3d_target_quit(void)
{
    glDeleteTextures(R3D_TARGET_COUNT, R3D_MOD_TARGET.targets);

    for (int i = 0; i < R3D_MOD_TARGET.fboCount; i++) {
        glDeleteFramebuffers(1, &R3D_MOD_TARGET.fbo[i].id);
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

    for (int i = 0; i < R3D_TARGET_COUNT; i++) {
        if (R3D_MOD_TARGET.targetLoaded[i]) {
            target_load(i);
        }
    }
}

int r3d_target_get_mip_count(r3d_target_t target)
{
    const target_config_t* config = &TARGET_CONFIG[target];
    if (!config->mipmaps) return 1;

    int w = (int)((float)R3D_MOD_TARGET.resW * config->resolutionFactor);
    int h = (int)((float)R3D_MOD_TARGET.resH * config->resolutionFactor);
    return 1 + (int)floorf(log2f(w > h ? w : h));
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

void r3d_target_clear(const r3d_target_t* targets, int count)
{
    int fboIndex = get_or_create_fbo(targets, count);
    if (fboIndex != R3D_MOD_TARGET.currentFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);
        R3D_MOD_TARGET.currentFbo = fboIndex;
        set_viewport(targets[0]);
    }

    bool hasDepth = false;
    bool hasColor = false;
    for (int i = 0; i < count; i++) {
        hasDepth |= (targets[i] == R3D_TARGET_DEPTH);
        hasColor |= (targets[i] != R3D_TARGET_DEPTH);
    }

    assert(hasDepth || hasColor);
    GLenum bitfield = GL_NONE;

    if (hasColor) {
        bitfield |= GL_COLOR_BUFFER_BIT;
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }

    if (hasDepth) {
        bitfield |= GL_DEPTH_BUFFER_BIT;
        glDepthMask(GL_TRUE);
        glClearDepth(1.0f);
    }

    glClear(bitfield);
}

void r3d_target_bind(const r3d_target_t* targets, int count)
{
    int fboIndex = get_or_create_fbo(targets, count);
    if (fboIndex != R3D_MOD_TARGET.currentFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);
        R3D_MOD_TARGET.currentFbo = fboIndex;
        set_viewport(targets[0]);
    }
}

void r3d_target_set_mip_level(int attachment, int level)
{
    assert(R3D_MOD_TARGET.currentFbo >= 0);

    const r3d_target_fbo_t* fbo = &R3D_MOD_TARGET.fbo[R3D_MOD_TARGET.currentFbo];
    r3d_target_t target = fbo->targets[attachment];

    assert(TARGET_CONFIG[target].mipmaps);

    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + attachment,
        GL_TEXTURE_2D, R3D_MOD_TARGET.targets[target], level
    );
}

void r3d_target_gen_mipmap(r3d_target_t target)
{
    GLuint id = r3d_target_get(target);

    glBindTexture(GL_TEXTURE_2D, id);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint r3d_target_get(r3d_target_t target)
{
    assert(target > R3D_TARGET_INVALID && target < R3D_TARGET_COUNT);
    assert(R3D_MOD_TARGET.targetLoaded[target]);
    return R3D_MOD_TARGET.targets[target];
}

GLuint r3d_target_get_or_null(r3d_target_t target)
{
    if (target <= R3D_TARGET_INVALID || target >= R3D_TARGET_COUNT) return 0;
    if (!R3D_MOD_TARGET.targetLoaded[target]) return 0;
    return R3D_MOD_TARGET.targets[target];
}

void r3d_target_blit(r3d_target_t* targets, int count, GLuint dstFbo, int dstX, int dstY, int dstW, int dstH, bool linear)
{
    bool hasDepth = (targets[count - 1] == R3D_TARGET_DEPTH);
    bool depthOnly = (count == 1 && hasDepth);

    int fboIndex = get_or_create_fbo(targets, count);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, R3D_MOD_TARGET.fbo[fboIndex].id);

    if (linear) {
        if (!depthOnly) {
            glBlitFramebuffer(
                0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH,
                dstX, dstY, dstX + dstW, dstY + dstH, GL_COLOR_BUFFER_BIT,
                GL_LINEAR
            );
        }
        if (hasDepth) {
            glBlitFramebuffer(
                0, 0, R3D_MOD_TARGET.resW, R3D_MOD_TARGET.resH,
                dstX, dstY, dstX + dstW, dstY + dstH, GL_DEPTH_BUFFER_BIT,
                GL_NEAREST
            );
        }
    }
    else {
        GLbitfield mask = GL_NONE;
        if (!depthOnly) mask |= GL_COLOR_BUFFER_BIT;
        if (hasDepth) mask |= GL_DEPTH_BUFFER_BIT;
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
