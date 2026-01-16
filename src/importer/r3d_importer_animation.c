/* r3d_importer_animation.c -- Module to import animations from assimp scene.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_importer.h"

#include <assimp/anim.h>
#include <r3d_config.h>
#include <raylib.h>
#include <string.h>

// ========================================
// CHANNEL LOADING (INTERNAL)
// ========================================

static bool load_vector3_track(R3D_AnimationTrack* track, unsigned int count, const struct aiVectorKey* keys)
{
    track->count = (int)count;
    track->times = NULL;
    track->values = NULL;

    if (track->count == 0)
        return true;

    float* times = RL_MALLOC(sizeof(float) * track->count);
    Vector3* values = RL_MALLOC(sizeof(Vector3) * track->count);

    if (!times || !values) {
        RL_FREE(times);
        RL_FREE(values);
        return false;
    }

    for (int i = 0; i < track->count; ++i) {
        times[i] = (float)keys[i].mTime;
        values[i] = r3d_importer_cast(keys[i].mValue);
    }

    track->times = times;
    track->values = values;
    return true;
}

static bool load_quaternion_track(R3D_AnimationTrack* track, unsigned int count, const struct aiQuatKey* keys)
{
    track->count = (int)count;
    track->times = NULL;
    track->values = NULL;

    if (track->count == 0)
        return true;

    float* times = RL_MALLOC(sizeof(float) * track->count);
    Quaternion* values = RL_MALLOC(sizeof(Quaternion) * track->count);

    if (!times || !values) {
        RL_FREE(times);
        RL_FREE(values);
        return false;
    }

    for (int i = 0; i < track->count; ++i) {
        times[i] = (float)keys[i].mTime;
        values[i] = r3d_importer_cast(keys[i].mValue);
    }

    track->times = times;
    track->values = values;
    return true;
}

static bool load_channel(R3D_AnimationChannel* channel, const r3d_importer_t* importer, const struct aiNodeAnim* aiChannel)
{
    if (!aiChannel) {
        R3D_TRACELOG(LOG_ERROR, "Invalid animation channel");
        return false;
    }

    const char* boneName = aiChannel->mNodeName.data;
    channel->boneIndex = r3d_importer_get_bone_index(importer, boneName);

    if (channel->boneIndex < 0) {
        R3D_TRACELOG(LOG_WARNING,
                 "Bone '%s' from animation not found in skeleton",
                 boneName);
        return false;
    }

    if (!load_vector3_track(&channel->translation, aiChannel->mNumPositionKeys, aiChannel->mPositionKeys)) {
        goto fail;
    }

    if (!load_quaternion_track(&channel->rotation, aiChannel->mNumRotationKeys, aiChannel->mRotationKeys)) {
        goto fail;
    }

    if (!load_vector3_track(&channel->scale, aiChannel->mNumScalingKeys, aiChannel->mScalingKeys)) {
        goto fail;
    }

    return true;

fail:
    RL_FREE((void*)channel->translation.times);
    RL_FREE((void*)channel->translation.values);
    RL_FREE((void*)channel->rotation.times);
    RL_FREE((void*)channel->rotation.values);
    RL_FREE((void*)channel->scale.times);
    RL_FREE((void*)channel->scale.values);
    return false;
}


// ========================================
// ANIMATION LOADING (INTERNAL)
// ========================================

static bool load_animation(R3D_Animation* animation, const r3d_importer_t* importer, const struct aiAnimation* aiAnim)
{
    // Basic validation
    if (!aiAnim || aiAnim->mNumChannels == 0) {
        R3D_TRACELOG(LOG_ERROR, "Invalid animation or no channels");
        return false;
    }

    // Check that we have bones in the skeleton
    const int boneCount = r3d_importer_get_bone_count(importer);
    if (boneCount == 0) {
        R3D_TRACELOG(LOG_ERROR, "No bones in skeleton");
        return false;
    }

    // Initialize animation structure
    animation->boneCount = boneCount;
    animation->duration = (float)aiAnim->mDuration;
    animation->ticksPerSecond = (aiAnim->mTicksPerSecond != 0.0) 
        ? (float)aiAnim->mTicksPerSecond 
        : 24.0f;

    // Copy animation name
    size_t nameLen = strlen(aiAnim->mName.data);
    if (nameLen >= sizeof(animation->name)) {
        nameLen = sizeof(animation->name) - 1;
    }
    memcpy(animation->name, aiAnim->mName.data, nameLen);
    animation->name[nameLen] = '\0';

    // Allocate channels
    animation->channelCount = aiAnim->mNumChannels;
    animation->channels = RL_CALLOC(animation->channelCount, sizeof(R3D_AnimationChannel));
    if (!animation->channels) {
        R3D_TRACELOG(LOG_ERROR, "Failed to allocate animation channels");
        return false;
    }

    // Load each channel
    int successChannels = 0;
    for (unsigned int i = 0; i < aiAnim->mNumChannels; i++) {
        if (load_channel(&animation->channels[successChannels], importer, aiAnim->mChannels[i])) {
            successChannels++;
        } else {
            R3D_TRACELOG(LOG_WARNING, "Failed to load channel %u", i);
        }
    }

    if (successChannels == 0) {
        R3D_TRACELOG(LOG_ERROR, "No channels were successfully loaded");
        RL_FREE(animation->channels);
        return false;
    }

    // Adjust channel count if some failed
    if (successChannels < animation->channelCount) {
        animation->channelCount = successChannels;
        R3D_AnimationChannel* resized = RL_REALLOC(
            animation->channels,
            successChannels * sizeof(R3D_AnimationChannel)
        );
        if (resized) {
            animation->channels = resized;
        }
    }

    R3D_TRACELOG(LOG_INFO, "Animation '%s' loaded: %.2f duration, %.2f ticks/sec, %d channels",
             animation->name, animation->duration, animation->ticksPerSecond, animation->channelCount);

    return true;
}

// ========================================
// PUBLIC FUNCTIONS
// ========================================

bool r3d_importer_load_animations(const r3d_importer_t* importer, R3D_AnimationLib* animLib)
{
    if (!importer || !r3d_importer_is_valid(importer)) {
        R3D_TRACELOG(LOG_ERROR, "Invalid importer for animation loading");
        return false;
    }

    int animCount = r3d_importer_get_animation_count(importer);
    if (animCount == 0) {
        R3D_TRACELOG(LOG_WARNING, "No animations found in the imported scene");
        return false;
    }

    // Allocate temporary animations array
    R3D_Animation* animations = RL_CALLOC(animCount, sizeof(R3D_Animation));
    if (!animations) {
        R3D_TRACELOG(LOG_ERROR, "Unable to allocate memory for animations");
        return false;
    }

    // Load each animation
    int successCount = 0;
    for (int i = 0; i < animCount; i++) {
        const struct aiAnimation* aiAnim = r3d_importer_get_animation(importer, i);
        if (load_animation(&animations[successCount], importer, aiAnim)) {
            successCount++;
        } else {
            R3D_TRACELOG(LOG_ERROR, "Failed to process animation %d", i);
        }
    }

    if (successCount == 0) {
        R3D_TRACELOG(LOG_ERROR, "No animations were successfully loaded");
        RL_FREE(animations);
        return false;
    }

    // Resize if some animations failed
    if (successCount < animCount) {
        R3D_TRACELOG(LOG_WARNING, "Only %d out of %d animations were successfully loaded", successCount, animCount);
        R3D_Animation* resized = RL_REALLOC(animations, successCount * sizeof(R3D_Animation));
        if (resized) {
            animations = resized;
        }
    }

    animLib->animations = animations;
    animLib->count = successCount;

    R3D_TRACELOG(LOG_INFO, "Loaded animation library with %d animations", successCount);

    return true;
}
