/* r3d_importer_material.c -- Module to import materials from assimp scene.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <raylib.h>
#include <string.h>

// ========================================
// MATERIAL LOADING (INTERNAL)
// ========================================

static void load_material(R3D_Material* material, const r3d_importer_t* importer, r3d_importer_texture_cache_t* textureCache, int index)
{
    const struct aiMaterial* aiMat = r3d_importer_get_material(importer, index);

    // Initialize with defaults
    *material = R3D_GetDefaultMaterial();

    // Load albedo map
    Texture2D* albedoTex = r3d_importer_get_loaded_texture(textureCache, index, R3D_MAP_ALBEDO);
    if (albedoTex) {
        material->albedo.texture = *albedoTex;
    }

    struct aiColor4D color;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS) {
        material->albedo.color = r3d_importer_cast(color);
    }
    else if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
        material->albedo.color = r3d_importer_cast(color);
    }

    // Load opacity factor
    if (material->albedo.color.a >= 255) {
        float opacity;
        if (aiGetMaterialFloat(aiMat, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS) {
            material->albedo.color.a = (unsigned char)(opacity * 255.0f);
        }
        else if (aiGetMaterialFloat(aiMat, AI_MATKEY_TRANSPARENCYFACTOR, &opacity) == AI_SUCCESS) {
            material->albedo.color.a = (unsigned char)((1.0f - opacity) * 255.0f);
        }
        else if (aiGetMaterialFloat(aiMat, AI_MATKEY_TRANSMISSION_FACTOR, &opacity) == AI_SUCCESS) {
            material->albedo.color.a = (unsigned char)((1.0f - opacity) * 255.0f);
        }
    }

    // Load emission map
    Texture2D* emissionTex = r3d_importer_get_loaded_texture(textureCache, index, R3D_MAP_EMISSION);
    if (emissionTex) {
        material->emission.texture = *emissionTex;
        material->emission.energy = 1.0f;
    }

    struct aiColor4D emissionColor;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_EMISSIVE, &emissionColor) == AI_SUCCESS) {
        material->emission.color = r3d_importer_cast(emissionColor);
        material->emission.energy = 1.0f;
    }

    // Load ORM map
    Texture2D* ormTex = r3d_importer_get_loaded_texture(textureCache, index, R3D_MAP_ORM);
    if (ormTex) {
        material->orm.texture = *ormTex;
    }

    float roughness;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS) {
        material->orm.roughness = roughness;
    }

    float metalness;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_METALLIC_FACTOR, &metalness) == AI_SUCCESS) {
        material->orm.metalness = metalness;
    }

    // Load normal map
    Texture2D* normalTex = r3d_importer_get_loaded_texture(textureCache, index, R3D_MAP_NORMAL);
    if (normalTex) {
        material->normal.texture = *normalTex;

        float normalScale;
        if (aiGetMaterialFloat(aiMat, AI_MATKEY_BUMPSCALING, &normalScale) == AI_SUCCESS) {
            material->normal.scale = normalScale;
        }
    }

    // Handle glTF alpha cutoff
    float alphaCutoff;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_GLTF_ALPHACUTOFF, &alphaCutoff) == AI_SUCCESS) {
        material->alphaCutoff = alphaCutoff;
    }

    // Handle glTF alpha mode
    struct aiString alphaMode;
    if (aiGetMaterialString(aiMat, AI_MATKEY_GLTF_ALPHAMODE, &alphaMode) == AI_SUCCESS) {
        if (strcmp(alphaMode.data, "MASK") == 0) {
            material->blendMode = R3D_BLEND_OPAQUE; // Performed during pre-pass
        }
        else if (strcmp(alphaMode.data, "BLEND") == 0) {
            material->blendMode = R3D_BLEND_ALPHA;
        }
    }

    // Handle blend function override
    int blendFunc;
    if (aiGetMaterialInteger(aiMat, AI_MATKEY_BLEND_FUNC, &blendFunc) == AI_SUCCESS) {
        switch (blendFunc) {
        case aiBlendMode_Default:
            material->blendMode = R3D_BLEND_ALPHA;
            break;
        case aiBlendMode_Additive:
            material->blendMode = R3D_BLEND_ADDITIVE;
            break;
        default:
            break;
        }
    }

    // Handle cull mode from two-sided property
    int twoSided;
    if (aiGetMaterialInteger(aiMat, AI_MATKEY_TWOSIDED, &twoSided) == AI_SUCCESS) {
        if (twoSided) {
            material->cullMode = R3D_CULL_NONE;
        }
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_materials(const r3d_importer_t* importer, R3D_Model* model, r3d_importer_texture_cache_t* textureCache)
{
    if (!model || !importer || !textureCache || !r3d_importer_is_valid(importer)) {
        TraceLog(LOG_ERROR, "RENDER: Invalid parameters for material loading");
        return false;
    }

    model->materialCount = r3d_importer_get_material_count(importer);
    model->materials = RL_CALLOC(model->materialCount, sizeof(R3D_Material));

    if (!model->materials) {
        TraceLog(LOG_ERROR, "RENDER: Unable to allocate memory for materials");
        return false;
    }

    for (int i = 0; i < model->materialCount; i++) {
        load_material(&model->materials[i], importer, textureCache, i);
    }

    return true;
}
