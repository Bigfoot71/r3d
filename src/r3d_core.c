/* r3d_core.h -- R3D Core Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_core.h>

#include <raymath.h>
#include <rlgl.h>
#include <glad.h>

#include <assimp/cimport.h>
#include <float.h>

#include "./modules/r3d_primitive.h"
#include "./modules/r3d_texture.h"
#include "./modules/r3d_storage.h"
#include "./modules/r3d_target.h"
#include "./modules/r3d_shader.h"
#include "./modules/r3d_light.h"
#include "./modules/r3d_cache.h"
#include "./modules/r3d_draw.h"

// ========================================
// PUBLIC API
// ========================================

void R3D_Init(int resWidth, int resHeight, R3D_Flags flags)
{
    r3d_mod_primitive_init();
    r3d_mod_texture_init();
    r3d_mod_storage_init();
    r3d_mod_target_init(resWidth, resHeight);
    r3d_mod_shader_init();
    r3d_mod_light_init();
    r3d_mod_cache_init(flags);
    r3d_mod_draw_init();

    // Defines suitable clipping plane distances for r3d
    rlSetClipPlanes(0.05f, 4000.0f);
}

void R3D_Close(void)
{
    r3d_mod_primitive_quit();
    r3d_mod_texture_quit();
    r3d_mod_storage_quit();
    r3d_mod_target_quit();
    r3d_mod_shader_quit();
    r3d_mod_light_quit();
    r3d_mod_cache_quit();
    r3d_mod_draw_quit();
}

bool R3D_HasState(R3D_Flags flags)
{
    return R3D_CACHE_FLAGS_HAS(state, flags);
}

void R3D_SetState(R3D_Flags flags)
{
    R3D_CACHE_FLAGS_ASSIGN(state, flags);
}

void R3D_ClearState(R3D_Flags flags)
{
    R3D_CACHE_FLAGS_CLEAR(state, flags);
}

void R3D_GetResolution(int* width, int* height)
{
    r3d_mod_target_get_resolution(width, height, 0);
}

void R3D_UpdateResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        TraceLog(LOG_ERROR, "R3D: Invalid resolution given to 'R3D_UpdateResolution'");
        return;
    }

    if (width == R3D_TARGET_WIDTH && height == R3D_TARGET_HEIGHT) {
        return;
    }

    // TODO: Add a texture resizing function that avoids
    //       reallocation if the new resolution is lower
    r3d_mod_target_quit();
    r3d_mod_target_init(width, height);
}

void R3D_SetTextureFilter(TextureFilter filter)
{
    R3D_CACHE_SET(textureFilter, filter);
}

R3D_Layer R3D_GetActiveLayers(void)
{
    return R3D_CACHE_GET(layers);
}

void R3D_SetActiveLayers(R3D_Layer bitfield)
{
    R3D_CACHE_SET(layers, bitfield);
}

void R3D_EnableLayers(R3D_Layer bitfield)
{
    R3D_CACHE_FLAGS_ASSIGN(layers, bitfield);
}

void R3D_DisableLayers(R3D_Layer bitfield)
{
    R3D_CACHE_FLAGS_CLEAR(layers, bitfield);
}
