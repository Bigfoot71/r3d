/* r3d_importer_skeleton.c -- Module to import skeleton from assimp scene.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"

#include <assimp/mesh.h>
#include <raylib.h>
#include <string.h>
#include <glad.h>

#include "../common/r3d_math.h"

// ========================================
// INTERNAL CONTEXT
// ========================================

typedef struct {
    const r3d_importer_t* importer;
    R3D_BoneInfo* bones;
    Matrix* boneOffsets;
    Matrix* bindLocal;
    Matrix* bindPose;
    int boneCount;
} skeleton_build_context_t;

// ========================================
// RECURSIVE HIERARCHY BUILD
// ========================================

static void build_skeleton_recursive(
    skeleton_build_context_t* ctx,
    const struct aiNode* node,
    int parentIndex,
    Matrix parentTransform)
{
    if (!node) return;

    Matrix localTransform = r3d_importer_cast(node->mTransformation);
    Matrix globalTransform = r3d_matrix_multiply(&localTransform, &parentTransform);

    // Check if this node is a bone
    int currentIndex = r3d_importer_get_bone_index(ctx->importer, node->mName.data);

    if (currentIndex >= 0) {
        // Store bone data
        ctx->bindPose[currentIndex] = globalTransform;
        ctx->bindLocal[currentIndex] = localTransform;

        strncpy(ctx->bones[currentIndex].name, node->mName.data, sizeof(ctx->bones[currentIndex].name) - 1);
        ctx->bones[currentIndex].name[sizeof(ctx->bones[currentIndex].name) - 1] = '\0';
        ctx->bones[currentIndex].parent = parentIndex;

        // This bone becomes the parent for its children
        parentIndex = currentIndex;
    }

    // Recursively process children
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        build_skeleton_recursive(ctx, node->mChildren[i], parentIndex, globalTransform);
    }
}

// ========================================
// BIND POSE TEXTURE UPLOAD
// ========================================

static void upload_skeleton_bind_pose(R3D_Skeleton* skeleton)
{
    Matrix* finalBindPose = RL_MALLOC(skeleton->boneCount * sizeof(Matrix));
    r3d_matrix_multiply_batch(finalBindPose, skeleton->boneOffsets, skeleton->bindPose, skeleton->boneCount);

    glGenTextures(1, &skeleton->texBindPose);
    glBindTexture(GL_TEXTURE_1D, skeleton->texBindPose);
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

bool r3d_importer_load_skeleton(const r3d_importer_t* importer, R3D_Skeleton* skeleton)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        TraceLog(LOG_ERROR, "RENDER: Invalid importer for skeleton processing");
        return false;
    }

    int boneCount = r3d_importer_get_bone_count(importer);
    if (boneCount == 0) {
        return true; // No skeleton in this model
    }

    // Allocate bone arrays
    skeleton->bones = RL_MALLOC(boneCount * sizeof(R3D_BoneInfo));
    skeleton->boneOffsets = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->bindLocal = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->bindPose = RL_MALLOC(boneCount * sizeof(Matrix));
    skeleton->boneCount = boneCount;

    if (!skeleton->bones || !skeleton->boneOffsets || !skeleton->bindLocal || !skeleton->bindPose) {
        TraceLog(LOG_ERROR, "RENDER: Failed to allocate memory for skeleton bones");
        RL_FREE(skeleton->bones);
        RL_FREE(skeleton->boneOffsets);
        RL_FREE(skeleton->bindLocal);
        RL_FREE(skeleton->bindPose);
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
                skeleton->boneOffsets[boneIdx] = r3d_importer_cast(bone->mOffsetMatrix);
            }
        }
    }
    
    // Build hierarchy and bind poses in single traversal
    skeleton_build_context_t ctx = {
        .importer = importer,
        .bones = skeleton->bones,
        .boneOffsets = skeleton->boneOffsets,
        .bindLocal = skeleton->bindLocal,
        .bindPose = skeleton->bindPose,
        .boneCount = boneCount
    };

    build_skeleton_recursive(&ctx, r3d_importer_get_root(importer), -1, R3D_MATRIX_IDENTITY);
    upload_skeleton_bind_pose(skeleton);

    TraceLog(LOG_INFO, "RENDER: Loaded skeleton with %d bones", boneCount);

    return true;
}
