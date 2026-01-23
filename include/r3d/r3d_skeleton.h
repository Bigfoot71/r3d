/* r3d_skeleton.h -- R3D Skeleton Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SKELETON_H
#define R3D_SKELETON_H

#include "./r3d_platform.h"
#include "./r3d_importer.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Skeleton
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Stores bone information for skeletal animation.
 *
 * Contains the bone name and the index of its parent bone.
 */
typedef struct R3D_BoneInfo {
    char name[32];   ///< Bone name (max 31 characters + null terminator).
    int parent;      ///< Index of the parent bone (-1 if root).
} R3D_BoneInfo;

/**
 * @brief Represents a skeletal hierarchy used for skinning.
 *
 * Defines the bone structure, reference poses, and inverse bind matrices
 * required for skeletal animation. The skeleton provides both local and
 * global bind poses used during skinning and animation playback.
 */
typedef struct R3D_Skeleton {

    R3D_BoneInfo* bones;    ///< Array of bone descriptors defining the hierarchy and names.
    int boneCount;          ///< Total number of bones in the skeleton.

    Matrix* localBind;      ///< Bind pose matrices relative to parent
    Matrix* modelBind;      ///< Bind pose matrices in model/global space
    Matrix* invBind;        ///< Inverse bind matrices (model space) for skinning
    Matrix rootBind;        ///< Root correction if local bind is not identity

    uint32_t skinTexture;   ///< Texture ID that contains the bind pose for GPU skinning. This is a 1D Texture RGBA16F 4*boneCount.

} R3D_Skeleton;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads a skeleton hierarchy from a 3D model file.
 *
 * Skeletons are automatically loaded when importing a model,
 * but can be loaded manually for advanced use cases.
 *
 * @param filePath Path to the model file containing the skeleton data.
 * @return Return the loaded R3D_Skeleton.
 */
R3DAPI R3D_Skeleton R3D_LoadSkeleton(const char* filePath);

/**
 * @brief Loads a skeleton hierarchy from memory data.
 *
 * Allows manual loading of skeletons directly from a memory buffer.
 * Typically used for advanced or custom asset loading workflows.
 *
 * @param data Pointer to the memory buffer containing skeleton data.
 * @param size Size of the memory buffer in bytes.
 * @param hint Optional format hint (can be NULL).
 * @return Return the loaded R3D_Skeleton.
 */
R3DAPI R3D_Skeleton R3D_LoadSkeletonFromMemory(const void* data, unsigned int size, const char* hint);

/**
 * @brief Loads a skeleton hierarchy from an existing importer.
 *
 * Extracts the skeleton data from a previously loaded importer instance.
 *
 * @param importer Importer instance to extract the skeleton from.
 * @return Return the loaded R3D_Skeleton.
 */
R3DAPI R3D_Skeleton R3D_LoadSkeletonFromImporter(const R3D_Importer* importer);

/**
 * @brief Frees the memory allocated for a skeleton.
 *
 * @param skeleton R3D_Skeleton to destroy.
 */
R3DAPI void R3D_UnloadSkeleton(R3D_Skeleton skeleton);

/**
 * @brief Check if a skeleton is valid.
 * 
 * Returns true if atleast the texBindPose is greater than zero.
 *
 * @param skeleton The skeleton to check.
 * @return true if valid, false otherwise.
 */
R3DAPI bool R3D_IsSkeletonValid(R3D_Skeleton skeleton);

/**
 * @brief Returns the index of the bone with the given name.
 * 
 * @param skeleton Skeleton to search in.
 * @param boneName Name of the bone to find.
 * @return Index of the bone, or a negative value if not found.
 */
R3DAPI int R3D_GetSkeletonBoneIndex(R3D_Skeleton skeleton, const char* boneName);

/**
 * @brief Returns a pointer to the bone with the given name.
 * 
 * @param skeleton Skeleton to search in.
 * @param boneName Name of the bone to find.
 * @return Pointer to the bone, or NULL if not found.
 */
R3DAPI R3D_BoneInfo* R3D_GetSkeletonBone(R3D_Skeleton skeleton, const char* boneName);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Skeleton

#endif // R3D_SKELETON_H
