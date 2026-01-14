/* r3d_animation_player.c -- R3D Animation Player Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation_player.h>
#include <raymath.h>
#include <glad.h>

#include "./common/r3d_math.h"

// ========================================
// INTERNAL FUNCTIONS DECLARATIONS
// ========================================

static void compute_pose(R3D_AnimationPlayer* player, float totalWeight);
static void emit_event(R3D_AnimationPlayer* player, R3D_AnimationEvent event, int animIndex);

// ========================================
// PUBLIC API
// ========================================

R3D_AnimationPlayer R3D_LoadAnimationPlayer(R3D_Skeleton skeleton, R3D_AnimationLib animLib)
{
    R3D_AnimationPlayer player = {0};

    player.skeleton = skeleton;
    player.animLib = animLib;

    player.states = RL_MALLOC(animLib.count * sizeof(*player.states));
    player.localPose = RL_CALLOC(skeleton.boneCount, sizeof(*player.localPose));
    player.globalPose = RL_CALLOC(skeleton.boneCount, sizeof(*player.globalPose));

    for (int i = 0; i < animLib.count; i++) {
        player.states[i] = (R3D_AnimationState) {
            .currentTime = 0.0f,
            .weight = 0.0f,
            .speed = 1.0f,
            .play = false,
            .loop = false
        };
    }

    glGenTextures(1, &player.texGlobalPose);
    glBindTexture(GL_TEXTURE_1D, player.texGlobalPose);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, 4 * skeleton.boneCount, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_1D, 0);

    return player;
}

void R3D_UnloadAnimationPlayer(R3D_AnimationPlayer player)
{
    if (player.texGlobalPose > 0) {
        glDeleteTextures(1, &player.texGlobalPose);
    }

    RL_FREE(player.localPose);
    RL_FREE(player.globalPose);
    RL_FREE(player.states);
}

bool R3D_IsAnimationPlayerValid(R3D_AnimationPlayer player)
{
    return (player.texGlobalPose > 0);
}

bool R3D_IsAnimationPlaying(R3D_AnimationPlayer player, int animIndex)
{
    return player.states[animIndex].play;
}

void R3D_PlayAnimation(R3D_AnimationPlayer* player, int animIndex)
{
    player->states[animIndex].play = true;
}

void R3D_PauseAnimation(R3D_AnimationPlayer* player, int animIndex)
{
    player->states[animIndex].play = false;
}

void R3D_StopAnimation(R3D_AnimationPlayer* player, int animIndex)
{
    R3D_AnimationState* state = &player->states[animIndex];

    if (state->speed >= 0.0f) {
        state->currentTime = 0.0f;
    }
    else {
        const R3D_Animation* anim = &player->animLib.animations[animIndex];
        state->currentTime = anim->duration / anim->ticksPerSecond;
    }

    state->play = false;
}

void R3D_RewindAnimation(R3D_AnimationPlayer* player, int animIndex)
{
    R3D_AnimationState* state = &player->states[animIndex];

    if (state->speed >= 0.0f) {
        state->currentTime = 0.0f;
    }
    else {
        const R3D_Animation* anim = &player->animLib.animations[animIndex];
        state->currentTime = anim->duration / anim->ticksPerSecond;
    }
}

float R3D_GetAnimationTime(R3D_AnimationPlayer player, int animIndex)
{
    return player.states[animIndex].currentTime;
}

void R3D_SetAnimationTime(R3D_AnimationPlayer* player, int animIndex, float time)
{
    const R3D_Animation* anim = &player->animLib.animations[animIndex];
    float duration = anim->duration / anim->ticksPerSecond;

    player->states[animIndex].currentTime = Wrap(time, 0.0f, duration);
}

float R3D_GetAnimationWeight(R3D_AnimationPlayer player, int animIndex)
{
    return player.states[animIndex].weight;
}

void R3D_SetAnimationWeight(R3D_AnimationPlayer* player, int animIndex, float weight)
{
    player->states[animIndex].weight = weight;
}

float R3D_GetAnimationSpeed(R3D_AnimationPlayer player, int animIndex)
{
    return player.states[animIndex].speed;
}

void R3D_SetAnimationSpeed(R3D_AnimationPlayer* player, int animIndex, float speed)
{
    player->states[animIndex].speed = speed;
}

bool R3D_GetAnimationLoop(R3D_AnimationPlayer player, int animIndex)
{
    return player.states[animIndex].loop;
}

void R3D_SetAnimationLoop(R3D_AnimationPlayer* player, int animIndex, bool loop)
{
    player->states[animIndex].loop = loop;
}

void R3D_AdvanceAnimationPlayerTime(R3D_AnimationPlayer* player, float dt)
{
    const int animCount = player->animLib.count;
    R3D_AnimationState* states = player->states;

    for (int i = 0; i < animCount; i++)
    {
        R3D_AnimationState* s = &states[i];
        if (!s->play) continue;

        const R3D_Animation* anim = &player->animLib.animations[i];
        float duration = anim->duration / anim->ticksPerSecond;

        s->currentTime += s->speed * dt;

        if ((s->speed > 0.0f && s->currentTime >= duration) || (s->speed < 0.0f && s->currentTime <= 0.0f)) {
            if ((s->play = s->loop)) {
                emit_event(player, R3D_ANIM_EVENT_LOOPED, i);
                s->currentTime -= copysignf(duration, s->speed);
            }
            else {
                emit_event(player, R3D_ANIM_EVENT_FINISHED, i);
                s->currentTime = Clamp(s->currentTime, 0.0f, duration);
            }
        }
    }
}

void R3D_CalculateAnimationPlayerPose(R3D_AnimationPlayer* player)
{
    const int boneCount = player->skeleton.boneCount;
    const int animCount = player->animLib.count;

    R3D_AnimationState* states = player->states;

    float totalWeight = 0.0f;
    for (int iAnim = 0; iAnim < animCount; iAnim++) {
        totalWeight += states[iAnim].weight;
    }

    if (totalWeight > 0.0f) {
        compute_pose(player, totalWeight);
    }
    else {
        memcpy(player->localPose, player->skeleton.bindLocal, boneCount * sizeof(Matrix));
        memcpy(player->globalPose, player->skeleton.bindPose, boneCount * sizeof(Matrix));
    }
}

void R3D_UploadAnimationPlayerPose(R3D_AnimationPlayer* player)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, player->texGlobalPose);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4 * player->skeleton.boneCount, GL_RGBA, GL_FLOAT, player->globalPose);
    glBindTexture(GL_TEXTURE_1D, 0);
}

void R3D_UpdateAnimationPlayer(R3D_AnimationPlayer* player, float dt)
{
    R3D_CalculateAnimationPlayerPose(player);
    R3D_UploadAnimationPlayerPose(player);

    R3D_AdvanceAnimationPlayerTime(player, dt);
}

// ========================================
// INTERNAL FUNCTIONS DEFINITIONS
// ========================================

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

            blended.translation = Vector3Add(blended.translation, Vector3Scale(local.translation, w));
            blended.rotation = QuaternionAdd(blended.rotation, QuaternionScale(local.rotation, w));
            blended.scale = Vector3Add(blended.scale, Vector3Scale(local.scale, w));
        }

        if (isAnimated) {
            blended.rotation = QuaternionNormalize(blended.rotation);
            player->localPose[iBone] = r3d_matrix_scale_rotq_translate(blended.scale, blended.rotation, blended.translation);
        }
        else {
            player->localPose[iBone] = player->skeleton.bindLocal[iBone];
        }

        int parentIdx = player->skeleton.bones[iBone].parent;
        if (parentIdx >= 0) {
            player->localPose[iBone] = r3d_matrix_multiply(&player->localPose[iBone], &player->localPose[parentIdx]);
        }
        else {
            Matrix invLocalBind = MatrixInvert(player->skeleton.bindLocal[iBone]);
            Matrix parentGlobalScene = r3d_matrix_multiply(&invLocalBind, &player->skeleton.bindPose[iBone]);
            player->localPose[iBone] = r3d_matrix_multiply(&player->localPose[iBone], &parentGlobalScene);
        }

        player->globalPose[iBone] = r3d_matrix_multiply(&player->skeleton.boneOffsets[iBone], &player->localPose[iBone]);
    }
}

void emit_event(R3D_AnimationPlayer* player, R3D_AnimationEvent event, int animIndex)
{
    if (player->eventCallback != NULL) {
        player->eventCallback(player, event, animIndex, player->eventUserData);
    }
}
