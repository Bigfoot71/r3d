/* r3d_importer.c -- R3D Importer Module
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./importer/r3d_importer_internal.h"
#include "./common/r3d_helper.h"
#include <assimp/cimport.h>
#include <r3d_config.h>

// ========================================
// INTERNAL CONSTANTS
// ========================================

#define POST_PROCESS_PRESET_FAST        \
    aiProcess_CalcTangentSpace      |   \
    aiProcess_GenNormals            |   \
    aiProcess_JoinIdenticalVertices |   \
    aiProcess_Triangulate           |   \
    aiProcess_GenUVCoords           |   \
    aiProcess_SortByPType           |   \
    aiProcess_FlipUVs

#define POST_PROCESS_PRESET_QUALITY        \
    aiProcess_CalcTangentSpace          |  \
    aiProcess_GenSmoothNormals          |  \
    aiProcess_JoinIdenticalVertices     |  \
    aiProcess_ImproveCacheLocality      |  \
    aiProcess_LimitBoneWeights          |  \
    aiProcess_RemoveRedundantMaterials  |  \
    aiProcess_SplitLargeMeshes          |  \
    aiProcess_Triangulate               |  \
    aiProcess_GenUVCoords               |  \
    aiProcess_SortByPType               |  \
    aiProcess_FindDegenerates           |  \
    aiProcess_FindInvalidData           |  \
    aiProcess_FlipUVs

// ========================================
// PRIVATE FUNCTIONS
// ========================================

static void build_bone_mapping(R3D_Importer* importer)
{
    int totalBones = 0;
    for (uint32_t meshIdx = 0; meshIdx < importer->scene->mNumMeshes; meshIdx++) {
        const struct aiMesh* mesh = importer->scene->mMeshes[meshIdx];
        if (mesh && mesh->mNumBones) totalBones += mesh->mNumBones;
    }

    if (totalBones == 0) {
        importer->boneMapArray = NULL;
        importer->boneMapHead = NULL;
        importer->boneCount = 0;
        return;
    }

    importer->boneMapArray = RL_MALLOC(totalBones * sizeof(r3d_bone_map_entry_t));
    importer->boneMapHead = NULL;
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

            r3d_bone_map_entry_t* entry = NULL;
            HASH_FIND_STR(importer->boneMapHead, boneName, entry);
            if (entry != NULL) continue;

            entry = &importer->boneMapArray[importer->boneCount];

            strncpy(entry->name, boneName, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            entry->index = importer->boneCount;

            HASH_ADD_STR(importer->boneMapHead, name, entry);
            importer->boneCount++;
        }
    }

    if (importer->boneCount > 0) {
        R3D_TRACELOG(LOG_DEBUG, "Built bone mapping with %d bones", importer->boneCount);
    }
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

R3D_Importer* R3D_LoadImporter(const char* filePath, R3D_ImportFlags flags)
{
    enum aiPostProcessSteps aiFlags = POST_PROCESS_PRESET_FAST;
    if (BIT_TEST(flags, R3D_IMPORT_QUALITY)) {
        aiFlags = POST_PROCESS_PRESET_QUALITY;
    }

    const struct aiScene* scene = aiImportFile(filePath, aiFlags);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        R3D_TRACELOG(LOG_ERROR, "Assimp error; %s", aiGetErrorString());
        return NULL;
    }

    R3D_Importer* importer = RL_CALLOC(1, sizeof(*importer));
    importer->scene = scene;
    importer->flags = flags;

    build_bone_mapping(importer);

    return importer;
}

R3D_Importer* R3D_LoadImporterFromMemory(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags)
{
    enum aiPostProcessSteps aiFlags = POST_PROCESS_PRESET_FAST;
    if (BIT_TEST(flags, R3D_IMPORT_QUALITY)) {
        aiFlags = POST_PROCESS_PRESET_QUALITY;
    }

    const struct aiScene* scene = aiImportFileFromMemory(data, size, aiFlags, hint);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        R3D_TRACELOG(LOG_ERROR, "Assimp error; %s", aiGetErrorString());
        return NULL;
    }

    R3D_Importer* importer = RL_CALLOC(1, sizeof(*importer));
    importer->scene = scene;
    importer->flags = flags;

    build_bone_mapping(importer);

    return importer;
}

void R3D_UnloadImporter(R3D_Importer* importer)
{
    if (!importer) return;

    HASH_CLEAR(hh, importer->boneMapHead);

    if (importer->boneMapArray) {
        RL_FREE(importer->boneMapArray);
    }

    aiReleaseImport(importer->scene);
    RL_FREE(importer);
}
