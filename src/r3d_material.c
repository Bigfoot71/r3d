/* r3d_material.c -- R3D Material Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_material.h>
#include <r3d/r3d_utils.h>
#include <r3d_config.h>

#ifdef R3D_SUPPORT_ASSIMP
#   include "./importer/r3d_importer_internal.h"
#endif

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

R3D_Material* R3D_LoadMaterials(const char* filePath, int* materialCount)
{
    R3D_Material* materials = NULL;

#ifdef R3D_SUPPORT_ASSIMP
    R3D_Importer* importer = R3D_LoadImporter(filePath, 0);
    if (importer == NULL) return materials;

    materials = R3D_LoadMaterialsFromImporter(importer, materialCount);
    R3D_UnloadImporter(importer);

#else
    R3D_TRACELOG(LOG_WARNING, "Cannot load '%s': built without Assimp support", filePath);

#endif // R3D_SUPPORT_ASSIMP

    return materials;
}

R3D_Material* R3D_LoadMaterialsFromMemory(const void* data, unsigned int size, const char* hint, int* materialCount)
{
    R3D_Material* materials = NULL;

#ifdef R3D_SUPPORT_ASSIMP
    R3D_Importer* importer = R3D_LoadImporterFromMemory(data, size, hint, 0);
    if (importer == NULL) return materials;

    materials = R3D_LoadMaterialsFromImporter(importer, materialCount);
    R3D_UnloadImporter(importer);

#else
    if (hint && hint[0] != '\0') {
        R3D_TRACELOG(LOG_WARNING, "Cannot load '%s' from memory: built without Assimp support", hint);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Cannot load asset from memory: built without Assimp support");
    }

#endif // R3D_SUPPORT_ASSIMP

    return materials;
}

R3D_Material* R3D_LoadMaterialsFromImporter(const R3D_Importer* importer, int* materialCount)
{
    R3D_Material* materials = NULL;

#ifdef R3D_SUPPORT_ASSIMP
    if (importer == NULL) {
        R3D_TRACELOG(LOG_WARNING, "Cannot load materials from importer: NULL importer");
        return materials;
    }

    r3d_importer_texture_cache_t* textureCache = r3d_importer_load_texture_cache(
        importer, R3D.colorSpace, R3D.textureFilter);
    if (textureCache == NULL) return NULL;

    if (!r3d_importer_load_materials(importer, &materials, materialCount, textureCache)) {
        r3d_importer_unload_texture_cache(textureCache, true);
        return NULL;
    }

    r3d_importer_unload_texture_cache(textureCache, false);

#else
    R3D_TRACELOG(LOG_WARNING, "Cannot load materials from importer: built without Assimp support");

#endif // R3D_SUPPORT_ASSIMP

    return materials;
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
        R3D_TRACELOG(LOG_WARNING, "Failed to load albedo image: '%s'", fileName);
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Albedo map loaded successfully: '%s'", fileName);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload albedo texture: '%s'", fileName);
    }

    UnloadImage(image);

    return map;
}

R3D_AlbedoMap R3D_LoadAlbedoMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color)
{
    R3D_AlbedoMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load albedo image from memory (type: '%s')", fileType);
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Albedo map loaded successfully from memory (type: '%s')", fileType);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload albedo texture from memory (type: '%s')", fileType);
    }

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
        R3D_TRACELOG(LOG_WARNING, "Failed to load emission image: '%s'", fileName);
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;
    map.energy = energy;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Emission map loaded successfully: '%s'", fileName);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload emission texture: '%s'", fileName);
    }

    UnloadImage(image);

    return map;
}

R3D_EmissionMap R3D_LoadEmissionMapFromMemory(const char* fileType, const void* fileData, int dataSize, Color color, float energy)
{
    R3D_EmissionMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load emission image from memory (type: '%s')", fileType);
        return map;
    }

    bool srgb = (R3D.colorSpace == R3D_COLORSPACE_SRGB);

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, srgb);
    map.color = color;
    map.energy = energy;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Emission map loaded successfully from memory (type: '%s')", fileType);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload emission texture from memory (type: '%s')", fileType);
    }

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
        R3D_TRACELOG(LOG_WARNING, "Failed to load normal map image: '%s'", fileName);
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.scale = scale;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Normal map loaded successfully: '%s'", fileName);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload normal map texture: '%s'", fileName);
    }

    UnloadImage(image);

    return map;
}

R3D_NormalMap R3D_LoadNormalMapFromMemory(const char* fileType, const void* fileData, int dataSize, float scale)
{
    R3D_NormalMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load normal map image from memory (type: '%s')", fileType);
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.scale = scale;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "Normal map loaded successfully from memory (type: '%s')", fileType);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload normal map texture from memory (type: '%s')", fileType);
    }

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
        R3D_TRACELOG(LOG_WARNING, "Failed to load ORM image: '%s'", fileName);
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.occlusion = occlusion;
    map.roughness = roughness;
    map.metalness = metalness;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "ORM map loaded successfully: '%s'", fileName);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload ORM texture: '%s'", fileName);
    }

    UnloadImage(image);

    return map;
}

R3D_OrmMap R3D_LoadOrmMapFromMemory(const char* fileType, const void* fileData, int dataSize,
                                    float occlusion, float roughness, float metalness)
{
    R3D_OrmMap map = {0};

    Image image = LoadImageFromMemory(fileType, fileData, dataSize);
    if (!IsImageValid(image)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to load ORM image from memory (type: '%s')", fileType);
        return map;
    }

    map.texture = r3d_image_upload(&image, TEXTURE_WRAP_CLAMP, R3D.textureFilter, false);
    map.occlusion = occlusion;
    map.roughness = roughness;
    map.metalness = metalness;

    if (map.texture.id != 0) {
        R3D_TRACELOG(LOG_INFO, "ORM map loaded successfully from memory (type: '%s')", fileType);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Failed to upload ORM texture from memory (type: '%s')", fileType);
    }

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
