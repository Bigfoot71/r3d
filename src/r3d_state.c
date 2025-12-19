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
    glGenTextures(1, &R3D.texture.iblBrdfLut);
    glBindTexture(GL_TEXTURE_2D, R3D.texture.iblBrdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_HALF_FLOAT, BRDF_LUT_512_RG16_FLOAT_RAW);

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
