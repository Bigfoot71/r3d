/* r3d_pass.c -- Common R3D Passes
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_pass.h"
#include <r3d_config.h>
#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include "../r3d_core_state.h"

#include "../common/r3d_helper.h"

#include "../modules/r3d_driver.h"
#include "../modules/r3d_shader.h"
#include "../modules/r3d_draw.h"
#include "../modules/r3d_env.h"

// ========================================
// COMMON ENVIRONMENT GENERATION
// ========================================

void r3d_pass_prepare_irradiance(int layerMap, GLuint srcCubemap, int srcSize, bool invalidateCache)
{
    if (invalidateCache) {
        r3d_driver_invalidate_cache();
    }

    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE_BLT(prepare.cubemapIrradiance);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);

    R3D_SHADER_BIND_SAMPLER_BLT(prepare.cubemapIrradiance, uSourceTex, srcCubemap);
    R3D_SHADER_SET_MAT4_BLT(prepare.cubemapIrradiance, uMatProj, matProj);

    for (int i = 0; i < 6; i++) {
        r3d_env_irradiance_bind_fbo(layerMap, i);
        R3D_SHADER_SET_MAT4_BLT(prepare.cubemapIrradiance, uMatView, R3D.matCubeViews[i]);
        R3D_DRAW_CUBE();
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r3d_driver_enable(GL_CULL_FACE);
}

void r3d_pass_prepare_prefilter(int layerMap, GLuint srcCubemap, int srcSize, bool invalidateCache)
{
    if (invalidateCache) {
        r3d_driver_invalidate_cache();
    }

    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);
    int srcNumLevels = r3d_get_mip_levels_1d(srcSize);

    R3D_SHADER_USE_BLT(prepare.cubemapPrefilter);
    r3d_driver_disable(GL_DEPTH_TEST);
    r3d_driver_disable(GL_CULL_FACE);

    R3D_SHADER_BIND_SAMPLER_BLT(prepare.cubemapPrefilter, uSourceTex, srcCubemap);

    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapPrefilter, uSourceNumLevels, srcNumLevels);
    R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapPrefilter, uSourceFaceSize, srcSize);
    R3D_SHADER_SET_MAT4_BLT(prepare.cubemapPrefilter, uMatProj, matProj);

    int numLevels = r3d_get_mip_levels_1d(R3D_CUBEMAP_PREFILTER_SIZE);

    for (int mip = 0; mip < numLevels; mip++) {
        float roughness = (float)mip / (float)(numLevels - 1);
        R3D_SHADER_SET_FLOAT_BLT(prepare.cubemapPrefilter, uRoughness, roughness);

        for (int i = 0; i < 6; i++) {
            r3d_env_prefilter_bind_fbo(layerMap, i, mip);
            R3D_SHADER_SET_MAT4_BLT(prepare.cubemapPrefilter, uMatView, R3D.matCubeViews[i]);
            R3D_DRAW_CUBE();
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r3d_driver_enable(GL_CULL_FACE);
}
