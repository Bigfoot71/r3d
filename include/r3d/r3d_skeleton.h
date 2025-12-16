/* r3d_skeleton.h -- R3D Skeleton Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_SKELETON_H
#define R3D_SKELETON_H

#include "./r3d_api.h"
#include <raylib.h>

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

    Matrix* boneOffsets;    ///< Inverse bind matrices, one per bone. Transform vertices from mesh space to bone space (used in skinning).
    Matrix* bindLocal;      ///< Bind pose transforms in local bone space (relative to parent).
    Matrix* bindPose;       ///< Bind pose transforms in model space (global). Used as the default pose when not animated.

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
R3DAPI R3D_Skeleton R3D_LoadSkeletonFromData(const void* data, unsigned int size, const char* hint);

/**
 * @brief Frees the memory allocated for a skeleton.
 *
 * @param skeleton Pointer to the R3D_Skeleton to destroy.
 */
R3DAPI void R3D_UnloadSkeleton(R3D_Skeleton* skeleton);

/**
 * @brief Check if a skeleton is valid.
 * 
 * Returns true if the skeleton has bones, offsets, bind local matrices,
 * and bind pose matrices properly initialized.
 *
 * @param skeleton Pointer to the skeleton to check.
 * @return true if valid, false otherwise.
 */
R3DAPI bool R3D_IsSkeletonValid(const R3D_Skeleton* skeleton);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Skeleton

#endif // R3D_SKELETON_H
