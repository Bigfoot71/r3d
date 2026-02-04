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

#include "./importer/r3d_importer_internal.h"
#include "./r3d_core_state.h"

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

    R3D_Importer* importer = R3D_LoadImporter(filePath, flags);
    if (importer == NULL) return model;

    model = R3D_LoadModelFromImporter(importer);

    R3D_UnloadImporter(importer);

    return model;
}

R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size, const char* hint)
{
    return R3D_LoadModelFromMemory(data, size, hint);
}

R3D_Model R3D_LoadModelFromMemoryEx(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags)
{
    R3D_Model model = {0};

    R3D_Importer* importer = R3D_LoadImporterFromMemory(data, size, hint, flags);
    if (importer == NULL) return model;

    model = R3D_LoadModelFromImporter(importer);

    R3D_UnloadImporter(importer);

    return model;
}

R3DAPI R3D_Model R3D_LoadModelFromImporter(const R3D_Importer* importer)
{
    R3D_Model model = {0};

    if (importer == NULL) {
        R3D_TRACELOG(LOG_ERROR, "Cannot load model from NULL importer");
        return model;
    }

    if (!r3d_importer_load_meshes(importer, &model)) goto fail;
    if (!r3d_importer_load_skeleton(importer, &model.skeleton)) goto fail;

    r3d_importer_texture_cache_t* textureCache = r3d_importer_load_texture_cache(
        importer, R3D.colorSpace, R3D.textureFilter);
    if (textureCache == NULL) goto fail;

    if (!r3d_importer_load_materials(importer, &model, textureCache)) goto fail;

    r3d_importer_unload_texture_cache(textureCache, false);

    R3D_TRACELOG(LOG_INFO, "Model loaded successfully: '%s'", importer->name);
    R3D_TRACELOG(LOG_INFO, "    > Materials count: %i", model.materialCount);
    R3D_TRACELOG(LOG_INFO, "    > Meshes count: %i", model.meshCount);
    R3D_TRACELOG(LOG_INFO, "    > Bones count: %i", model.skeleton.boneCount);

    return model;

fail:
    r3d_importer_unload_texture_cache(textureCache, true);
    R3D_UnloadModel(model, false);
    memset(&model, 0, sizeof(model));

    R3D_TRACELOG(LOG_WARNING, "Failed to load model: '%s'", importer->name);

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
