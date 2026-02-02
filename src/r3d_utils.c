/* r3d_utils.h -- R3D Utility Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_utils.h>

#include "./modules/r3d_texture.h"
#include "./modules/r3d_target.h"
#include "./r3d_core_state.h"

// ========================================
// PUBLIC API
// ========================================

Texture2D R3D_GetWhiteTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = r3d_texture_get(R3D_TEXTURE_WHITE);
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return texture;
}

Texture2D R3D_GetBlackTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = r3d_texture_get(R3D_TEXTURE_BLACK);
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    return texture;
}

Texture2D R3D_GetNormalTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = r3d_texture_get(R3D_TEXTURE_NORMAL);
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    return texture;
}

Texture2D R3D_GetBufferNormal(void)
{
    Texture2D texture = { 0 };
    texture.id = r3d_target_get(R3D_TARGET_NORMAL);
    texture.width = R3D_TARGET_WIDTH;
    texture.height = R3D_TARGET_HEIGHT;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32;
    return texture;
}

Texture2D R3D_GetBufferDepth(void)
{
    Texture2D texture = { 0 };
    texture.id = r3d_target_get(R3D_TARGET_DEPTH);
    texture.width = R3D_TARGET_WIDTH;
    texture.height = R3D_TARGET_HEIGHT;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R16;
    return texture;
}

Matrix R3D_GetMatrixView(void)
{
    return R3D.viewState.view;
}

Matrix R3D_GetMatrixInvView(void)
{
    return R3D.viewState.invView;
}

Matrix R3D_GetMatrixProjection(void)
{
    return R3D.viewState.proj;
}

Matrix R3D_GetMatrixInvProjection(void)
{
    return R3D.viewState.invProj;
}
