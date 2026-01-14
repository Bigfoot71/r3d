/* r3d_animation_player.h -- R3D Animation Player Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_ANIMATION_PLAYER_H
#define R3D_ANIMATION_PLAYER_H

#include "./r3d_animation.h"
#include "./r3d_platform.h"
#include "./r3d_skeleton.h"
#include <raylib.h>
#include <stdint.h>

/**
 * @defgroup AnimationPlayer
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Types of events that an animation player can emit.
 */
typedef enum {
    R3D_ANIM_EVENT_FINISHED, ///< Animation has finished playing (non-looping).
    R3D_ANIM_EVENT_LOOPED,   ///< Animation has completed a loop.
} R3D_AnimationEvent;

// ========================================
// CALLBACKS TYPES
// ========================================

struct R3D_AnimationPlayer;

/**
 * @brief Callback type for receiving animation events.
 *
 * @param player Pointer to the animation player emitting the event.
 * @param eventType Type of the event (finished, looped).
 * @param animIndex Index of the animation triggering the event.
 * @param userData Optional user-defined data passed when the callback was registered.
 */
typedef void (*R3D_AnimationEventCallback)(
    struct R3D_AnimationPlayer* player,
    R3D_AnimationEvent eventType,
    int animIndex, void* userData
);

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Describes the playback state of a single animation within a player.
 *
 * Tracks the current time, blending weight, speed, play/pause state, and looping behavior.
 */
typedef struct R3D_AnimationState {
    float currentTime;  ///< Current playback time in animation ticks.
    float weight;       ///< Blending weight; any positive value is valid.
    float speed;        ///< Playback speed; can be negative for reverse playback.
    bool play;          ///< Whether the animation is currently playing.
    bool loop;          ///< True to enable looping playback.
} R3D_AnimationState;

/**
 * @brief Manages playback and blending of multiple animations for a skeleton.
 *
 * The animation player updates animation states, interpolates keyframes,
 * blends animations according to their weights, and stores the resulting
 * local and global bone transforms. It also supports GPU skinning by uploading
 * the global pose into a texture.
 */
typedef struct R3D_AnimationPlayer {

    R3D_AnimationState* states; ///< Array of active animation states, one per animation.
    R3D_AnimationLib animLib;   ///< Animation library providing the available animations.
    R3D_Skeleton skeleton;      ///< Skeleton to animate.

    Matrix* localPose;          ///< Array of bone transforms representing the blended local pose.
    Matrix* globalPose;         ///< Array of bone transforms multiplied by bone offsets for GPU skinning.
    uint32_t texGlobalPose;     ///< GPU texture ID storing the global pose as a 1D RGBA32F texture.

    R3D_AnimationEventCallback eventCallback;   ///< Callback function to receive animation events.
    void* eventUserData;                        ///< Optional user data pointer passed to the callback.

} R3D_AnimationPlayer;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates an animation player for a skeleton and animation library.
 *
 * Allocates memory for animation states and pose buffers.
 *
 * @param skeleton Skeleton to animate.
 * @param animLib Animation library providing animations.
 * @return Newly created animation player, or a zeroed struct on failure.
 */
R3DAPI R3D_AnimationPlayer R3D_LoadAnimationPlayer(R3D_Skeleton skeleton, R3D_AnimationLib animLib);

/**
 * @brief Releases all resources used by an animation player.
 *
 * @param player Animation player to unload.
 */
R3DAPI void R3D_UnloadAnimationPlayer(R3D_AnimationPlayer player);

/**
 * @brief Checks whether an animation player is valid.
 *
 * @param player Animation player to check.
 * @return true if valid, false otherwise.
 */
R3DAPI bool R3D_IsAnimationPlayerValid(R3D_AnimationPlayer player);

/**
 * @brief Returns whether a given animation is currently playing.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @return true if playing, false otherwise.
 */
R3DAPI bool R3D_IsAnimationPlaying(R3D_AnimationPlayer player, int animIndex);

/**
 * @brief Starts playback of the specified animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation to play.
 */
R3DAPI void R3D_PlayAnimation(R3D_AnimationPlayer* player, int animIndex);

/**
 * @brief Pauses the specified animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation to pause.
 */
R3DAPI void R3D_PauseAnimation(R3D_AnimationPlayer* player, int animIndex);

/**
 * @brief Stops the specified animation and clamps its time.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation to stop.
 */
R3DAPI void R3D_StopAnimation(R3D_AnimationPlayer* player, int animIndex);

/**
 * @brief Rewinds the animation to the start or end depending on playback direction.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation to rewind.
 */
R3DAPI void R3D_RewindAnimation(R3D_AnimationPlayer* player, int animIndex);

/**
 * @brief Gets the current playback time of an animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @return Current time in animation ticks.
 */
R3DAPI float R3D_GetAnimationTime(R3D_AnimationPlayer player, int animIndex);

/**
 * @brief Sets the current playback time of an animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @param time Time in animation ticks.
 */
R3DAPI void R3D_SetAnimationTime(R3D_AnimationPlayer* player, int animIndex, float time);

/**
 * @brief Gets the blending weight of an animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @return Current weight.
 */
R3DAPI float R3D_GetAnimationWeight(R3D_AnimationPlayer player, int animIndex);

/**
 * @brief Sets the blending weight of an animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @param weight Blending weight to apply.
 */
R3DAPI void R3D_SetAnimationWeight(R3D_AnimationPlayer* player, int animIndex, float weight);

/**
 * @brief Gets the playback speed of an animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @return Current speed (may be negative for reverse playback).
 */
R3DAPI float R3D_GetAnimationSpeed(R3D_AnimationPlayer player, int animIndex);

/**
 * @brief Sets the playback speed of an animation.
 *
 * Negative values play the animation backwards. If setting a negative speed
 * on a stopped animation, consider calling RewindAnimation() to start at the end.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @param speed Playback speed.
 */
R3DAPI void R3D_SetAnimationSpeed(R3D_AnimationPlayer* player, int animIndex, float speed);

/**
 * @brief Gets whether the animation is set to loop.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @return True if looping is enabled.
 */
R3DAPI bool R3D_GetAnimationLoop(R3D_AnimationPlayer player, int animIndex);

/**
 * @brief Enables or disables looping for the animation.
 *
 * @param player Animation player.
 * @param animIndex Index of the animation.
 * @param loop True to enable looping.
 */
R3DAPI void R3D_SetAnimationLoop(R3D_AnimationPlayer* player, int animIndex, bool loop);

/**
 * @brief Advances the time of all active animations.
 *
 * Updates all internal animation timers based on speed and delta time.
 * Does NOT recalculate the skeleton pose.
 *
 * @param player Animation player.
 * @param dt Delta time in seconds.
 */
R3DAPI void R3D_AdvanceAnimationPlayerTime(R3D_AnimationPlayer* player, float dt);

/**
 * @brief Calculates the current blended skeleton pose.
 *
 * Interpolates keyframes and blends all active animations according to their weights.
 * Does NOT advance animation time.
 *
 * @param player Animation player.
 */
R3DAPI void R3D_CalculateAnimationPlayerPose(R3D_AnimationPlayer* player);

/**
 * @brief Uploads the current global pose array to the internal GPU texture.
 *
 * @param player Animation player.
 */
R3DAPI void R3D_UploadAnimationPlayerPose(R3D_AnimationPlayer* player);

/**
 * @brief Updates the animation player: calculates and upload blended pose, then advances time.
 *
 * Equivalent to calling R3D_CalculateAnimationPlayerPose() followed by
 * R3D_UploadAnimationPlayerPose() and R3D_AdvanceAnimationPlayerTime().
 *
 * @param player Animation player.
 * @param dt Delta time in seconds.
 */
R3DAPI void R3D_UpdateAnimationPlayer(R3D_AnimationPlayer* player, float dt);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of AnimationPlayer

#endif // R3D_ANIMATION_PLAYER_H
