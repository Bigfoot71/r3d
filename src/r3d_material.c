/* r3d_material.c -- R3D Material Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_material.h>
#include <r3d/r3d_utils.h>

#include "./modules/r3d_texture.h"
#include "./common/r3d_image.h"
#include "./r3d_core_state.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Material R3D_GetDefaultMaterial(void)
{
    return R3D.material;
}

void R3D_SetDefaultMaterial(R3D_Material material)
{
    R3D.material = material;
}

void R3D_UnloadMaterial(R3D_Material material)
{
    R3D_UnloadAlbedoMap(material.albedo);
    R3D_UnloadEmissionMap(material.emission);
    R3D_UnloadNormalMap(material.normal);
    R3D_UnloadOrmMap(material.orm);
}

R3D_AlbedoMap R3D_LoadAlbedoMap(const char* fileName, Color color)
{
    R3D_AlbedoMap map = {0};

    Image image = LoadImage(fileName);
    if (!IsImageValid(image)) {
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_REPEAT, R3D.textureFilter, srgb);
    map.color = color;

    UnloadImage(image);

    return map;
}

R3D_AlbedoMap R3D_LoadAlbedoMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color)
{
    R3D_AlbedoMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;

    UnloadImage(image);

    return map;
}

void R3D_UnloadAlbedoMap(R3D_AlbedoMap map)
{
    if (map.texture.id == 0 || r3d_texture_is_default(map.texture.id)) {
        return;
    }

    UnloadTexture(map.texture);
}

R3D_EmissionMap R3D_LoadEmissionMap(const char* fileName, Color color, float energy)
{
    R3D_EmissionMap map = {0};

    Image image = LoadImage(fileName);
    if (!IsImageValid(image)) {
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;
    map.energy = energy;

    UnloadImage(image);

    return map;
}

R3D_EmissionMap R3D_LoadEmissionMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color, float energy)
{
    R3D_EmissionMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;
    map.energy = energy;

    UnloadImage(image);

    return map;
}

void R3D_UnloadEmissionMap(R3D_EmissionMap map)
{
    if (map.texture.id == 0 || r3d_texture_is_default(map.texture.id)) {
        return;
    }

    UnloadTexture(map.texture);
}

R3D_NormalMap R3D_LoadNormalMap(const char* fileName, float scale)
{
    R3D_NormalMap map = {0};

    Image image = LoadImage(fileName);
    if (!IsImageValid(image)) {
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.scale = scale;

    UnloadImage(image);

    return map;
}

R3D_NormalMap R3D_LoadNormalMapFromMemory(const char* fileType, const void* fileData, int dataSize, float scale)
{
    R3D_NormalMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.scale = scale;

    UnloadImage(image);

    return map;
}

void R3D_UnloadNormalMap(R3D_NormalMap map)
{
    if (map.texture.id == 0 || r3d_texture_is_default(map.texture.id)) {
        return;
    }

    UnloadTexture(map.texture);
}

R3D_OrmMap R3D_LoadOrmMap(const char* fileName, float occlusion, float roughness, float metalness)
{
    R3D_OrmMap map = {0};

    Image image = LoadImage(fileName);
    if (!IsImageValid(image)) {
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.occlusion = occlusion;
    map.roughness = roughness;
    map.metalness = metalness;

    UnloadImage(image);

    return map;
}

R3D_OrmMap R3D_LoadOrmMapFromMemory(const char* fileType, const void* fileData, int dataSize,
                                    float occlusion, float roughness, float metalness)
{
    R3D_OrmMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.occlusion = occlusion;
    map.roughness = roughness;
    map.metalness = metalness;

    UnloadImage(image);

    return map;
}

void R3D_UnloadOrmMap(R3D_OrmMap map)
{
    if (map.texture.id == 0 || r3d_texture_is_default(map.texture.id)) {
        return;
    }

    UnloadTexture(map.texture);
}
