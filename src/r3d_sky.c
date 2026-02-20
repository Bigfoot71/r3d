/* r3d_cubemap.c -- R3D Cubemap Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_cubemap.h>
#include <r3d/r3d_sky.h>
#include <raymath.h>
#include <stdlib.h>
#include <stddef.h>
#include <rlgl.h>
#include <glad.h>

#include "./modules/r3d_driver.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_render.h"
#include "./r3d_core_state.h"

// ========================================
// EXTERNAL FUNCTIONS
// ========================================

R3D_Cubemap r3d_cubemap_allocate(int size);
void r3d_cubemap_gen_mipmap(const R3D_Cubemap* cubemap);

// ========================================
// PUBLIC API
// ========================================

R3D_Cubemap R3D_GenProceduralSky(int size, R3D_ProceduralSky params)
{
    R3D_Cubemap cubemap = r3d_cubemap_allocate(size);
    R3D_UpdateProceduralSky(&cubemap, params);
    return cubemap;
}

void R3D_UpdateProceduralSky(R3D_Cubemap* cubemap, R3D_ProceduralSky params)
{
    r3d_driver_invalidate_cache();

    Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    glBindFramebuffer(GL_FRAMEBUFFER, cubemap->fbo);
    glViewport(0, 0, cubemap->size, cubemap->size);

    R3D_SHADER_USE(prepare.cubemapSkybox);
    r3d_driver_disable(GL_CULL_FACE);

    R3D_SHADER_SET_MAT4(prepare.cubemapSkybox, uMatProj, matProj);
    R3D_SHADER_SET_COL3(prepare.cubemapSkybox, uSkyTopColor, R3D.colorSpace, params.skyTopColor);
    R3D_SHADER_SET_COL3(prepare.cubemapSkybox, uSkyHorizonColor, R3D.colorSpace, params.skyHorizonColor);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uSkyHorizonCurve, params.skyHorizonCurve);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uSkyEnergy, params.skyEnergy);
    R3D_SHADER_SET_COL3(prepare.cubemapSkybox, uGroundBottomColor, R3D.colorSpace, params.groundBottomColor);
    R3D_SHADER_SET_COL3(prepare.cubemapSkybox, uGroundHorizonColor, R3D.colorSpace, params.groundHorizonColor);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uGroundHorizonCurve, params.groundHorizonCurve);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uGroundEnergy, params.groundEnergy);
    R3D_SHADER_SET_VEC3(prepare.cubemapSkybox, uSunDirection, Vector3Normalize(Vector3Negate(params.sunDirection)));
    R3D_SHADER_SET_COL3(prepare.cubemapSkybox, uSunColor, R3D.colorSpace, params.sunColor);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uSunSize, params.sunSize);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uSunCurve, params.sunCurve);
    R3D_SHADER_SET_FLOAT(prepare.cubemapSkybox, uSunEnergy, params.sunEnergy);

    for (int i = 0; i < 6; i++) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap->texture, 0);
        glClear(GL_DEPTH_BUFFER_BIT);

        R3D_SHADER_SET_MAT4(prepare.cubemapSkybox, uMatView, R3D.matCubeViews[i]);
        R3D_RENDER_CUBE();
    }

    r3d_cubemap_gen_mipmap(cubemap);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r3d_driver_enable(GL_CULL_FACE);
}
