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

/* === Assets === */

#include <assets/brdf_lut_512_rg16_float.raw.h>

/* === Global state definition === */

struct R3D_State R3D = { 0 };

/* === Helper functions === */

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
