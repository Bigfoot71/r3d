/* r3d_pass.c -- Common R3D Passes
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_pass.h"
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include "../modules/r3d_shader.h"
#include "../modules/r3d_draw.h"
#include "../modules/r3d_env.h"
#include "../r3d_core_state.h"

// ========================================
// COMMON ENVIRONMENT GENERATION
// ========================================

void r3d_pass_prepare_irradiance(int layerMap, GLuint srcCubemap, int srcSize)
{
    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE(prepare.cubemapIrradiance);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    R3D_SHADER_BIND_SAMPLER(prepare.cubemapIrradiance, uSourceTex, srcCubemap);
    R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatProj, matProj);

    for (int i = 0; i < 6; i++) {
        r3d_env_irradiance_bind_fbo(layerMap, i);
        R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatView, R3D.matCubeViews[i]);
        R3D_DRAW_CUBE();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void r3d_pass_prepare_prefilter(int layerMap, GLuint srcCubemap, int srcSize)
{
    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);
    int srcNumLevels = r3d_get_mip_levels_1d(srcSize);

    R3D_SHADER_USE(prepare.cubemapPrefilter);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    R3D_SHADER_BIND_SAMPLER(prepare.cubemapPrefilter, uSourceTex, srcCubemap);

    R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uSourceNumLevels, srcNumLevels);
    R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uSourceFaceSize, srcSize);
    R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatProj, matProj);

    for (int mip = 0; mip < R3D_ENV_PREFILTER_MIPS; mip++) {
        float roughness = (float)mip / (float)(R3D_ENV_PREFILTER_MIPS - 1);
        R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uRoughness, roughness);

        for (int i = 0; i < 6; i++) {
            r3d_env_prefilter_bind_fbo(layerMap, i, mip);
            R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatView, R3D.matCubeViews[i]);
            R3D_DRAW_CUBE();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
