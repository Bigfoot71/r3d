/* r3d_ambient_map.c -- R3D Ambient Map Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_ambient_map.h>
#include <r3d/r3d_cubemap.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stddef.h>
#include <rlgl.h>
#include <glad.h>

#include "./common/r3d_pass.h"
#include "./modules/r3d_env.h"

// ========================================
// PUBLIC API
// ========================================

R3D_AmbientMap R3D_LoadAmbientMap(const char* fileName, R3D_CubemapLayout layout, R3D_AmbientFlags flags)
{
    R3D_Cubemap cubemap = R3D_LoadCubemap(fileName, layout);
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, flags);
    R3D_UnloadCubemap(cubemap);
    return ambientMap;
}

R3D_AmbientMap R3D_LoadAmbientMapFromImage(Image image, R3D_CubemapLayout layout, R3D_AmbientFlags flags)
{
    R3D_Cubemap cubemap = R3D_LoadCubemapFromImage(image, layout);
    R3D_AmbientMap ambientMap = R3D_GenAmbientMap(cubemap, flags);
    R3D_UnloadCubemap(cubemap);
    return ambientMap;
}

R3D_AmbientMap R3D_GenAmbientMap(R3D_Cubemap cubemap, R3D_AmbientFlags flags)
{
    R3D_AmbientMap ambientMap = {0};

    int irradiance = -1;
    if (flags & R3D_AMBIENT_ILLUMINATION) {
        irradiance = r3d_env_irradiance_reserve_layer();
        if (irradiance < 0) {
            R3D_TRACELOG(LOG_WARNING, "Failed to reserve irradiance cubemap for ambient map");
            return ambientMap;
        }
        r3d_pass_prepare_irradiance(irradiance, cubemap.texture, cubemap.size);
    }

    int prefilter = -1;
    if (flags & R3D_AMBIENT_REFLECTION) {
        prefilter = r3d_env_prefilter_reserve_layer();
        if (prefilter < 0) {
            r3d_env_irradiance_release_layer(irradiance);
            R3D_TRACELOG(LOG_WARNING, "Failed to reserve irradiance cubemap for ambient map");
            return ambientMap;
        }
        r3d_pass_prepare_prefilter(prefilter, cubemap.texture, cubemap.size);
    }

    ambientMap.irradiance = irradiance + 1;
    ambientMap.prefilter = prefilter + 1;
    ambientMap.flags = flags;

    return ambientMap;
}

void R3D_UnloadAmbientMap(R3D_AmbientMap ambientMap)
{
    if (ambientMap.irradiance > 0) {
        r3d_env_irradiance_release_layer((int)ambientMap.irradiance - 1);
    }

    if (ambientMap.prefilter > 0) {
        r3d_env_prefilter_release_layer((int)ambientMap.prefilter - 1);
    }
}

void R3D_UpdateAmbientMap(R3D_AmbientMap ambientMap, R3D_Cubemap cubemap)
{
    if ((ambientMap.flags & R3D_AMBIENT_ILLUMINATION) && ambientMap.irradiance > 0) {
        r3d_pass_prepare_irradiance((int)ambientMap.irradiance - 1, cubemap.texture, cubemap.size);
    }

    if ((ambientMap.flags & R3D_AMBIENT_REFLECTION) && ambientMap.prefilter > 0) {
        r3d_pass_prepare_prefilter((int)ambientMap.prefilter - 1, cubemap.texture, cubemap.size);
    }
}
