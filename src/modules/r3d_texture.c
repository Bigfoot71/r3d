/* r3d_texture.c -- Internal R3D texture module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_texture.h"
#include <raymath.h>
#include <stdint.h>
#include <string.h>
#include <glad.h>

// ========================================
// TEXTURE DATA INCLUDES
// ========================================

#include <assets/brdf_lut_512_rg16_float.raw.h>
#include <assets/ssgi_lut_4096_rgba16_float.raw.h>

// ========================================
// MODULE STATE
// ========================================

static struct r3d_texture {
    GLuint textures[R3D_TEXTURE_COUNT];
    bool loaded[R3D_TEXTURE_COUNT];
} R3D_MOD_TEXTURE;

// ========================================
// UTILITY FUNCTIONS
// ========================================

static void tex_params(GLenum target, GLint filter, GLint wrap)
{
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
    if (target != GL_TEXTURE_1D) {
        glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
    }
}

// ========================================
// TEXTURE LOADERS
// ========================================

typedef void (*texture_loader_func)(void);

static void load_white(void);
static void load_black(void);
static void load_blank(void);
static void load_normal(void);
static void load_ibl_brdf_lut(void);
static void load_ssgi_lut(void);

static const texture_loader_func LOADERS[] = {
    [R3D_TEXTURE_WHITE] = load_white,
    [R3D_TEXTURE_BLACK] = load_black,
    [R3D_TEXTURE_BLANK] = load_blank,
    [R3D_TEXTURE_NORMAL] = load_normal,
    [R3D_TEXTURE_BRDF_LUT] = load_ibl_brdf_lut,
    [R3D_TEXTURE_SSGI_LUT] = load_ssgi_lut
};

void load_white(void) {
    const uint8_t px[4] = {255, 255, 255, 255};
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_WHITE]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    tex_params(GL_TEXTURE_2D, GL_NEAREST, GL_CLAMP_TO_EDGE);
}

void load_black(void) {
    const uint8_t px[4] = {0, 0, 0, 255};
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_BLACK]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    tex_params(GL_TEXTURE_2D, GL_NEAREST, GL_CLAMP_TO_EDGE);
}

void load_blank(void) {
    const uint8_t px[4] = {0, 0, 0, 0};
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_BLANK]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    tex_params(GL_TEXTURE_2D, GL_NEAREST, GL_CLAMP_TO_EDGE);
}

void load_normal(void) {
    const uint8_t px[4] = {127, 127, 255, 0};
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_NORMAL]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    tex_params(GL_TEXTURE_2D, GL_NEAREST, GL_CLAMP_TO_EDGE);
}

void load_ibl_brdf_lut(void) {
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_BRDF_LUT]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_HALF_FLOAT, BRDF_LUT_512_RG16_FLOAT_RAW);
    tex_params(GL_TEXTURE_2D, GL_LINEAR, GL_CLAMP_TO_EDGE);
}

void load_ssgi_lut(void) {
    glBindTexture(GL_TEXTURE_1D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_SSGI_LUT]);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, 4096, 0, GL_RGBA, GL_HALF_FLOAT, SSGI_LUT_4096_RGBA16_FLOAT_RAW);
    tex_params(GL_TEXTURE_1D, GL_NEAREST, GL_CLAMP_TO_EDGE);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_texture_init(void)
{
    memset(&R3D_MOD_TEXTURE, 0, sizeof(R3D_MOD_TEXTURE));
    glGenTextures(R3D_TEXTURE_COUNT, R3D_MOD_TEXTURE.textures);
    return true;
}

void r3d_texture_quit(void)
{
    glDeleteTextures(R3D_TEXTURE_COUNT, R3D_MOD_TEXTURE.textures);
}

bool r3d_texture_is_default(GLuint id)
{
    for (int i = 0; i < R3D_TEXTURE_COUNT; i++) {
        if (id == R3D_MOD_TEXTURE.textures[i]) return true;
    }
    return false;
}

GLuint r3d_texture_get(r3d_texture_t texture)
{
    if (!R3D_MOD_TEXTURE.loaded[texture]) {
        R3D_MOD_TEXTURE.loaded[texture] = true;
        LOADERS[texture]();
    }
    return R3D_MOD_TEXTURE.textures[texture];
}
