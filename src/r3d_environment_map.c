/* r3d_environment_map.c -- R3D Environment Map Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_environment_map.h>
#include <r3d/r3d_cubemap.h>
#include <raymath.h>
#include <stddef.h>
#include <rlgl.h>
#include <glad.h>

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_cache.h"
#include "./modules/r3d_env.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static void gen_irradiance(int layerMap, R3D_Cubemap cubemap)
{
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE(prepare.cubemapIrradiance);
    R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatProj, matProj);
    R3D_SHADER_BIND_SAMPLER_CUBE(prepare.cubemapIrradiance, uCubemap, cubemap.texture);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for (int i = 0; i < 6; i++) {
        r3d_env_bind_irradiance_fbo(layerMap, i);
        R3D_SHADER_SET_MAT4(prepare.cubemapIrradiance, uMatView, R3D_CACHE_GET(matCubeViews[i]));
        R3D_PRIMITIVE_DRAW_CUBE();
    }

    R3D_SHADER_UNBIND_SAMPLER_CUBE(prepare.cubemapIrradiance, uCubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

static void gen_prefilter(int layerMap, R3D_Cubemap cubemap)
{
    const Matrix matProj = MatrixPerspective(90.0 * DEG2RAD, 1.0, 0.1, 10.0);

    R3D_SHADER_USE(prepare.cubemapPrefilter);
    R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatProj, matProj);
    R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uResolution, cubemap.size);
    R3D_SHADER_BIND_SAMPLER_CUBE(prepare.cubemapPrefilter, uCubemap, cubemap.texture);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    for (int mip = 0; mip < R3D_ENV_PREFILTER_MIPS; mip++)
    {
        float roughness = (float)mip / (float)(R3D_ENV_PREFILTER_MIPS - 1);
        R3D_SHADER_SET_FLOAT(prepare.cubemapPrefilter, uRoughness, roughness);

        for (int i = 0; i < 6; i++) {
            r3d_env_bind_prefilter_fbo(layerMap, i, mip);
            R3D_SHADER_SET_MAT4(prepare.cubemapPrefilter, uMatView, R3D_CACHE_GET(matCubeViews[i]));
            R3D_PRIMITIVE_DRAW_CUBE();
        }
    }

    R3D_SHADER_UNBIND_SAMPLER_CUBE(prepare.cubemapPrefilter, uCubemap);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, rlGetFramebufferWidth(), rlGetFramebufferHeight());
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

// ========================================
// PUBLIC API
// ========================================

R3D_EnvironmentMap R3D_GenEnvironmentMap(R3D_Cubemap cubemap)
{
    R3D_EnvironmentMap envmap = {0};

    int irradiance = r3d_env_reserve_irradiance_layer();
    if (irradiance < 0) {
        TraceLog(LOG_WARNING, "");
        return envmap;
    }

    int prefilter = r3d_env_reserve_prefilter_layer();
    if (prefilter < 0) {
        r3d_env_release_irradiance_layer(irradiance);
        TraceLog(LOG_WARNING, "");
        return envmap;
    }

    gen_irradiance(irradiance, cubemap);
    gen_prefilter(prefilter, cubemap);

    envmap.irradiance = irradiance + 1;
    envmap.prefilter = prefilter + 1;

    return envmap;
}

void R3D_UpdateEnvironmentMap(R3D_EnvironmentMap envmap, R3D_Cubemap cubemap)
{
    if (envmap.irradiance == 0 || envmap.prefilter == 0) {
        TraceLog(LOG_WARNING, "");
        return;
    }

    int irradiance = (int)envmap.irradiance - 1;
    int prefilter = (int)envmap.prefilter - 1;

    gen_irradiance(irradiance, cubemap);
    gen_prefilter(prefilter, cubemap);
}
