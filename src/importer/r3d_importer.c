/* r3d_importer.c -- Module to manage model imports via assimp.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"
#include <assimp/cimport.h>
#include <r3d_config.h>

// ========================================
// PRIVATE FUNCTIONS
// ========================================

static void build_bone_mapping(r3d_importer_t* importer)
{
    importer->boneMap = NULL;
    importer->boneCount = 0;

    for (uint32_t meshIdx = 0; meshIdx < importer->scene->mNumMeshes; meshIdx++)
    {
        const struct aiMesh* mesh = importer->scene->mMeshes[meshIdx];
        if (!mesh || !mesh->mNumBones) continue;

        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++)
        {
            const struct aiBone* bone = mesh->mBones[boneIdx];
            if (!bone) continue;

            const char* boneName = bone->mName.data;

            // Check if the bone already exists
            r3d_bone_map_entry_t* entry = NULL;
            HASH_FIND_STR(importer->boneMap, boneName, entry);

            if (entry == NULL) {
                entry = RL_MALLOC(sizeof(r3d_bone_map_entry_t));
                strncpy(entry->name, boneName, sizeof(entry->name) - 1);
                entry->name[sizeof(entry->name) - 1] = '\0';
                entry->index = importer->boneCount;

                HASH_ADD_STR(importer->boneMap, name, entry);
                importer->boneCount++;
            }
        }
    }

    if (importer->boneCount > 0) {
        R3D_TRACELOG(LOG_DEBUG, "Built bone mapping with %d bones", importer->boneCount);
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_create_from_file(r3d_importer_t* importer, const char* filePath)
{
    const uint32_t flags = (
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices
    );

    const struct aiScene* scene = aiImportFile(filePath, flags);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        R3D_TRACELOG(LOG_ERROR, "Assimp error; %s", aiGetErrorString());
        return false;
    }

    importer->scene = scene;
    importer->boneMap = NULL;
    importer->boneCount = 0;

    build_bone_mapping(importer);
    
    return true;
}

bool r3d_importer_create_from_memory(r3d_importer_t* importer, const void* data, uint32_t size, const char* hint)
{
    const uint32_t flags = (
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenNormals |
        aiProcess_CalcTangentSpace |
        aiProcess_JoinIdenticalVertices
    );

    const struct aiScene* scene = aiImportFileFromMemory(data, size, flags, hint);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        R3D_TRACELOG(LOG_ERROR, "Assimp error; %s", aiGetErrorString());
        return false;
    }

    importer->scene = scene;
    importer->boneMap = NULL;
    importer->boneCount = 0;

    build_bone_mapping(importer);
    
    return true;
}

void r3d_importer_destroy(r3d_importer_t* importer)
{
    if (!importer) return;

    r3d_bone_map_entry_t *entry, *tmp;
    HASH_ITER(hh, importer->boneMap, entry, tmp) {
        HASH_DEL(importer->boneMap, entry);
        RL_FREE(entry);
    }

    aiReleaseImport(importer->scene);
}

int r3d_importer_get_bone_index(const r3d_importer_t* importer, const char* name)
{
    if (!importer || !name) return -1;
    
    r3d_bone_map_entry_t* entry = NULL;
    HASH_FIND_STR(importer->boneMap, name, entry);
    
    return entry ? entry->index : -1;
}
