/* r3d_model.h -- R3D Model Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_material.h>
#include <r3d/r3d_skeleton.h>
#include <r3d/r3d_model.h>
#include <r3d/r3d_mesh.h>

#include "./details/importer/r3d_importer.h"
#include "./r3d_state.h"

// ========================================
// INTERNAL FUNCTIONS
// ========================================

static bool import_model(r3d_importer_t* importer, R3D_Model* model)
{
    r3d_importer_texture_cache_t* textureCache = r3d_importer_load_texture_cache(importer, R3D.state.loading.textureFilter);
    if (textureCache == NULL) {
        r3d_importer_destroy(importer);
        return false;
    }

    if (!r3d_importer_load_meshes(importer, model)) {
        r3d_importer_destroy(importer);
        return false;
    }

    if (!r3d_importer_load_skeleton(importer, &model->skeleton)) {
        r3d_importer_destroy(importer);
        return false;
    }

    if (!r3d_importer_load_materials(importer, model, textureCache)) {
        r3d_importer_destroy(importer);
        return false;
    }

    return true;
}

// ========================================
// PUBLIC API
// ========================================

R3D_Model R3D_LoadModel(const char* filePath)
{
    R3D_Model model = { 0 };

    r3d_importer_t importer = { 0 };
    if (!r3d_importer_create_from_file(&importer, filePath)) {
        return model;
    }

    if (!import_model(&importer, &model)) {
        R3D_UnloadModel(&model, true);
    }

    r3d_importer_destroy(&importer);

    return model;
}

R3D_Model R3D_LoadModelFromMemory(const void* data, unsigned int size, const char* hint)
{
    R3D_Model model = { 0 };

    r3d_importer_t importer = { 0 };
    if (!r3d_importer_create_from_memory(&importer, data, size, hint)) {
        return model;
    }

    if (!import_model(&importer, &model)) {
        R3D_UnloadModel(&model, true);
    }

    r3d_importer_destroy(&importer);

    return model;
}

R3D_Model R3D_LoadModelFromMesh(const R3D_Mesh* mesh)
{
    R3D_Model model = { 0 };

    if (!mesh) {
        return model;
    }

    model.meshes = RL_MALLOC(sizeof(R3D_Mesh));
    model.meshes[0] = *mesh;
    model.meshCount = 1;

    model.materials = RL_MALLOC(sizeof(R3D_Material));
    model.materials[0] = R3D_GetDefaultMaterial();
    model.materialCount = 1;

    model.meshMaterials = RL_MALLOC(sizeof(int));
    model.meshMaterials[0] = 0;

    model.aabb = mesh->aabb;

    return model;
}

void R3D_UnloadModel(R3D_Model* model, bool unloadMaterials)
{
    R3D_UnloadSkeleton(&model->skeleton);

    if (model->meshes != NULL) {
        for (int i = 0; i < model->meshCount; i++) {
            R3D_UnloadMesh(&model->meshes[i]);
        }
    }

    if (unloadMaterials && model->materials != NULL) {
        for (int i = 0; i < model->materialCount; i++) {
            R3D_UnloadMaterial(&model->materials[i]);
        }
    }

    RL_FREE(model->meshMaterials);
    RL_FREE(model->materials);
    RL_FREE(model->meshes);
}
