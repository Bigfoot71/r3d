/* r3d_animation.h -- R3D Animation Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ANIMATION_H
#define R3D_ANIMATION_H

#include "./r3d_skeleton.h"
#include <raylib.h>

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

/**
 * @brief Describes the playback state of a single animation.
 *
 * Each state tracks the current playback time, blending weight,
 * and looping behavior for one animation within a player.
 */
typedef struct R3D_AnimationState {
    float currentTime;  ///< Current playback time in animation ticks.
    float weight;       ///< Blending weight of this animation (0.0-1.0).
    bool loop;          ///< True to enable looping playback.
} R3D_AnimationState;

/**
 * @brief Controls playback and blending of animations for a skeleton.
 *
 * The animation player manages multiple animation states from a given
 * animation library and computes the blended pose for the associated skeleton.
 * On each update, it advances internal timers, interpolates keyframes,
 * blends active animations according to their weights, and updates the
 * current skeleton pose.
 */
typedef struct R3D_AnimationPlayer {
    const R3D_AnimationLib* animLib;    ///< Animation library providing available animations.
    const R3D_Skeleton* skeleton;       ///< Target skeleton to animate.
    R3D_AnimationState* states;         ///< Array of active animation states.
    Matrix* currentPose;                ///< Array of bone transforms representing the blended pose.
} R3D_AnimationPlayer;

// ========================================
// PUBLIC API
// ========================================

// ----------------------------------------
// ANIMATION: Animation Library Functions
// ----------------------------------------

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
 * @brief Frees memory allocated for model animations.
 * @param animLib Pointer to the animation library to free.
 */
R3DAPI void R3D_UnloadAnimationLib(R3D_AnimationLib* animLib);

/**
 * @brief Retrieves the index of a named animation within an animation library.
 * @param animLib Pointer to the animation library.
 * @param name Name of the animation to look for (case-sensitive).
 * @return Zero-based index of the matching animation, or -1 if not found.
 */
R3DAPI int R3D_GetAnimationIndex(const R3D_AnimationLib* animLib, const char* name);

/**
 * @brief Finds a named animation in an array of animations.
 * @param animLib Pointer to the animation library.
 * @param name Name of the animation to find (case-sensitive).
 * @return Pointer to the matching animation, or NULL if not found.
 */
R3DAPI R3D_Animation* R3D_GetAnimation(const R3D_AnimationLib* animLib, const char* name);

// ----------------------------------------
// ANIMATION: Animation Player Functions
// ----------------------------------------

/**
 * @brief Creates a new animation player for a skeleton and animation library.
 *
 * Allocates internal structures for managing animation states and poses.
 *
 * @param skeleton Pointer to the target skeleton.
 * @param animLib Pointer to the animation library containing available animations.
 * @return Pointer to a newly created animation player, or NULL on failure.
 */
R3DAPI R3D_AnimationPlayer* R3D_LoadAnimationPlayer(const R3D_Skeleton* skeleton, const R3D_AnimationLib* animLib);

/**
 * @brief Destroys an animation player and frees its allocated resources.
 *
 * @param player Pointer to the animation player to destroy.
 */
R3DAPI void R3D_UnloadAnimationPlayer(R3D_AnimationPlayer* player);

/**
 * @brief Updates the animation player by advancing time and blending animations.
 *
 * This function interpolates keyframes, blends all active animation states,
 * and updates the current skeleton pose. The time step `dt` can be scaled
 * to modify playback speed.
 *
 * @param player Pointer to the animation player.
 * @param dt Delta time since the last update, in seconds.
 */
R3DAPI void R3D_UpdateAnimationPlayer(R3D_AnimationPlayer* player, float dt);

/** @} */ // end of Animation

#endif // R3D_ANIMATION_H
