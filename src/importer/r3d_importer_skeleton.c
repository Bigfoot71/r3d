/* r3d_importer_skeleton.c -- Module to import skeleton from assimp scene.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer_internal.h"

#include <assimp/mesh.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>
#include <glad.h>

#include "../common/r3d_math.h"

// ========================================
// INTERNAL CONTEXT
// ========================================

typedef struct {
    const R3D_Importer* importer;
    R3D_BoneInfo* bones;
    Matrix* invBind;
    Matrix* localBind;
    Matrix* modelBind;
    Matrix* rootBind;
    int boneCount;
} skeleton_build_context_t;

// ========================================
// RECURSIVE HIERARCHY BUILD
// ========================================

static void build_skeleton_recursive(
    skeleton_build_context_t* ctx,
    const struct aiNode* node,
    Matrix parentTransform,
    int parentIndex)
{
    if (!node) return;

    Matrix localTransform = r3d_importer_cast(node->mTransformation);
    Matrix modelTransform = r3d_matrix_multiply(&localTransform, &parentTransform);

    // Check if this node is a bone
    int currentIndex = r3d_importer_get_bone_index(ctx->importer, node->mName.data);

    if (currentIndex >= 0) {
        // Store bone matrices
        ctx->localBind[currentIndex] = localTransform;
        ctx->modelBind[currentIndex] = modelTransform;

        // Store bind root matrix
        if (parentIndex == -1) {
            Matrix invLocalTransform = MatrixInvert(localTransform);
            *ctx->rootBind = r3d_matrix_multiply(&invLocalTransform, &modelTransform);
        }

        // Store bone infos
        strncpy(ctx->bones[currentIndex].name, node->mName.data, sizeof(ctx->bones[currentIndex].name) - 1);
        ctx->bones[currentIndex].name[sizeof(ctx->bones[currentIndex].name) - 1] = '\0';
        ctx->bones[currentIndex].parent = parentIndex;

        // This bone becomes the parent for its children
        parentIndex = currentIndex;
    }

    // Recursively process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        build_skeleton_recursive(ctx, node->mChildren[i], modelTransform, parentIndex);
    }
}

// ========================================
// BIND POSE TEXTURE UPLOAD
// ========================================

static void upload_skeleton_bind_pose(R3D_Skeleton* skeleton)
{
    Matrix* finalBindPose = RL_MALLOC(skeleton->boneCount * sizeof(Matrix));
    r3d_matrix_multiply_batch(finalBindPose, skeleton->invBind, skeleton->modelBind, skeleton->boneCount);

    glGenTextures(1, &skeleton->skinTexture);
    glBindTexture(GL_TEXTURE_1D, skeleton->skinTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, 4 * skeleton->boneCount, 0, GL_RGBA, GL_FLOAT, finalBindPose);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_1D, 0);

    RL_FREE(finalBindPose);
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_skeleton(const R3D_Importer* importer, R3D_Skeleton* skeleton)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        R3D_TRACELOG(LOG_ERROR, "Invalid importer for skeleton processing");
        return false;
    }

    int boneCount = r3d_importer_get_bone_count(importer);
    if (boneCount == 0) {
        return true; // No skeleton in this model
    }

    // Allocate bone arrays
    skeleton->bones = RL_MALLOC(boneCount * sizeof(R3D_BoneInfo));
    skeleton->invBind = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->localBind = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->modelBind = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->boneCount = boneCount;

    if (!skeleton->bones || !skeleton->invBind || !skeleton->localBind || !skeleton->modelBind) {
        R3D_TRACELOG(LOG_ERROR, "Failed to allocate memory for skeleton bones");
        RL_FREE(skeleton->bones);
        RL_FREE(skeleton->invBind);
        RL_FREE(skeleton->localBind);
        RL_FREE(skeleton->modelBind);
        RL_FREE(skeleton);
        return false;
    }

    // Initialize parent indices to -1 (no parent)
    for (int i = 0; i < boneCount; i++) {
        skeleton->bones[i].parent = -1;
        memset(skeleton->bones[i].name, 0, sizeof(skeleton->bones[i].name));
    }

    // Fill bone offsets from meshes
    for (int m = 0; m < r3d_importer_get_mesh_count(importer); m++) {
        const struct aiMesh* mesh = r3d_importer_get_mesh(importer, m);

        for (unsigned int b = 0; b < mesh->mNumBones; b++) {
            const struct aiBone* bone = mesh->mBones[b];
            int boneIdx = r3d_importer_get_bone_index(importer, bone->mName.data);

            if (boneIdx >= 0) {
                skeleton->invBind[boneIdx] = r3d_importer_cast(bone->mOffsetMatrix);
            }
        }
    }
    
    // Build hierarchy and bind poses in single traversal
    skeleton_build_context_t ctx = {
        .importer = importer,
        .bones = skeleton->bones,
        .invBind = skeleton->invBind,
        .localBind = skeleton->localBind,
        .modelBind = skeleton->modelBind,
        .rootBind = &skeleton->rootBind,
        .boneCount = boneCount
    };

    build_skeleton_recursive(&ctx, r3d_importer_get_root(importer), R3D_MATRIX_IDENTITY, -1);
    upload_skeleton_bind_pose(skeleton);

    return true;
}
