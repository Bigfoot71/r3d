/* r3d_texture.c -- Internal R3D texture module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_texture.h"
#include <raymath.h>
#include <stdint.h>
#include <string.h>
#include <glad.h>

#include "../details/misc/r3d_half.h"

// ========================================
// TEXTURE DATA INCLUDES
// ========================================

#include <assets/brdf_lut_512_rg16_float.raw.h>

// ========================================
// MODULE STATE
// ========================================

static struct r3d_mod_texture {
    GLuint textures[R3D_TEXTURE_COUNT];
    bool loaded[R3D_TEXTURE_COUNT];
} R3D_MOD_TEXTURE;

// ========================================
// TEXTURE LOADERS
// ========================================

typedef void (*texture_loader_func)(void);

static void load_white(void);
static void load_black(void);
static void load_normal(void);
static void load_ssao_noise(void);
static void load_ssao_kernel(void);
static void load_ibl_brdf_lut(void);

static const texture_loader_func LOADERS[] = {
    [R3D_TEXTURE_WHITE] = load_white,
    [R3D_TEXTURE_BLACK] = load_black,
    [R3D_TEXTURE_NORMAL] = load_normal,
    [R3D_TEXTURE_SSAO_NOISE] = load_ssao_noise,
    [R3D_TEXTURE_SSAO_KERNEL] = load_ssao_kernel,
    [R3D_TEXTURE_IBL_BRDF_LUT] = load_ibl_brdf_lut
};

static void load_white(void)
{
    const uint8_t PIXELS[4] = {255, 255, 255, 255};

    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_WHITE]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, PIXELS);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void load_black(void)
{
    const uint8_t PIXELS[4] = {0, 0, 0, 255};

    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_BLACK]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, PIXELS);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void load_normal(void)
{
    const uint8_t PIXELS[4] = {127, 127, 255, 0};

    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_NORMAL]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, PIXELS);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void load_ssao_noise(void)
{
#   define R3D_RAND_NOISE_RESOLUTION 4

    r3d_half_t noise[2 * R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION];

    for (int i = 0; i < R3D_RAND_NOISE_RESOLUTION * R3D_RAND_NOISE_RESOLUTION; i++) {
        noise[i * 2 + 0] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
        noise[i * 2 + 1] = r3d_cvt_fh(((float)GetRandomValue(0, INT16_MAX) / INT16_MAX) * 2.0f - 1.0f);
    }

    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_SSAO_NOISE]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, R3D_RAND_NOISE_RESOLUTION, R3D_RAND_NOISE_RESOLUTION, 0, GL_RG, GL_HALF_FLOAT, noise);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

static void load_ssao_kernel(void)
{
#   define R3D_SSAO_KERNEL_SIZE 32

    r3d_half_t kernel[3 * R3D_SSAO_KERNEL_SIZE];

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

    glBindTexture(GL_TEXTURE_1D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_SSAO_KERNEL]);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB16F, R3D_SSAO_KERNEL_SIZE, 0, GL_RGB, GL_HALF_FLOAT, kernel);

    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);
}

static void load_ibl_brdf_lut(void)
{
    glBindTexture(GL_TEXTURE_2D, R3D_MOD_TEXTURE.textures[R3D_TEXTURE_IBL_BRDF_LUT]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_HALF_FLOAT, BRDF_LUT_512_RG16_FLOAT_RAW);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

// ========================================
// TEXTURE FUNCTIONS
// ========================================

bool r3d_texture_is_default(GLuint id)
{
    for (int i = 0; i < R3D_TEXTURE_COUNT; i++) {
        if (id == R3D_MOD_TEXTURE.textures[i]) {
            return true;
        }
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

// ========================================
// MODULE FUNCTIONS
// ========================================

void r3d_mod_texture_init(void)
{
    memset(&R3D_MOD_TEXTURE, 0, sizeof(R3D_MOD_TEXTURE));
    glGenTextures(R3D_TEXTURE_COUNT, R3D_MOD_TEXTURE.textures);
}

void r3d_mod_texture_quit(void)
{
    glDeleteTextures(R3D_TEXTURE_COUNT, R3D_MOD_TEXTURE.textures);
}
