/* r3d_importer.c -- R3D Importer Module
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_importer.h>
#include <r3d_config.h>
#include <raylib.h>
#include <stddef.h>

#ifdef R3D_SUPPORT_ASSIMP
#   include "./importer/r3d_importer_internal.h"
#   include "./common/r3d_helper.h"
#   include <assimp/cimport.h>
#endif

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

#ifdef R3D_SUPPORT_ASSIMP

static void determine_importer_name(char* outName, size_t outSize, const struct aiScene* scene, const char* hint)
{
    if (!outName || outSize == 0) return;

    if (scene && scene->mMetaData) {
        struct aiMetadata* meta = scene->mMetaData;
        for (unsigned int i = 0; i < meta->mNumProperties; i++) {
            if ((strcmp(meta->mKeys[i].data, "SourceAsset_Filename") == 0 ||
                 strcmp(meta->mKeys[i].data, "FileName") == 0) &&
                meta->mValues[i].mType == AI_AISTRING)
            {
                struct aiString* str = meta->mValues[i].mData;
                const char* filename = strrchr(str->data, '/');
                if (!filename) filename = strrchr(str->data, '\\');
                filename = filename ? filename + 1 : str->data;
                r3d_string_format(outName, outSize, "memory data (%s)", filename);
                return;
            }
        }
    }

    if (hint && hint[0] != '\0') {
        r3d_string_format(outName, outSize, "memory data (%s)", hint);
        return;
    }

    r3d_string_format(outName, outSize, "memory data");
}

static void build_bone_mapping(R3D_Importer* importer)
{
    int totalBones = 0;
    for (uint32_t meshIdx = 0; meshIdx < importer->scene->mNumMeshes; meshIdx++) {
        const struct aiMesh* mesh = importer->scene->mMeshes[meshIdx];
        if (mesh && mesh->mNumBones) totalBones += mesh->mNumBones;
    }

    if (totalBones == 0) {
        importer->bones.array = NULL;
        importer->bones.head = NULL;
        importer->bones.count = 0;
        return;
    }

    importer->bones.array = RL_MALLOC(totalBones * sizeof(r3d_importer_bone_entry_t));
    importer->bones.head = NULL;
    importer->bones.count = 0;

    for (uint32_t meshIdx = 0; meshIdx < importer->scene->mNumMeshes; meshIdx++)
    {
        const struct aiMesh* mesh = importer->scene->mMeshes[meshIdx];
        if (!mesh || !mesh->mNumBones) continue;

        for (uint32_t boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++)
        {
            const struct aiBone* bone = mesh->mBones[boneIdx];
            if (!bone) continue;

            const char* boneName = bone->mName.data;

            r3d_importer_bone_entry_t* entry = NULL;
            HASH_FIND_STR(importer->bones.head, boneName, entry);
            if (entry != NULL) continue;

            entry = &importer->bones.array[importer->bones.count];

            strncpy(entry->name, boneName, sizeof(entry->name) - 1);
            entry->name[sizeof(entry->name) - 1] = '\0';
            entry->index = importer->bones.count;

            HASH_ADD_STR(importer->bones.head, name, entry);
            importer->bones.count++;
        }
    }

    if (importer->bones.count > 0) {
        R3D_TRACELOG(LOG_DEBUG, "Built bone mapping with %d bones", importer->bones.count);
    }
}

#endif // R3D_SUPPORT_ASSIMP

// ========================================
// PUBLIC FUNCTIONS
// ========================================

R3D_Importer* R3D_LoadImporter(const char* filePath, R3D_ImportFlags flags)
{
#ifdef R3D_SUPPORT_ASSIMP
    enum aiPostProcessSteps aiFlags = POST_PROCESS_PRESET_FAST;
    if (BIT_TEST(flags, R3D_IMPORT_QUALITY)) {
        aiFlags = POST_PROCESS_PRESET_QUALITY;
    }

    const struct aiScene* scene = aiImportFile(filePath, aiFlags);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        R3D_TRACELOG(LOG_ERROR, "Assimp failed to load '%s': %s", filePath, aiGetErrorString());
        return NULL;
    }

    R3D_Importer* importer = RL_CALLOC(1, sizeof(*importer));
    importer->scene = scene;
    importer->flags = flags;

    strncpy(importer->name, filePath, sizeof(importer->name) - 1);
    importer->name[sizeof(importer->name) - 1] = '\0';

    build_bone_mapping(importer);

    R3D_TRACELOG(LOG_INFO, "Importer loaded successfully: '%s'", filePath);

    return importer;

#else
    R3D_TRACELOG(LOG_WARNING, "Cannot load '%s': built without Assimp support", filePath);
    return NULL;

#endif // R3D_SUPPORT_ASSIMP
}

R3D_Importer* R3D_LoadImporterFromMemory(const void* data, unsigned int size, const char* hint, R3D_ImportFlags flags)
{
#ifdef R3D_SUPPORT_ASSIMP
    enum aiPostProcessSteps aiFlags = POST_PROCESS_PRESET_FAST;
    if (BIT_TEST(flags, R3D_IMPORT_QUALITY)) {
        aiFlags = POST_PROCESS_PRESET_QUALITY;
    }

    const struct aiScene* scene = aiImportFileFromMemory(data, size, aiFlags, hint);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        if (hint && hint[0] != '\0') {
            R3D_TRACELOG(LOG_ERROR, "Assimp failed to load memory asset '%s': %s", hint, aiGetErrorString());
        }
        else {
            R3D_TRACELOG(LOG_ERROR, "Assimp failed to load memory asset: %s", aiGetErrorString());
        }
        return NULL;
    }

    R3D_Importer* importer = RL_CALLOC(1, sizeof(*importer));
    importer->scene = scene;
    importer->flags = flags;

    determine_importer_name(importer->name, sizeof(importer->name), scene, hint);
    build_bone_mapping(importer);

    if (hint && hint[0] != '\0') {
        R3D_TRACELOG(LOG_INFO, "Importer loaded successfully from memory: '%s'", hint);
    }
    else {
        R3D_TRACELOG(LOG_INFO, "Importer loaded successfully from memory");
    }

    return importer;

#else
    if (hint && hint[0] != '\0') {
        R3D_TRACELOG(LOG_WARNING, "Cannot load '%s' from memory: built without Assimp support", hint);
    }
    else {
        R3D_TRACELOG(LOG_WARNING, "Cannot load asset from memory: built without Assimp support");
    }

    return NULL;

#endif // R3D_SUPPORT_ASSIMP
}

void R3D_UnloadImporter(R3D_Importer* importer)
{
#ifdef R3D_SUPPORT_ASSIMP
    if (!importer) return;

    HASH_CLEAR(hh, importer->bones.head);

    if (importer->bones.array) {
        RL_FREE(importer->bones.array);
    }

    aiReleaseImport(importer->scene);
    RL_FREE(importer);

#else
    (void)importer;

#endif // R3D_SUPPORT_ASSIMP
}
