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

#include "../modules/r3d_primitive.h"
#include "../modules/r3d_shader.h"
#include "../modules/r3d_env.h"
#include "../r3d_core_state.h"

// ========================================
// COMMON ENVIRONMENT GENERATION
// ========================================

void r3d_pass_prepare_irradiance(int layerMap, GLuint srcCubemap, int srcSize)
{
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE(prepare.cubemapIrradiance);
    R3D_SHADER_BIND_SAMPLER(prepare.cubemapIrradiance, uSourceTex, srcCubemap);

    R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatProj, matProj);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for (int i = 0; i < 6; i++) {
        r3d_env_irradiance_bind_fbo(layerMap, i);
        R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatView, R3D.matCubeViews[i]);
        R3D_PRIMITIVE_DRAW_CUBE();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void r3d_pass_prepare_prefilter(int layerMap, GLuint srcCubemap, int srcSize)
{
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);
    int srcNumLevels = 1 + (int)floor(log2(srcSize));

    R3D_SHADER_USE(prepare.cubemapPrefilter);
    R3D_SHADER_BIND_SAMPLER(prepare.cubemapPrefilter, uSourceTex, srcCubemap);

    R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatProj, matProj);
    R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uSourceNumLevels, srcNumLevels);
    R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uSourceFaceSize, srcSize);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for (int mip = 0; mip < R3D_ENV_PREFILTER_MIPS; mip++) {
        float roughness = (float)mip / (float)(R3D_ENV_PREFILTER_MIPS - 1);
        R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uRoughness, roughness);

        for (int i = 0; i < 6; i++) {
            r3d_env_prefilter_bind_fbo(layerMap, i, mip);
            R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatView, R3D.matCubeViews[i]);
            R3D_PRIMITIVE_DRAW_CUBE();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}
