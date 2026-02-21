/* r3d_model.h -- R3D Model Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_material.h>
#include <r3d/r3d_skeleton.h>
#include <r3d/r3d_model.h>
#include <r3d/r3d_mesh.h>
#include <r3d_config.h>
#include <stdlib.h>

#ifdef R3D_SUPPORT_ASSIMP
#   include "./importer/r3d_importer_internal.h"
#   include "./r3d_core_state.h"
#endif

// ========================================
// INTERNAL FUNCTIONS
// ========================================

#ifdef R3D_SUPPORT_ASSIMP

static bool load_model_components(R3D_Model* model, const R3D_Importer* importer)
{
    if (!r3d_importer_load_meshes(importer, model)) {
        return false;
    }

    if (!r3d_importer_load_skeleton(importer, &model->skeleton)) {
        return false;
    }

    r3d_importer_texture_cache_t* textureCache = NULL;
    textureCache = r3d_importer_load_texture_cache(importer, R3D.colorSpace, R3D.textureFilter);
    if (textureCache == NULL) {
        return false;
    }

    if (!r3d_importer_load_materials(importer, &model->materials, &model->materialCount, textureCache)) {
        r3d_importer_unload_texture_cache(textureCache, true);
        return false;
    }

    r3d_importer_unload_texture_cache(textureCache, false);

    return true;
}

#endif // R3D_SUPPORT_ASSIMP

// ========================================
// PUBLIC API
// ========================================

R3D_Model R3D_LoadModel(const char* filePath)
{
    return R3D_LoadModelEx(filePath, 0);
}

R3D_Model R3D_LoadModelEx(const char* filePath, R3D_ImportFlags flags)
{
    R3D_Model model = {0};

#ifdef R3D_SUPPORT_ASSIMP
    R3D_Importer* importer = R3D_LoadImporter(filePath, flags);
    if (importer == NULL) return model;

    model = R3D_LoadModelFromImporter(importer);

    R3D_UnloadImporter(importer);

#else
    R3D_TRACELOG(LOG_WARNING, "Cannot load '%s': built without Assimp support", filePath);

#endif // R3D_SUPPORT_ASSIMP

    return model;
}

R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size, const char* hint)
{
    return R3D_LoadModelFromMemoryEx(data, size, hint, 0);
}

R3D_Model R3D_LoadModelFromMemoryEx(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags)
{
    R3D_Model model = {0};

#ifdef R3D_SUPPORT_ASSIMP
    R3D_Importer* importer = R3D_LoadImporterFromMemory(data, size, hint, flags);
    if (importer == NULL) return model;

    model = R3D_LoadModelFromImporter(importer);

    R3D_UnloadImporter(importer);

#else
    if (hint && hint[0] != '\0') {
        R3D_TRACELOG(LOG_WARNING, "Cannot load '%s' from memory: built without Assimp support", hint);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Cannot load asset from memory: built without Assimp support");
    }

#endif // R3D_SUPPORT_ASSIMP

    return model;
}

R3D_Model R3D_LoadModelFromImporter(const R3D_Importer* importer)
{
    R3D_Model model = {0};

#ifdef R3D_SUPPORT_ASSIMP
    if (importer == NULL) {
        R3D_TRACELOG(LOG_WARNING, "Cannot load model from importer: NULL importer");
        return model;
    }

    if (load_model_components(&model, importer)) {
        R3D_TRACELOG(LOG_INFO, "Model loaded successfully: '%s'", importer->name);
        R3D_TRACELOG(LOG_INFO, "    > Materials count: %i", model.materialCount);
        R3D_TRACELOG(LOG_INFO, "    > Meshes count: %i", model.meshCount);
        R3D_TRACELOG(LOG_INFO, "    > Bones count: %i", model.skeleton.boneCount);
    }
    else {
        R3D_UnloadModel(model, false);
        memset(&model, 0, sizeof(model));

        R3D_TRACELOG(LOG_WARNING, "Failed to load model: '%s'", importer->name);
    }

#else
    R3D_TRACELOG(LOG_WARNING, "Cannot load model from importer: built without Assimp support");

#endif // R3D_SUPPORT_ASSIMP

    return model;
}

void R3D_UnloadModel(R3D_Model model, bool unloadMaterials)
{
    R3D_UnloadSkeleton(model.skeleton);

    if (model.meshes != NULL) {
        for (int i = 0; i < model.meshCount; i++) {
            R3D_UnloadMesh(model.meshes[i]);
        }
    }

    if (model.meshData != NULL) {
        for (int i = 0; i < model.meshCount; i++) {
            R3D_UnloadMeshData(model.meshData[i]);
        }
    }

    if (unloadMaterials && model.materials != NULL) {
        for (int i = 0; i < model.materialCount; i++) {
            R3D_UnloadMaterial(model.materials[i]);
        }
    }

    RL_FREE(model.meshMaterials);
    RL_FREE(model.materials);
    RL_FREE(model.meshData);
    RL_FREE(model.meshes);
}
