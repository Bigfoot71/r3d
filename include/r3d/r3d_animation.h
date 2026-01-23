/* r3d_animation.h -- R3D Animation Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ANIMATION_H
#define R3D_ANIMATION_H

#include "./r3d_platform.h"
#include "./r3d_importer.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup Animation
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Animation track storing keyframe times and values.
 *
 * Represents a single animated property (translation, rotation or scale).
 * Keys are sampled by time and interpolated at runtime.
 */
typedef struct R3D_AnimationTrack {
    const float* times;   ///< Keyframe times (sorted, in animation ticks).
    const void*  values;  ///< Keyframe values (Vector3 or Quaternion).
    int          count;   ///< Number of keyframes.
} R3D_AnimationTrack;

/**
 * @brief Animation channel controlling a single bone.
 *
 * Contains animation tracks for translation, rotation and scale.
 * The sampled tracks are combined to produce the bone local transform.
 */
typedef struct R3D_AnimationChannel {
    R3D_AnimationTrack translation; ///< Translation track (Vector3).
    R3D_AnimationTrack rotation;    ///< Rotation track (Quaternion).
    R3D_AnimationTrack scale;       ///< Scale track (Vector3).
    int boneIndex;                  ///< Index of the affected bone.
} R3D_AnimationChannel;

/**
 * @brief Represents a skeletal animation for a model.
 *
 * Contains all animation channels required to animate a skeleton.
 * Each channel corresponds to one bone and defines its transformation
 * (translation, rotation, scale) over time.
 */
typedef struct R3D_Animation {
    R3D_AnimationChannel* channels;     ///< Array of animation channels, one per animated bone.
    int channelCount;                   ///< Total number of channels in this animation.
    float ticksPerSecond;               ///< Playback rate; number of animation ticks per second.
    float duration;                     ///< Total length of the animation, in ticks.
    int boneCount;                      ///< Number of bones in the target skeleton.
    char name[32];                      ///< Animation name (null-terminated string).
} R3D_Animation;

/**
 * @brief Represents a collection of skeletal animations sharing the same skeleton.
 *
 * Holds multiple animations that can be applied to compatible models or skeletons.
 * Typically loaded together from a single 3D model file (e.g., GLTF, FBX) containing several animation clips.
 */
typedef struct R3D_AnimationLib {
    R3D_Animation* animations;          ///< Array of animations included in this library.
    int count;                          ///< Number of animations contained in the library.
} R3D_AnimationLib;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Loads animations from a model file.
 * @param filePath Path to the model file containing animations.
 * @param targetFrameRate Desired frame rate (FPS) for sampling the animations.
 * @return Pointer to an array of R3D_Animation, or NULL on failure.
 * @note Free the returned array using R3D_UnloadAnimationLib().
 */
R3DAPI R3D_AnimationLib R3D_LoadAnimationLib(const char* filePath);

/**
 * @brief Loads animations from memory data.
 * @param data Pointer to memory buffer containing model animation data.
 * @param size Size of the buffer in bytes.
 * @param hint Hint on the model format (can be NULL).
 * @param targetFrameRate Desired frame rate (FPS) for sampling the animations.
 * @return Pointer to an array of R3D_Animation, or NULL on failure.
 * @note Free the returned array using R3D_UnloadAnimationLib().
 */
R3DAPI R3D_AnimationLib R3D_LoadAnimationLibFromMemory(const void* data, unsigned int size, const char* hint);

/**
 * @brief Loads animations from an existing importer.
 * @param importer Importer instance containing animation data.
 * @return Pointer to an array of R3D_Animation, or NULL on failure.
 * @note Free the returned array using R3D_UnloadAnimationLib().
 */
R3DAPI R3D_AnimationLib R3D_LoadAnimationLibFromImporter(const R3D_Importer* importer);

/**
 * @brief Releases all resources associated with an animation library.
 * @param animLib Animation library to unload.
 */
R3DAPI void R3D_UnloadAnimationLib(R3D_AnimationLib animLib);

/**
 * @brief Returns the index of an animation by name.
 * @param animLib Animation library to search.
 * @param name Name of the animation (case-sensitive).
 * @return Zero-based index if found, or -1 if not found.
 */
R3DAPI int R3D_GetAnimationIndex(R3D_AnimationLib animLib, const char* name);

/**
 * @brief Retrieves an animation by name.
 * @param animLib Animation library to search.
 * @param name Name of the animation (case-sensitive).
 * @return Pointer to the animation, or NULL if not found.
 */
R3DAPI R3D_Animation* R3D_GetAnimation(R3D_AnimationLib animLib, const char* name);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Animation

#endif // R3D_ANIMATION_H
