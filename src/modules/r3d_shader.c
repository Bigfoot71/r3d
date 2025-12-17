/* r3d_mod_shader.c -- Internal R3D shader module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_shader.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <rlgl.h>

// ========================================
// SHADER CODE INCLUDES
// ========================================

#include <shaders/color.frag.h>
#include <shaders/screen.vert.h>
#include <shaders/cubemap.vert.h>
#include <shaders/ssao.frag.h>
#include <shaders/ssao_blur.frag.h>
#include <shaders/bloom_down.frag.h>
#include <shaders/bloom_up.frag.h>
#include <shaders/cubemap_from_equirectangular.frag.h>
#include <shaders/cubemap_irradiance.frag.h>
#include <shaders/cubemap_prefilter.frag.h>
#include <shaders/geometry.vert.h>
#include <shaders/geometry.frag.h>
#include <shaders/forward.vert.h>
#include <shaders/forward.frag.h>
#include <shaders/skybox.vert.h>
#include <shaders/skybox.frag.h>
#include <shaders/depth.vert.h>
#include <shaders/depth.frag.h>
#include <shaders/depth_cube.vert.h>
#include <shaders/depth_cube.frag.h>
#include <shaders/decal.vert.h>
#include <shaders/decal.frag.h>
#include <shaders/ambient.frag.h>
#include <shaders/lighting.frag.h>
#include <shaders/compose.frag.h>
#include <shaders/bloom.frag.h>
#include <shaders/ssr.frag.h>
#include <shaders/fog.frag.h>
#include <shaders/dof.frag.h>
#include <shaders/output.frag.h>
#include <shaders/fxaa.frag.h>

// ========================================
// MODULE STATE
// ========================================

struct r3d_mod_shader R3D_MOD_SHADER;

// ========================================
// INTERNAL MACROS
// ========================================

#define LOAD_SHADER(shader_name, vs_code, fs_code) do {                         \
    R3D_MOD_SHADER.shader_name.id = rlLoadShaderCode(vs_code, fs_code);         \
    if (R3D_MOD_SHADER.shader_name.id == 0) {                                   \
        TraceLog(LOG_ERROR, "R3D: Failed to load shader '" #shader_name "'");   \
        assert(false);                                                          \
        return;                                                                 \
    }                                                                           \
} while(0)

#define USE_SHADER(shader_name) do {                                            \
    glUseProgram(R3D_MOD_SHADER.shader_name.id);                                \
} while(0)                                                                      \

#define GET_LOCATION(shader_name, uniform) do {                                 \
    R3D_MOD_SHADER.shader_name.uniform.loc = glGetUniformLocation(              \
        R3D_MOD_SHADER.shader_name.id, #uniform                                 \
    );                                                                          \
} while(0)

#define GET_LOCATION_ARRAY(shader_name, uniform, index) do {                    \
    char _name[64];                                                             \
    snprintf(_name, sizeof(_name), "%s[%d]", #uniform, (int)(index));           \
    R3D_MOD_SHADER.shader_name.uniform[index].loc =                             \
        glGetUniformLocation(R3D_MOD_SHADER.shader_name.id, _name);             \
} while (0)

#define GET_LOCATION_ARRAY_STRUCT(shader_name, array, index, member) do {       \
    char _name[96];                                                             \
    snprintf(                                                                   \
        _name, sizeof(_name),                                                   \
        "%s[%d].%s",                                                            \
        #array, (int)(index), #member                                           \
    );                                                                          \
    R3D_MOD_SHADER.shader_name.array[index].member.loc =                        \
        glGetUniformLocation(                                                   \
            R3D_MOD_SHADER.shader_name.id,                                      \
            _name                                                               \
        );                                                                      \
} while (0)

#define SET_SAMPLER_1D(shader_name, uniform, value) do {                        \
    R3D_MOD_SHADER.shader_name.uniform.slot1D = (value);                        \
    glUniform1i(                                                                \
        R3D_MOD_SHADER.shader_name.uniform.loc,                                 \
        R3D_MOD_SHADER.shader_name.uniform.slot1D                               \
    );                                                                          \
} while(0)

#define SET_SAMPLER_2D(shader_name, uniform, value) do {                        \
    R3D_MOD_SHADER.shader_name.uniform.slot2D = (value);                        \
    glUniform1i(                                                                \
        R3D_MOD_SHADER.shader_name.uniform.loc,                                 \
        R3D_MOD_SHADER.shader_name.uniform.slot2D                               \
    );                                                                          \
} while(0)

#define SET_SAMPLER_CUBE(shader_name, uniform, value) do {                      \
    R3D_MOD_SHADER.shader_name.uniform.slotCube = (value);                      \
    glUniform1i(                                                                \
        R3D_MOD_SHADER.shader_name.uniform.loc,                                 \
        R3D_MOD_SHADER.shader_name.uniform.slotCube                             \
    );                                                                          \
} while(0)

#define UNLOAD_SHADER(shader_name) do {                                         \
    if (R3D_MOD_SHADER.shader_name.id != 0) {                                   \
        glDeleteProgram(R3D_MOD_SHADER.shader_name.id);                         \
    }                                                                           \
} while(0)

// ========================================
// HELPER FUNCTIONS DECLARATIONS
// ========================================

static char* inject_defines_to_shader_code(const char* code, const char* defines[], int count);

// ========================================
// SHADER LOADING FUNCTIONS
// ========================================

void r3d_mod_shader_load_prepare_ssao(void)
{
    LOAD_SHADER(prepare.ssao, SCREEN_VERT, SSAO_FRAG);

    GET_LOCATION(prepare.ssao, uTexDepth);
    GET_LOCATION(prepare.ssao, uTexNormal);
    GET_LOCATION(prepare.ssao, uTexKernel);
    GET_LOCATION(prepare.ssao, uTexNoise);
    GET_LOCATION(prepare.ssao, uMatInvProj);
    GET_LOCATION(prepare.ssao, uMatProj);
    GET_LOCATION(prepare.ssao, uMatView);
    GET_LOCATION(prepare.ssao, uRadius);
    GET_LOCATION(prepare.ssao, uBias);
    GET_LOCATION(prepare.ssao, uIntensity);

    USE_SHADER(prepare.ssao);

    SET_SAMPLER_2D(prepare.ssao, uTexDepth, 0);
    SET_SAMPLER_2D(prepare.ssao, uTexNormal, 1);
    SET_SAMPLER_1D(prepare.ssao, uTexKernel, 2);
    SET_SAMPLER_2D(prepare.ssao, uTexNoise, 3);
}
    
void r3d_mod_shader_load_prepare_ssao_blur(void)
{
    LOAD_SHADER(prepare.ssaoBlur, SCREEN_VERT, SSAO_BLUR_FRAG);

    GET_LOCATION(prepare.ssaoBlur, uTexOcclusion);
    GET_LOCATION(prepare.ssaoBlur, uTexNormal);
    GET_LOCATION(prepare.ssaoBlur, uTexDepth);
    GET_LOCATION(prepare.ssaoBlur, uMatInvProj);
    GET_LOCATION(prepare.ssaoBlur, uDirection);

    USE_SHADER(prepare.ssaoBlur);

    SET_SAMPLER_2D(prepare.ssaoBlur, uTexOcclusion, 0);
    SET_SAMPLER_2D(prepare.ssaoBlur, uTexNormal, 1);
    SET_SAMPLER_2D(prepare.ssaoBlur, uTexDepth, 2);
}

void r3d_mod_shader_load_prepare_bloom_down(void)
{
    LOAD_SHADER(prepare.bloomDown, SCREEN_VERT, BLOOM_DOWN_FRAG);

    GET_LOCATION(prepare.bloomDown, uTexture);
    GET_LOCATION(prepare.bloomDown, uTexelSize);
    GET_LOCATION(prepare.bloomDown, uMipLevel);
    GET_LOCATION(prepare.bloomDown, uPrefilter);

    USE_SHADER(prepare.bloomDown);

    SET_SAMPLER_2D(prepare.bloomDown, uTexture, 0);
}

void r3d_mod_shader_load_prepare_bloom_up(void)
{
    LOAD_SHADER(prepare.bloomUp, SCREEN_VERT, BLOOM_UP_FRAG);

    GET_LOCATION(prepare.bloomUp, uTexture);
    GET_LOCATION(prepare.bloomUp, uFilterRadius);

    USE_SHADER(prepare.bloomUp);

    SET_SAMPLER_2D(prepare.bloomUp, uTexture, 0);
}

void r3d_mod_shader_load_prepare_cubemap_from_equirectangular(void)
{
    LOAD_SHADER(prepare.cubemapFromEquirectangular, CUBEMAP_VERT, CUBEMAP_FROM_EQUIRECTANGULAR_FRAG);

    GET_LOCATION(prepare.cubemapFromEquirectangular, uMatProj);
    GET_LOCATION(prepare.cubemapFromEquirectangular, uMatView);
    GET_LOCATION(prepare.cubemapFromEquirectangular, uTexEquirectangular);

    USE_SHADER(prepare.cubemapFromEquirectangular);

    SET_SAMPLER_2D(prepare.cubemapFromEquirectangular, uTexEquirectangular, 0);
}

void r3d_mod_shader_load_prepare_cubemap_irradiance(void)
{
    LOAD_SHADER(prepare.cubemapIrradiance, CUBEMAP_VERT, CUBEMAP_IRRADIANCE_FRAG);

    GET_LOCATION(prepare.cubemapIrradiance, uMatProj);
    GET_LOCATION(prepare.cubemapIrradiance, uMatView);
    GET_LOCATION(prepare.cubemapIrradiance, uCubemap);

    USE_SHADER(prepare.cubemapIrradiance);

    SET_SAMPLER_CUBE(prepare.cubemapIrradiance, uCubemap, 0);
}

void r3d_mod_shader_load_prepare_cubemap_prefilter(void)
{
    LOAD_SHADER(prepare.cubemapPrefilter, CUBEMAP_VERT, CUBEMAP_PREFILTER_FRAG);

    GET_LOCATION(prepare.cubemapPrefilter, uMatProj);
    GET_LOCATION(prepare.cubemapPrefilter, uMatView);
    GET_LOCATION(prepare.cubemapPrefilter, uCubemap);
    GET_LOCATION(prepare.cubemapPrefilter, uResolution);
    GET_LOCATION(prepare.cubemapPrefilter, uRoughness);

    USE_SHADER(prepare.cubemapPrefilter);

    SET_SAMPLER_CUBE(prepare.cubemapPrefilter, uCubemap, 0);
}

void r3d_mod_shader_load_scene_geometry(void)
{
    LOAD_SHADER(scene.geometry, GEOMETRY_VERT, GEOMETRY_FRAG);

    GET_LOCATION(scene.geometry, uTexBoneMatrices);
    GET_LOCATION(scene.geometry, uMatInvView);
    GET_LOCATION(scene.geometry, uMatNormal);
    GET_LOCATION(scene.geometry, uMatModel);
    GET_LOCATION(scene.geometry, uMatVP);
    GET_LOCATION(scene.geometry, uAlbedoColor);
    GET_LOCATION(scene.geometry, uEmissionEnergy);
    GET_LOCATION(scene.geometry, uEmissionColor);
    GET_LOCATION(scene.geometry, uTexCoordOffset);
    GET_LOCATION(scene.geometry, uTexCoordScale);
    GET_LOCATION(scene.geometry, uInstancing);
    GET_LOCATION(scene.geometry, uSkinning);
    GET_LOCATION(scene.geometry, uBillboard);
    GET_LOCATION(scene.geometry, uTexAlbedo);
    GET_LOCATION(scene.geometry, uTexNormal);
    GET_LOCATION(scene.geometry, uTexEmission);
    GET_LOCATION(scene.geometry, uTexORM);
    GET_LOCATION(scene.geometry, uAlphaCutoff);
    GET_LOCATION(scene.geometry, uNormalScale);
    GET_LOCATION(scene.geometry, uOcclusion);
    GET_LOCATION(scene.geometry, uRoughness);
    GET_LOCATION(scene.geometry, uMetalness);

    USE_SHADER(scene.geometry);

    SET_SAMPLER_1D(scene.geometry, uTexBoneMatrices, 0);
    SET_SAMPLER_2D(scene.geometry, uTexAlbedo, 1);
    SET_SAMPLER_2D(scene.geometry, uTexNormal, 2);
    SET_SAMPLER_2D(scene.geometry, uTexEmission, 3);
    SET_SAMPLER_2D(scene.geometry, uTexORM, 4);
}

void r3d_mod_shader_load_scene_forward(void)
{
    LOAD_SHADER(scene.forward, FORWARD_VERT, FORWARD_FRAG);

    GET_LOCATION(scene.forward, uTexBoneMatrices);
    GET_LOCATION(scene.forward, uMatInvView);
    GET_LOCATION(scene.forward, uMatNormal);
    GET_LOCATION(scene.forward, uMatModel);
    GET_LOCATION(scene.forward, uMatVP);
    GET_LOCATION(scene.forward, uAlbedoColor);
    GET_LOCATION(scene.forward, uTexCoordOffset);
    GET_LOCATION(scene.forward, uTexCoordScale);
    GET_LOCATION(scene.forward, uInstancing);
    GET_LOCATION(scene.forward, uSkinning);
    GET_LOCATION(scene.forward, uBillboard);
    GET_LOCATION(scene.forward, uTexAlbedo);
    GET_LOCATION(scene.forward, uTexEmission);
    GET_LOCATION(scene.forward, uTexNormal);
    GET_LOCATION(scene.forward, uTexORM);
    GET_LOCATION(scene.forward, uEmissionEnergy);
    GET_LOCATION(scene.forward, uNormalScale);
    GET_LOCATION(scene.forward, uOcclusion);
    GET_LOCATION(scene.forward, uRoughness);
    GET_LOCATION(scene.forward, uMetalness);
    GET_LOCATION(scene.forward, uAmbientLight);
    GET_LOCATION(scene.forward, uEmissionColor);
    GET_LOCATION(scene.forward, uCubeIrradiance);
    GET_LOCATION(scene.forward, uCubePrefilter);
    GET_LOCATION(scene.forward, uTexBrdfLut);
    GET_LOCATION(scene.forward, uQuatSkybox);
    GET_LOCATION(scene.forward, uHasSkybox);
    GET_LOCATION(scene.forward, uSkyboxAmbientIntensity);
    GET_LOCATION(scene.forward, uSkyboxReflectIntensity);
    GET_LOCATION(scene.forward, uAlphaCutoff);
    GET_LOCATION(scene.forward, uViewPosition);
    GET_LOCATION(scene.forward, uFar);

    USE_SHADER(scene.forward);

    SET_SAMPLER_1D(scene.forward, uTexBoneMatrices, 0);
    SET_SAMPLER_2D(scene.forward, uTexAlbedo, 1);
    SET_SAMPLER_2D(scene.forward, uTexEmission, 2);
    SET_SAMPLER_2D(scene.forward, uTexNormal, 3);
    SET_SAMPLER_2D(scene.forward, uTexORM, 4);
    SET_SAMPLER_CUBE(scene.forward, uCubeIrradiance, 5);
    SET_SAMPLER_CUBE(scene.forward, uCubePrefilter, 6);
    SET_SAMPLER_2D(scene.forward, uTexBrdfLut, 7);

    int shadowMapSlot = 10;
    for (int i = 0; i < R3D_SHADER_FORWARD_NUM_LIGHTS; i++)
    {
        SET_SAMPLER_CUBE(scene.forward, uShadowMapCube[i], shadowMapSlot++);
        SET_SAMPLER_2D(scene.forward, uShadowMap2D[i], shadowMapSlot++);

        GET_LOCATION_ARRAY(scene.forward, uMatLightVP, i);
        GET_LOCATION_ARRAY(scene.forward, uShadowMapCube, i);
        GET_LOCATION_ARRAY(scene.forward, uShadowMap2D, i);

        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, color);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, position);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, direction);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, specular);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, energy);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, range);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, near);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, far);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, attenuation);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, innerCutOff);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, outerCutOff);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, shadowSoftness);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, shadowMapTxlSz);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, shadowDepthBias);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, shadowSlopeBias);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, type);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, enabled);
        GET_LOCATION_ARRAY_STRUCT(scene.forward, uLights, i, shadow);
    }
}

void r3d_mod_shader_load_scene_background(void)
{
    LOAD_SHADER(scene.background, SCREEN_VERT, COLOR_FRAG);
    GET_LOCATION(scene.background, uColor);
}

void r3d_mod_shader_load_scene_skybox(void)
{
    LOAD_SHADER(scene.skybox, SKYBOX_VERT, SKYBOX_FRAG);

    GET_LOCATION(scene.skybox, uMatProj);
    GET_LOCATION(scene.skybox, uMatView);
    GET_LOCATION(scene.skybox, uRotation);
    GET_LOCATION(scene.skybox, uSkyIntensity);
    GET_LOCATION(scene.skybox, uCubeSky);

    USE_SHADER(scene.skybox);

    SET_SAMPLER_CUBE(scene.skybox, uCubeSky, 0);
}

void r3d_mod_shader_load_scene_depth(void)
{
    LOAD_SHADER(scene.depth, DEPTH_VERT, DEPTH_FRAG);

    GET_LOCATION(scene.depth, uTexBoneMatrices);
    GET_LOCATION(scene.depth, uMatInvView);
    GET_LOCATION(scene.depth, uMatModel);
    GET_LOCATION(scene.depth, uMatVP);
    GET_LOCATION(scene.depth, uTexCoordOffset);
    GET_LOCATION(scene.depth, uTexCoordScale);
    GET_LOCATION(scene.depth, uAlpha);
    GET_LOCATION(scene.depth, uInstancing);
    GET_LOCATION(scene.depth, uSkinning);
    GET_LOCATION(scene.depth, uBillboard);
    GET_LOCATION(scene.depth, uTexAlbedo);
    GET_LOCATION(scene.depth, uAlphaCutoff);

    USE_SHADER(scene.depth);

    SET_SAMPLER_1D(scene.depth, uTexBoneMatrices, 0);
    SET_SAMPLER_2D(scene.depth, uTexAlbedo, 1);
}

void r3d_mod_shader_load_scene_depth_cube(void)
{
    LOAD_SHADER(scene.depthCube, DEPTH_CUBE_VERT, DEPTH_CUBE_FRAG);

    GET_LOCATION(scene.depthCube, uTexBoneMatrices);
    GET_LOCATION(scene.depthCube, uMatInvView);
    GET_LOCATION(scene.depthCube, uMatModel);
    GET_LOCATION(scene.depthCube, uMatVP);
    GET_LOCATION(scene.depthCube, uTexCoordOffset);
    GET_LOCATION(scene.depthCube, uTexCoordScale);
    GET_LOCATION(scene.depthCube, uAlpha);
    GET_LOCATION(scene.depthCube, uInstancing);
    GET_LOCATION(scene.depthCube, uSkinning);
    GET_LOCATION(scene.depthCube, uBillboard);
    GET_LOCATION(scene.depthCube, uTexAlbedo);
    GET_LOCATION(scene.depthCube, uAlphaCutoff);
    GET_LOCATION(scene.depthCube, uViewPosition);
    GET_LOCATION(scene.depthCube, uFar);

    USE_SHADER(scene.depthCube);

    SET_SAMPLER_1D(scene.depthCube, uTexBoneMatrices, 0);
    SET_SAMPLER_2D(scene.depthCube, uTexAlbedo, 1);
}

void r3d_mod_shader_load_scene_decal(void)
{
    LOAD_SHADER(scene.decal, DECAL_VERT, DECAL_FRAG);

    GET_LOCATION(scene.decal, uMatInvProj);
    GET_LOCATION(scene.decal, uMatProj);
    GET_LOCATION(scene.decal, uMatInvView);
    GET_LOCATION(scene.decal, uMatNormal);
    GET_LOCATION(scene.decal, uMatModel);
    GET_LOCATION(scene.decal, uMatVP);
    GET_LOCATION(scene.decal, uAlbedoColor);
    GET_LOCATION(scene.decal, uEmissionEnergy);
    GET_LOCATION(scene.decal, uEmissionColor);
    GET_LOCATION(scene.decal, uTexCoordOffset);
    GET_LOCATION(scene.decal, uTexCoordScale);
    GET_LOCATION(scene.decal, uInstancing);
    GET_LOCATION(scene.decal, uTexAlbedo);
    GET_LOCATION(scene.decal, uTexNormal);
    GET_LOCATION(scene.decal, uTexEmission);
    GET_LOCATION(scene.decal, uTexORM);
    GET_LOCATION(scene.decal, uTexDepth);
    GET_LOCATION(scene.decal, uAlphaCutoff);
    GET_LOCATION(scene.decal, uNormalScale);
    GET_LOCATION(scene.decal, uOcclusion);
    GET_LOCATION(scene.decal, uRoughness);
    GET_LOCATION(scene.decal, uMetalness);

    USE_SHADER(scene.decal);

    SET_SAMPLER_2D(scene.decal, uTexAlbedo, 0);
    SET_SAMPLER_2D(scene.decal, uTexNormal, 1);
    SET_SAMPLER_2D(scene.decal, uTexEmission, 2);
    SET_SAMPLER_2D(scene.decal, uTexORM, 3);
    SET_SAMPLER_2D(scene.decal, uTexDepth, 4);
}

void r3d_mod_shader_load_deferred_ambient_ibl(void)
{
    const char* defines[] = {"IBL"};
    char* fsCode = inject_defines_to_shader_code(AMBIENT_FRAG, defines, 1);

    LOAD_SHADER(deferred.ambientIbl, SCREEN_VERT, fsCode);

    RL_FREE(fsCode);

    GET_LOCATION(deferred.ambientIbl, uTexAlbedo);
    GET_LOCATION(deferred.ambientIbl, uTexNormal);
    GET_LOCATION(deferred.ambientIbl, uTexDepth);
    GET_LOCATION(deferred.ambientIbl, uTexSSAO);
    GET_LOCATION(deferred.ambientIbl, uTexORM);
    GET_LOCATION(deferred.ambientIbl, uCubeIrradiance);
    GET_LOCATION(deferred.ambientIbl, uCubePrefilter);
    GET_LOCATION(deferred.ambientIbl, uTexBrdfLut);
    GET_LOCATION(deferred.ambientIbl, uQuatSkybox);
    GET_LOCATION(deferred.ambientIbl, uSkyboxAmbientIntensity);
    GET_LOCATION(deferred.ambientIbl, uSkyboxReflectIntensity);
    GET_LOCATION(deferred.ambientIbl, uViewPosition);
    GET_LOCATION(deferred.ambientIbl, uMatInvProj);
    GET_LOCATION(deferred.ambientIbl, uMatInvView);
    GET_LOCATION(deferred.ambientIbl, uSSAOPower);

    USE_SHADER(deferred.ambientIbl);

    SET_SAMPLER_2D(deferred.ambientIbl, uTexAlbedo, 0);
    SET_SAMPLER_2D(deferred.ambientIbl, uTexNormal, 1);
    SET_SAMPLER_2D(deferred.ambientIbl, uTexDepth, 2);
    SET_SAMPLER_2D(deferred.ambientIbl, uTexSSAO, 3);
    SET_SAMPLER_2D(deferred.ambientIbl, uTexORM, 4);

    SET_SAMPLER_CUBE(deferred.ambientIbl, uCubeIrradiance, 5);
    SET_SAMPLER_CUBE(deferred.ambientIbl, uCubePrefilter, 6);
    SET_SAMPLER_2D(deferred.ambientIbl, uTexBrdfLut, 7);

}

void r3d_mod_shader_load_deferred_ambient(void)
{
    LOAD_SHADER(deferred.ambient, SCREEN_VERT, AMBIENT_FRAG);

    GET_LOCATION(deferred.ambient, uTexAlbedo);
    GET_LOCATION(deferred.ambient, uTexSSAO);
    GET_LOCATION(deferred.ambient, uTexORM);
    GET_LOCATION(deferred.ambient, uAmbientLight);
    GET_LOCATION(deferred.ambient, uSSAOPower);

    USE_SHADER(deferred.ambient);

    SET_SAMPLER_2D(deferred.ambient, uTexAlbedo, 0);
    SET_SAMPLER_2D(deferred.ambient, uTexSSAO, 1);
    SET_SAMPLER_2D(deferred.ambient, uTexORM, 2);
}

void r3d_mod_shader_load_deferred_lighting(void)
{
    LOAD_SHADER(deferred.lighting, SCREEN_VERT, LIGHTING_FRAG);

    GET_LOCATION(deferred.lighting, uTexAlbedo);
    GET_LOCATION(deferred.lighting, uTexNormal);
    GET_LOCATION(deferred.lighting, uTexDepth);
    GET_LOCATION(deferred.lighting, uTexORM);
    GET_LOCATION(deferred.lighting, uViewPosition);
    GET_LOCATION(deferred.lighting, uMatInvProj);
    GET_LOCATION(deferred.lighting, uMatInvView);

    GET_LOCATION(deferred.lighting, uLight.matVP);
    GET_LOCATION(deferred.lighting, uLight.shadowMap);
    GET_LOCATION(deferred.lighting, uLight.shadowCubemap);
    GET_LOCATION(deferred.lighting, uLight.color);
    GET_LOCATION(deferred.lighting, uLight.position);
    GET_LOCATION(deferred.lighting, uLight.direction);
    GET_LOCATION(deferred.lighting, uLight.specular);
    GET_LOCATION(deferred.lighting, uLight.energy);
    GET_LOCATION(deferred.lighting, uLight.range);
    GET_LOCATION(deferred.lighting, uLight.near);
    GET_LOCATION(deferred.lighting, uLight.far);
    GET_LOCATION(deferred.lighting, uLight.attenuation);
    GET_LOCATION(deferred.lighting, uLight.innerCutOff);
    GET_LOCATION(deferred.lighting, uLight.outerCutOff);
    GET_LOCATION(deferred.lighting, uLight.shadowSoftness);
    GET_LOCATION(deferred.lighting, uLight.shadowMapTxlSz);
    GET_LOCATION(deferred.lighting, uLight.shadowDepthBias);
    GET_LOCATION(deferred.lighting, uLight.shadowSlopeBias);
    GET_LOCATION(deferred.lighting, uLight.type);
    GET_LOCATION(deferred.lighting, uLight.shadow);

    USE_SHADER(deferred.lighting);

    SET_SAMPLER_2D(deferred.lighting, uTexAlbedo, 0);
    SET_SAMPLER_2D(deferred.lighting, uTexNormal, 1);
    SET_SAMPLER_2D(deferred.lighting, uTexDepth, 2);
    SET_SAMPLER_2D(deferred.lighting, uTexORM, 3);

    SET_SAMPLER_2D(deferred.lighting, uLight.shadowMap, 4);
    SET_SAMPLER_CUBE(deferred.lighting, uLight.shadowCubemap, 5);
}

void r3d_mod_shader_load_deferred_compose(void)
{
    LOAD_SHADER(deferred.compose, SCREEN_VERT, COMPOSE_FRAG);

    GET_LOCATION(deferred.compose, uTexAlbedo);
    GET_LOCATION(deferred.compose, uTexEmission);
    GET_LOCATION(deferred.compose, uTexDiffuse);
    GET_LOCATION(deferred.compose, uTexSpecular);
    GET_LOCATION(deferred.compose, uTexSSAO);
    GET_LOCATION(deferred.compose, uSSAOPower);
    GET_LOCATION(deferred.compose, uSSAOLightAffect);

    USE_SHADER(deferred.compose);

    SET_SAMPLER_2D(deferred.compose, uTexAlbedo, 0);
    SET_SAMPLER_2D(deferred.compose, uTexEmission, 1);
    SET_SAMPLER_2D(deferred.compose, uTexDiffuse, 2);
    SET_SAMPLER_2D(deferred.compose, uTexSpecular, 3);

    SET_SAMPLER_2D(deferred.compose, uTexSSAO, 4);

}

void r3d_mod_shader_load_post_bloom(void)
{
    LOAD_SHADER(post.bloom, SCREEN_VERT, BLOOM_FRAG);

    GET_LOCATION(post.bloom, uTexColor);
    GET_LOCATION(post.bloom, uTexBloomBlur);
    GET_LOCATION(post.bloom, uBloomMode);
    GET_LOCATION(post.bloom, uBloomIntensity);

    USE_SHADER(post.bloom);

    SET_SAMPLER_2D(post.bloom, uTexColor, 0);
    SET_SAMPLER_2D(post.bloom, uTexBloomBlur, 1);
}

void r3d_mod_shader_load_post_ssr(void)
{
    LOAD_SHADER(post.ssr, SCREEN_VERT, SSR_FRAG);

    GET_LOCATION(post.ssr, uTexColor);
    GET_LOCATION(post.ssr, uTexAlbedo);
    GET_LOCATION(post.ssr, uTexNormal);
    GET_LOCATION(post.ssr, uTexORM);
    GET_LOCATION(post.ssr, uTexDepth);
    GET_LOCATION(post.ssr, uMatView);
    GET_LOCATION(post.ssr, uMaxRaySteps);
    GET_LOCATION(post.ssr, uBinarySearchSteps);
    GET_LOCATION(post.ssr, uRayMarchLength);
    GET_LOCATION(post.ssr, uDepthThickness);
    GET_LOCATION(post.ssr, uDepthTolerance);
    GET_LOCATION(post.ssr, uEdgeFadeStart);
    GET_LOCATION(post.ssr, uEdgeFadeEnd);
    GET_LOCATION(post.ssr, uMatInvProj);
    GET_LOCATION(post.ssr, uMatInvView);
    GET_LOCATION(post.ssr, uMatViewProj);
    GET_LOCATION(post.ssr, uViewPosition);

    USE_SHADER(post.ssr);

    SET_SAMPLER_2D(post.ssr, uTexColor, 0);
    SET_SAMPLER_2D(post.ssr, uTexAlbedo, 1);
    SET_SAMPLER_2D(post.ssr, uTexNormal, 2);
    SET_SAMPLER_2D(post.ssr, uTexORM, 3);
    SET_SAMPLER_2D(post.ssr, uTexDepth, 4);
}

void r3d_mod_shader_load_post_fog(void)
{
    LOAD_SHADER(post.fog, SCREEN_VERT, FOG_FRAG);

    GET_LOCATION(post.fog, uTexColor);
    GET_LOCATION(post.fog, uTexDepth);
    GET_LOCATION(post.fog, uNear);
    GET_LOCATION(post.fog, uFar);
    GET_LOCATION(post.fog, uFogMode);
    GET_LOCATION(post.fog, uFogColor);
    GET_LOCATION(post.fog, uFogStart);
    GET_LOCATION(post.fog, uFogEnd);
    GET_LOCATION(post.fog, uFogDensity);
    GET_LOCATION(post.fog, uSkyAffect);

    USE_SHADER(post.fog);

    SET_SAMPLER_2D(post.fog, uTexColor, 0);
    SET_SAMPLER_2D(post.fog, uTexDepth, 1);
}

void r3d_mod_shader_load_post_dof(void)
{
    LOAD_SHADER(post.dof, SCREEN_VERT, DOF_FRAG);

    GET_LOCATION(post.dof, uTexColor);
    GET_LOCATION(post.dof, uTexDepth);
    GET_LOCATION(post.dof, uTexelSize);
    GET_LOCATION(post.dof, uNear);
    GET_LOCATION(post.dof, uFar);
    GET_LOCATION(post.dof, uFocusPoint);
    GET_LOCATION(post.dof, uFocusScale);
    GET_LOCATION(post.dof, uMaxBlurSize);
    GET_LOCATION(post.dof, uDebugMode);

    USE_SHADER(post.dof);

    SET_SAMPLER_2D(post.dof, uTexColor, 0);
    SET_SAMPLER_2D(post.dof, uTexDepth, 1);
}

void r3d_mod_shader_load_post_output(void)
{
    LOAD_SHADER(post.output, SCREEN_VERT, OUTPUT_FRAG);

    GET_LOCATION(post.output, uTexColor);
    GET_LOCATION(post.output, uTonemapExposure);
    GET_LOCATION(post.output, uTonemapWhite);
    GET_LOCATION(post.output, uTonemapMode);
    GET_LOCATION(post.output, uBrightness);
    GET_LOCATION(post.output, uContrast);
    GET_LOCATION(post.output, uSaturation);

    USE_SHADER(post.output);

    SET_SAMPLER_2D(post.output, uTexColor, 0);
}

void r3d_mod_shader_load_post_fxaa(void)
{
    LOAD_SHADER(post.fxaa, SCREEN_VERT, FXAA_FRAG);

    GET_LOCATION(post.fxaa, uTexture);
    GET_LOCATION(post.fxaa, uTexelSize);

    USE_SHADER(post.fxaa);

    SET_SAMPLER_2D(post.fxaa, uTexture, 0);
}

// ========================================
// MODULE FUNCTIONS
// ========================================

bool r3d_mod_shader_init()
{
    memset(&R3D_MOD_SHADER, 0, sizeof(R3D_MOD_SHADER));
    return true;
}

void r3d_mod_shader_quit()
{
    UNLOAD_SHADER(prepare.ssao);
    UNLOAD_SHADER(prepare.ssaoBlur);
    UNLOAD_SHADER(prepare.bloomDown);
    UNLOAD_SHADER(prepare.bloomUp);
    UNLOAD_SHADER(prepare.cubemapFromEquirectangular);
    UNLOAD_SHADER(prepare.cubemapIrradiance);
    UNLOAD_SHADER(prepare.cubemapPrefilter);

    UNLOAD_SHADER(scene.geometry);
    UNLOAD_SHADER(scene.forward);
    UNLOAD_SHADER(scene.background);
    UNLOAD_SHADER(scene.skybox);
    UNLOAD_SHADER(scene.depth);
    UNLOAD_SHADER(scene.depthCube);
    UNLOAD_SHADER(scene.decal);

    UNLOAD_SHADER(deferred.ambientIbl);
    UNLOAD_SHADER(deferred.ambient);
    UNLOAD_SHADER(deferred.lighting);
    UNLOAD_SHADER(deferred.compose);

    UNLOAD_SHADER(post.bloom);
    UNLOAD_SHADER(post.ssr);
    UNLOAD_SHADER(post.fog);
    UNLOAD_SHADER(post.dof);
    UNLOAD_SHADER(post.output);
    UNLOAD_SHADER(post.fxaa);
}

// ========================================
// HELPER FUNCTIONS DEFINITIONS
// ========================================

static char* inject_defines_to_shader_code(const char* code, const char* defines[], int count)
{
    if (!code || count < 0) return NULL;

    // Find the end of the #version line
    const char* versionStart = strstr(code, "#version");
    assert(versionStart && "Shader must have version");

    const char* versionEnd = strchr(versionStart, '\n');
    if (!versionEnd) versionEnd = versionStart + strlen(versionStart);
    else versionEnd++; // Include the \n

    // Calculate sizes
    static const char DEFINE_PREFIX[] = "#define ";
    static const size_t DEFINE_PREFIX_LEN = sizeof(DEFINE_PREFIX) - 1; // -1 to exclude '\0'
    
    size_t prefixLen = versionEnd - code;
    size_t definesLen = 0;
    for (int i = 0; i < count; i++) {
        if (defines[i]) {
            definesLen += DEFINE_PREFIX_LEN + strlen(defines[i]) + 1; // +1 for \n
        }
    }
    size_t suffixLen = strlen(versionEnd);
    
    // Allocate and build the new shader
    char* newShader = (char*)RL_MALLOC(prefixLen + definesLen + suffixLen + 1);
    if (!newShader) return NULL;

    char* dest = newShader;
    
    // Copy the part before defines (up to after #version)
    memcpy(dest, code, prefixLen);
    dest += prefixLen;

    // Add the defines
    for (int i = 0; i < count; i++) {
        if (defines[i]) {
            memcpy(dest, DEFINE_PREFIX, DEFINE_PREFIX_LEN);
            dest += DEFINE_PREFIX_LEN;
            
            size_t defineLen = strlen(defines[i]);
            memcpy(dest, defines[i], defineLen);
            dest += defineLen;
            
            *dest++ = '\n';
        }
    }

    // Copy the rest of the shader
    memcpy(dest, versionEnd, suffixLen);
    dest[suffixLen] = '\0';

    return newShader;
}
