/* r3d_animation.h -- R3D Animation Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation.h>
#include <raymath.h>
#include <string.h>

#include "./details/importer/r3d_importer.h"
#include "./details/r3d_math.h"

// ========================================
// PUBLIC API
// ========================================

// ----------------------------------------
// ANIMATION: Animation Library Functions
// ----------------------------------------

R3D_AnimationLib R3D_LoadAnimationLib(const char* filePath)
{
    R3D_AnimationLib animLib = { 0 };

    r3d_importer_t importer = { 0 };
    if (!r3d_importer_create_from_file(&importer, filePath)) {
        return animLib;
    }

    r3d_importer_load_animations(&importer, &animLib);
    r3d_importer_destroy(&importer);

    return animLib;
}

R3D_AnimationLib R3D_LoadAnimationLibFromMemory(const void* data, unsigned int size, const char* hint)
{
    R3D_AnimationLib animLib = { 0 };

    r3d_importer_t importer = { 0 };
    if (!r3d_importer_create_from_memory(&importer, data, size, hint)) {
        return animLib;
    }

    r3d_importer_load_animations(&importer, &animLib);
    r3d_importer_destroy(&importer);

    return animLib;
}

void R3D_UnloadAnimationLib(R3D_AnimationLib* animLib)
{
    if (!animLib || !animLib->animations)
        return;

    for (int i = 0; i < animLib->count; ++i) {
        R3D_Animation* anim = &animLib->animations[i];

        for (int j = 0; j < anim->channelCount; ++j) {
            R3D_AnimationChannel* channel = &anim->channels[j];

            RL_FREE((void*)channel->translation.times);
            RL_FREE((void*)channel->translation.values);

            RL_FREE((void*)channel->rotation.times);
            RL_FREE((void*)channel->rotation.values);

            RL_FREE((void*)channel->scale.times);
            RL_FREE((void*)channel->scale.values);
        }

        RL_FREE(anim->channels);
    }

    RL_FREE(animLib->animations);
}

int R3D_GetAnimationIndex(const R3D_AnimationLib* animLib, const char* name)
{
    for (int i = 0; i < animLib->count; i++) {
        if (strcmp(animLib->animations[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

R3D_Animation* R3D_GetAnimation(const R3D_AnimationLib* animLib, const char* name)
{
    int index = R3D_GetAnimationIndex(animLib, name);
    if (index < 0) return NULL;

    return &animLib->animations[index];
}

// ----------------------------------------
// ANIMATION: Animation Player Functions
// ----------------------------------------

R3D_AnimationPlayer* R3D_LoadAnimationPlayer(const R3D_Skeleton* skeleton, const R3D_AnimationLib* animLib)
{
    R3D_AnimationPlayer* player = RL_MALLOC(sizeof(R3D_AnimationPlayer));

    player->skeleton = *skeleton;
    player->animLib = *animLib;

    player->states = RL_CALLOC(animLib->count, sizeof(R3D_AnimationState));
    player->currentPose = RL_CALLOC(skeleton->boneCount, sizeof(Matrix));

    return player;
}

void R3D_UnloadAnimationPlayer(R3D_AnimationPlayer* player)
{
    RL_FREE(player->currentPose);
    RL_FREE(player->states);
}

void R3D_AdvanceAnimationPlayerTime(R3D_AnimationPlayer* player, float dt)
{
    const int animCount = player->animLib.count;
    R3D_AnimationState* states = player->states;

    for (int iAnim = 0; iAnim < animCount; iAnim++)
    {
        const R3D_Animation* anim = &player->animLib.animations[iAnim];
        R3D_AnimationState* state = &states[iAnim];

        state->currentTime += dt;

        float durationInSeconds = anim->duration / anim->ticksPerSecond;

        if (state->currentTime >= durationInSeconds) {
            state->currentTime = state->loop
                ? fmodf(state->currentTime, durationInSeconds)
                : durationInSeconds;
        }
    }
}

static void compute_pose(R3D_AnimationPlayer* player, float totalWeight);

void R3D_CalculateAnimationPlayerPose(R3D_AnimationPlayer* player)
{
    const int boneCount = player->skeleton.boneCount;
    const int animCount = player->animLib.count;

    R3D_AnimationState* states = player->states;
    Matrix* pose = player->currentPose;

    float totalWeight = 0.0f;
    for (int iAnim = 0; iAnim < animCount; iAnim++) {
        totalWeight += states[iAnim].weight;
    }

    if (totalWeight <= 0.0f) {
        memcpy(pose, player->skeleton.bindPose, boneCount * sizeof(Matrix));
    }
    else {
        compute_pose(player, totalWeight);
    }
}

void R3D_UpdateAnimationPlayer(R3D_AnimationPlayer* player, float dt)
{
    R3D_CalculateAnimationPlayerPose(player);
    R3D_AdvanceAnimationPlayerTime(player, dt);
}

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

static void find_key_frames(
    const float* times, uint32_t count, float time,
    uint32_t* outIdx0, uint32_t* outIdx1, float* outT)
{
    // No keys
    if (count == 0) {
        *outIdx0 = *outIdx1 = 0;
        *outT = 0.0f;
        return;
    }

    // Single key or before first
    if (count == 1 || time <= times[0]) {
        *outIdx0 = *outIdx1 = 0;
        *outT = 0.0f;
        return;
    }

    // After last
    if (time >= times[count - 1]) {
        *outIdx0 = *outIdx1 = count - 1;
        *outT = 0.0f;
        return;
    }

    // Binary search
    uint32_t left = 0;
    uint32_t right = count - 1;

    while (right - left > 1) {
        uint32_t mid = (left + right) >> 1;
        if (times[mid] <= time) left = mid;
        else right = mid;
    }

    *outIdx0 = left;
    *outIdx1 = right;

    const float t0 = times[left];
    const float t1 = times[right];
    const float dt = t1 - t0;

    *outT = (dt > 0.0f) ? (time - t0) / dt : 0.0f;
}

static Transform interpolate_channel(const R3D_AnimationChannel* channel, float time)
{
    Transform result = {
        .translation = { 0.0f, 0.0f, 0.0f },
        .rotation    = { 0.0f, 0.0f, 0.0f, 1.0f },
        .scale       = { 1.0f, 1.0f, 1.0f }
    };

    // Translation
    if (channel->translation.count > 0) {
        uint32_t i0, i1;
        float t;

        find_key_frames(
            channel->translation.times,
            channel->translation.count,
            time,
            &i0, &i1, &t
        );

        const Vector3* values = (const Vector3*)channel->translation.values;
        result.translation = Vector3Lerp(values[i0], values[i1], t);
    }

    // Rotation
    if (channel->rotation.count > 0) {
        uint32_t i0, i1;
        float t;

        find_key_frames(
            channel->rotation.times,
            channel->rotation.count,
            time,
            &i0, &i1, &t
        );

        const Quaternion* values = (const Quaternion*)channel->rotation.values;
        result.rotation = QuaternionSlerp(values[i0], values[i1], t);
    }

    // Scale
    if (channel->scale.count > 0) {
        uint32_t i0, i1;
        float t;

        find_key_frames(
            channel->scale.times,
            channel->scale.count,
            time,
            &i0, &i1, &t
        );

        const Vector3* values = (const Vector3*)channel->scale.values;
        result.scale = Vector3Lerp(values[i0], values[i1], t);
    }

    return result;
}

static const R3D_AnimationChannel* find_channel_for_bone(const R3D_Animation* anim, int iBone)
{
    for (int i = 0; i < anim->channelCount; i++) {
        if (anim->channels[i].boneIndex == iBone) {
            return &anim->channels[i];
        }
    }
    return NULL;
}

void compute_pose(R3D_AnimationPlayer* player, float totalWeight)
{
    const int boneCount = player->skeleton.boneCount;
    const int animCount = player->animLib.count;
    R3D_AnimationState* states = player->states;

    for (int iBone = 0; iBone < boneCount; iBone++)
    {
        Transform blended = {0};
        bool isAnimated = false;

        for (int iAnim = 0; iAnim < animCount; iAnim++)
        {
            const R3D_Animation* anim = &player->animLib.animations[iAnim];
            const R3D_AnimationState* state = &states[iAnim];
            if (state->weight <= 0.0f) continue;

            const R3D_AnimationChannel* channel = find_channel_for_bone(anim, iBone);
            if (!channel) continue;
            isAnimated = true;

            Transform local = interpolate_channel(channel, state->currentTime * anim->ticksPerSecond);
            float w = state->weight / totalWeight;

            blended.translation = Vector3Scale(Vector3Add(blended.translation, local.translation), w);
            blended.rotation = QuaternionScale(QuaternionAdd(blended.rotation, local.rotation), w);
            blended.scale = Vector3Scale(Vector3Add(blended.scale, local.scale), w);
        }

        if (isAnimated) {
            blended.rotation = QuaternionNormalize(blended.rotation);
            player->currentPose[iBone] = r3d_matrix_scale_rotq_translate(blended.translation, blended.rotation, blended.scale);
        }
        else {
            player->currentPose[iBone] = player->skeleton.bindLocal[iBone];
        }

        int parentIdx = player->skeleton.bones[iBone].parent;
        if (parentIdx >= 0) {
            player->currentPose[iBone] = r3d_matrix_multiply(&player->currentPose[iBone], &player->currentPose[parentIdx]);
        }
        else {
            Matrix invLocalBind = MatrixInvert(player->skeleton.bindLocal[iBone]);
            Matrix parentGlobalScene = r3d_matrix_multiply(&invLocalBind, &player->skeleton.bindPose[iBone]);
            player->currentPose[iBone] = r3d_matrix_multiply(&player->currentPose[iBone], &parentGlobalScene);
        }
    }
}
