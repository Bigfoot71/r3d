/* r3d_texture.h -- R3D Texture Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_texture.h>

#include "./modules/r3d_texture.h"
#include "./common/r3d_image.h"
#include "./r3d_core_state.h"

// ========================================
// PUBLIC API
// ========================================

Texture2D R3D_LoadTexture(const char* fileName, bool isColor)
{
    return R3D_LoadTextureEx(fileName, R3D.textureWrap, R3D.textureFilter, isColor);
}

Texture2D R3D_LoadTextureEx(const char* fileName, TextureWrap wrap, TextureFilter filter, bool isColor)
{
    Texture2D texture = {0};

    Image image = LoadImage(fileName);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture (path: '%s')", fileName);
        return texture;
    }

    bool srgb = isColor ? (R3D.colorSpace == R3D_COLORSPACE_SRGB) : false;
    texture = r3d_image_upload(&image, wrap, filter, srgb);

    if (texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Texture loaded successfully (path: '%s')", fileName);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture (path: '%s')", fileName);
    }

    UnloadImage(image);

    return texture;
}

Texture2D R3D_LoadTextureFromImage(Image image, bool isColor)
{
    return R3D_LoadTextureFromImageEx(image, R3D.textureWrap, R3D.textureFilter, isColor);
}

Texture2D R3D_LoadTextureFromImageEx(Image image, TextureWrap wrap, TextureFilter filter, bool isColor)
{
    bool srgb = isColor ? (R3D.colorSpace == R3D_COLORSPACE_SRGB) : false;
    Texture2D texture = r3d_image_upload(&image, wrap, filter, srgb);

    if (texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Texture loaded successfully from image");
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture from image");
    }

    UnloadImage(image);

    return texture;
}

Texture2D R3D_LoadTextureFromMemory(const char* fileType, const void* fileData, int dataSize, bool isColor)
{
    return R3D_LoadTextureFromMemoryEx(fileType, fileData, dataSize, R3D.textureWrap, R3D.textureFilter, isColor);
}

Texture2D R3D_LoadTextureFromMemoryEx(const char* fileType, const void* fileData, int dataSize, TextureWrap wrap, TextureFilter filter, bool isColor)
{
    Texture2D texture = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture from memory (type: '%s')", fileType);
        return texture;
    }

    bool srgb = isColor ? (R3D.colorSpace == R3D_COLORSPACE_SRGB) : false;
    texture = r3d_image_upload(&image, wrap, filter, srgb);

    if (texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Texture loaded successfully from memory (type: '%s')", fileType);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to load texture from memory (type: '%s')", fileType);
    }

    UnloadImage(image);

    return texture;
}

void R3D_UnloadTexture(Texture2D texture)
{
    if (texture.id == 0 || r3d_texture_is_default(texture.id)) {
        return;
    }

    UnloadTexture(texture);
}
