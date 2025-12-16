/* r3d_utils.h -- R3D Utility Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_utils.h>
#include "./r3d_state.h"

// ========================================
// PUBLIC API
// ========================================

Texture2D R3D_GetWhiteTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.texture.white;
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    return texture;
}

Texture2D R3D_GetBlackTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.texture.black;
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    return texture;
}

Texture2D R3D_GetNormalTexture(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.texture.normal;
    texture.width = 1;
    texture.height = 1;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32G32B32;
    return texture;
}

Texture2D R3D_GetBufferColor(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.target.scenePp[1];
    texture.width = R3D.state.resolution.width;
    texture.height = R3D.state.resolution.height;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    return texture;
}

Texture2D R3D_GetBufferNormal(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.target.normal;
    texture.width = R3D.state.resolution.width;
    texture.height = R3D.state.resolution.height;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32;
    return texture;
}

Texture2D R3D_GetBufferDepth(void)
{
    Texture2D texture = { 0 };
    texture.id = R3D.target.depthStencil;
    texture.width = R3D.state.resolution.width;
    texture.height = R3D.state.resolution.height;
    texture.mipmaps = 1;
    texture.format = PIXELFORMAT_UNCOMPRESSED_R32;
    return texture;
}

Matrix R3D_GetMatrixView(void)
{
    return R3D.state.transform.view;
}

Matrix R3D_GetMatrixInvView(void)
{
    return R3D.state.transform.invView;
}

Matrix R3D_GetMatrixProjection(void)
{
    return R3D.state.transform.proj;
}

Matrix R3D_GetMatrixInvProjection(void)
{
    return R3D.state.transform.invProj;
}

void R3D_DrawBufferAlbedo(float x, float y, float w, float h)
{
    Texture2D tex = {
        .id = R3D.target.albedo,
        .width = R3D.state.resolution.width,
        .height = R3D.state.resolution.width
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}

void R3D_DrawBufferEmission(float x, float y, float w, float h)
{
    Texture2D tex = {
        .id = R3D.target.emission,
        .width = R3D.state.resolution.width,
        .height = R3D.state.resolution.height
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}

void R3D_DrawBufferNormal(float x, float y, float w, float h)
{
    Texture2D tex = {
        .id = R3D.target.normal,
        .width = R3D.state.resolution.width,
        .height = R3D.state.resolution.height
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}

void R3D_DrawBufferORM(float x, float y, float w, float h)
{
    Texture2D tex = {
        .id = R3D.target.orm,
        .width = R3D.state.resolution.width,
        .height = R3D.state.resolution.height
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}

void R3D_DrawBufferSSAO(float x, float y, float w, float h)
{
    Texture2D tex = {
        .id = R3D.target.ssaoPpHs[1],
        .width = R3D.state.resolution.width / 2,
        .height = R3D.state.resolution.height / 2
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}

void R3D_DrawBufferBloom(float x, float y, float w, float h)
{
    if (R3D.target.mipChainHs.chain == NULL) {
        return;
    }

    Texture2D tex = {
        .id = R3D.target.mipChainHs.chain[0].id,
        .width = R3D.state.resolution.width / 2,
        .height = R3D.state.resolution.height / 2
    };

    DrawTexturePro(
        tex, (Rectangle) { 0, 0, (float)tex.width, (float)-tex.height },
        (Rectangle) { x, y, w, h }, (Vector2) { 0 }, 0, WHITE
    );

    DrawRectangleLines(
        (int)(x + 0.5f), (int)(y + 0.5f),
        (int)(w + 0.5f), (int)(h + 0.5f),
        (Color) { 255, 0, 0, 255 }
    );
}
