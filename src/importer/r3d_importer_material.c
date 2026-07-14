/* r3d_importer_material.c -- Module to import materials from assimp scene.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer_internal.h"
#include "r3d/r3d_material.h"

#include <assimp/GltfMaterial.h>
#include <assimp/material.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>
#include <math.h>

// ========================================
// MATERIAL MAP LOADERS
// ========================================

static void load_map_albedo(R3D_Material* material, const struct aiMaterial* aiMat, r3d_importer_texture_cache_t* cache, int index)
{
    Texture2D* tex = r3d_importer_get_loaded_texture(cache, index, R3D_MAP_ALBEDO);
    if (tex) {
        material->albedo.texture = *tex;
    }

    struct aiColor4D color;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS) {
        material->albedo.color = r3d_importer_cast(color);
    }
    else if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS) {
        material->albedo.color = r3d_importer_cast(color);
    }

    // Opacity: try each source in order, stop as soon as alpha is modified
    // AI_MATKEY_OPACITY: direct opacity factor (1.0 = fully opaque)
    float opacityFactor;
    if (material->albedo.color.a == 255
            && aiGetMaterialFloat(aiMat, AI_MATKEY_OPACITY, &opacityFactor) == AI_SUCCESS) {
        material->albedo.color.a = (unsigned char)(opacityFactor * 255.0f);
    }

    // AI_MATKEY_TRANSPARENCYFACTOR: inverse of opacity (1.0 = fully transparent)
    if (material->albedo.color.a == 255
            && aiGetMaterialFloat(aiMat, AI_MATKEY_TRANSPARENCYFACTOR, &opacityFactor) == AI_SUCCESS) {
        material->albedo.color.a = (unsigned char)((1.0f - opacityFactor) * 255.0f);
    }

    // AI_MATKEY_TRANSMISSION_FACTOR: glTF transmission (1.0 = fully transparent)
    if (material->albedo.color.a == 255
            && aiGetMaterialFloat(aiMat, AI_MATKEY_TRANSMISSION_FACTOR, &opacityFactor) == AI_SUCCESS) {
        material->albedo.color.a = (unsigned char)((1.0f - opacityFactor) * 255.0f);
    }

    // AI_MATKEY_COLOR_TRANSPARENT: OBJ/MTL 'Tf' transmission filter
    // Tf 1 1 1 = fully transparent, Tf 0 0 0 = fully opaque (opposite of opacity)
    // Tf 1 1 1 is also the default for opaque materials, so we ignore it
    struct aiColor4D tf;
    if (material->albedo.color.a == 255
            && aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_TRANSPARENT, &tf) == AI_SUCCESS) {
        float transmission = tf.r * 0.2126f + tf.g * 0.7152f + tf.b * 0.0722f;
        if (transmission > 0.0f && transmission < 1.0f) {
            material->albedo.color.a = (unsigned char)((1.0f - transmission) * 255.0f);
        }
    }
}

static void load_map_emission(R3D_Material* material, const struct aiMaterial* aiMat, r3d_importer_texture_cache_t* cache, int index)
{
    Texture2D* tex = r3d_importer_get_loaded_texture(cache, index, R3D_MAP_EMISSION);
    if (tex) {
        material->emission.texture = *tex;
    }

    struct aiColor4D color;
    if (aiGetMaterialColor(aiMat, AI_MATKEY_COLOR_EMISSIVE, &color) == AI_SUCCESS) {
        material->emission.color = r3d_importer_cast(color);
    }

    float intensity;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_EMISSIVE_INTENSITY, &intensity) == AI_SUCCESS) {
        material->emission.energy = intensity;
    }
    else if (tex || material->emission.color.r || material->emission.color.g || material->emission.color.b) {
        // No explicit intensity but emission is present: default to 1.0
        material->emission.energy = 1.0f;
    }
}

static void load_map_orm(R3D_Material* material, const struct aiMaterial* aiMat, r3d_importer_texture_cache_t* cache, int index)
{
    Texture2D* tex = r3d_importer_get_loaded_texture(cache, index, R3D_MAP_ORM);
    if (tex) {
        material->orm.texture = *tex;
    }

    // Roughness: prefer PBR value, fallback to Phong shininess for OBJ/MTL
    float roughness;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS) {
        material->orm.roughness = roughness;
    }
    else {
        // OBJ/MTL fallback: derive roughness from Ns (specular exponent, range 0-1000)
        // Ns 1000 = very shiny = low roughness, Ns 0 = matte = high roughness
        float shininess = 0.0f;
        if (aiGetMaterialFloat(aiMat, AI_MATKEY_SHININESS, &shininess) == AI_SUCCESS && shininess > 0.0f) {
            float normalized = fminf(shininess / 1000.0f, 1.0f);
            float derived = 1.0f - sqrtf(normalized);

            // Modulate by specular strength if available: low strength = weaker specular = more rough
            float strength = 1.0f;
            if (aiGetMaterialFloat(aiMat, AI_MATKEY_SHININESS_STRENGTH, &strength) == AI_SUCCESS) {
                strength = fminf(fmaxf(strength, 0.0f), 1.0f);
                derived = derived + (1.0f - derived) * (1.0f - strength);
            }

            material->orm.roughness = fminf(fmaxf(derived, 0.0f), 1.0f);
        }
    }

    // Metalness: no equivalent in Phong, stays at default if not found
    float metalness;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_METALLIC_FACTOR, &metalness) == AI_SUCCESS) {
        material->orm.metalness = metalness;
    }
}

static void load_map_normal(R3D_Material* material, const struct aiMaterial* aiMat, r3d_importer_texture_cache_t* cache, int index)
{
    Texture2D* tex = r3d_importer_get_loaded_texture(cache, index, R3D_MAP_NORMAL);
    if (!tex) return;

    material->normal.texture = *tex;

    float scale;
    if (aiGetMaterialFloat(aiMat, AI_MATKEY_BUMPSCALING, &scale) == AI_SUCCESS) {
        material->normal.scale = scale;
    }
}

// ========================================
// MATERIAL PARAMETER LOADERS
// ========================================

static void load_param_blend_mode(R3D_Material* material, const struct aiMaterial* aiMat)
{
    // glTF alpha mode takes priority when present
    struct aiString alphaMode;
    if (aiGetMaterialString(aiMat, AI_MATKEY_GLTF_ALPHAMODE, &alphaMode) == AI_SUCCESS) {
        if (strcmp(alphaMode.data, "MASK") == 0) {
            float alphaCutoff;
            if (aiGetMaterialFloat(aiMat, AI_MATKEY_GLTF_ALPHACUTOFF, &alphaCutoff) == AI_SUCCESS) {
                material->alphaCutoff = alphaCutoff;
            }
            return;
        }
        if (strcmp(alphaMode.data, "BLEND") == 0) {
            material->transparencyMode = R3D_TRANSPARENCY_PREPASS;
            material->blendMode = R3D_BLEND_MIX;
            return;
        }
    }

    // Generic blend function
    int blendFunc;
    if (aiGetMaterialInteger(aiMat, AI_MATKEY_BLEND_FUNC, &blendFunc) == AI_SUCCESS) {
        switch (blendFunc) {
        case aiBlendMode_Default:
            // sColor*sAlpha + dColor*(1-sAlpha)
            material->transparencyMode = R3D_TRANSPARENCY_PREPASS;
            material->blendMode = R3D_BLEND_MIX;
            return;
        case aiBlendMode_Additive:
            // sColor*1 + dColor*1
            material->transparencyMode = R3D_TRANSPARENCY_DISABLED;
            material->blendMode = R3D_BLEND_ADDITIVE;
            return;
        default:
            break;
        }
    }

    // Fallback: infer blend mode from albedo alpha uniform
    // alpha == 0 is likely a degenerate material, ignore it
    if (material->albedo.color.a > 0 && material->albedo.color.a < 255) {
        material->transparencyMode = R3D_TRANSPARENCY_ALPHA;
        material->blendMode = R3D_BLEND_MIX;
    }
}

static void load_param_cull_mode(R3D_Material* material, const struct aiMaterial* aiMat)
{
    int twoSided;
    if (aiGetMaterialInteger(aiMat, AI_MATKEY_TWOSIDED, &twoSided) == AI_SUCCESS && twoSided) {
        material->cullMode = R3D_CULL_NONE;
    }
}

static void load_param_shading_mode(R3D_Material* material, const struct aiMaterial* aiMat)
{
    int shadingMode;
    if (aiGetMaterialInteger(aiMat, AI_MATKEY_SHADING_MODEL, &shadingMode) == AI_SUCCESS) {
        material->unlit = (shadingMode == aiShadingMode_Unlit);
    }
}

// ========================================
// MATERIAL LOADING (INTERNAL)
// ========================================

static void load_material(R3D_Material* material, const R3D_Importer* importer, r3d_importer_texture_cache_t* cache, int index)
{
    const struct aiMaterial* aiMat = r3d_importer_get_material(importer, index);

    *material = R3D_GetDefaultMaterial();

    load_map_albedo(material, aiMat, cache, index);
    load_map_emission(material, aiMat, cache, index);
    load_map_orm(material, aiMat, cache, index);
    load_map_normal(material, aiMat, cache, index);

    load_param_blend_mode(material, aiMat);
    load_param_cull_mode(material, aiMat);
    load_param_shading_mode(material, aiMat);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_materials(const R3D_Importer* importer, R3D_Material** materials, int* materialCount, r3d_importer_texture_cache_t* textureCache)
{
    if (!materials || !materialCount || !importer || !r3d_importer_is_valid(importer)) {
        R3D_TRACELOG(LOG_ERROR, "Invalid parameters for material loading");
        return false;
    }

    *materialCount = r3d_importer_get_material_count(importer);
    *materials = MemAlloc(*materialCount * sizeof(R3D_Material));

    if (*materials == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Unable to allocate memory for materials");
        return false;
    }

    for (int i = 0; i < *materialCount; i++) {
        load_material(&(*materials)[i], importer, textureCache, i);
    }

    return true;
}
