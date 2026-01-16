/* r3d_skeleton.h -- R3D Skeleton Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_skeleton.h>
#include <stdlib.h>
#include <stddef.h>
#include <glad.h>

#include "./importer/r3d_importer.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Skeleton R3D_LoadSkeleton(const char* filePath)
{
    R3D_Skeleton skeleton = {0};

    r3d_importer_t importer = {0};
    if (!r3d_importer_create_from_file(&importer, filePath)) {
        return skeleton;
    }

    r3d_importer_load_skeleton(&importer, &skeleton);
    r3d_importer_destroy(&importer);

    return skeleton;
}

R3D_Skeleton R3D_LoadSkeletonFromData(const void* data, unsigned int size, const char* hint)
{
    R3D_Skeleton skeleton = {0};

    r3d_importer_t importer = {0};
    if (!r3d_importer_create_from_memory(&importer, data, size, hint)) {
        return skeleton;
    }

    r3d_importer_load_skeleton(&importer, &skeleton);
    r3d_importer_destroy(&importer);

    return skeleton;
}

void R3D_UnloadSkeleton(R3D_Skeleton skeleton)
{
    if (skeleton.skinTexture > 0) {
        glDeleteTextures(1, &skeleton.skinTexture);
    }

    RL_FREE(skeleton.bones);
    RL_FREE(skeleton.invBind);
    RL_FREE(skeleton.modelBind);
    RL_FREE(skeleton.localBind);
}

bool R3D_IsSkeletonValid(R3D_Skeleton skeleton)
{
    return (skeleton.skinTexture > 0);
}

int R3D_GetSkeletonBoneIndex(R3D_Skeleton skeleton, const char* boneName)
{
    for (int i = 0; i < skeleton.boneCount; i++) {
        if (strcmp(skeleton.bones[i].name, boneName) == 0) {
            return i;
        }
    }
    return -1;
}

R3D_BoneInfo* R3D_GetSkeletonBone(R3D_Skeleton skeleton, const char* boneName)
{
    for (int i = 0; i < skeleton.boneCount; i++) {
        if (strcmp(skeleton.bones[i].name, boneName) == 0) {
            return &skeleton.bones[i];
        }
    }
    return NULL;
}
