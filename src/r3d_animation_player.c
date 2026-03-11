/* r3d_animation_player.c -- R3D Animation Player Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation_player.h>
#include <r3d_config.h>
#include <raymath.h>
#include <stdlib.h>
#include <assert.h>
#include <glad.h>

#include "./common/r3d_anim.h"

// ========================================
// INTERNAL FUNCTIONS DECLARATIONS
// ========================================

static void emit_event(R3D_AnimationPlayer* player, R3D_AnimationEvent event, int animIndex);
static bool is_anim_index_valid(R3D_AnimationPlayer* player, int animIndex);
static void reset_anim_time(R3D_AnimationPlayer* player, int animIndex);
static void compute_local_matrices(R3D_AnimationPlayer* player);

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
    player.modelPose = RL_CALLOC(skeleton.boneCount, sizeof(*player.modelPose));
    player.skinBuffer = RL_CALLOC(skeleton.boneCount, sizeof(*player.skinBuffer));

    for (int i = 0; i < animLib.count; i++) {
        player.states[i] = (R3D_AnimationState) {
            .currentTime = 0.0f,
            .speed = 1.0f,
            .play = false,
            .loop = false
        };
    }
    player.activeAnimIndex = -1;

    glGenTextures(1, &player.skinTexture);
    glBindTexture(GL_TEXTURE_1D, player.skinTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, 4 * skeleton.boneCount, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_1D, 0);

    return player;
}

void R3D_UnloadAnimationPlayer(R3D_AnimationPlayer player)
{
    if (player.skinTexture > 0) {
        glDeleteTextures(1, &player.skinTexture);
    }

    RL_FREE(player.skinBuffer);
    RL_FREE(player.modelPose);
    RL_FREE(player.localPose);
    RL_FREE(player.states);
}

bool R3D_IsAnimationPlayerValid(R3D_AnimationPlayer player)
{
    return (player.skinTexture > 0);
}

bool R3D_IsAnimationPlaying(R3D_AnimationPlayer player)
{
    if (is_anim_index_valid(&player, player.activeAnimIndex)) {
        return player.states[player.activeAnimIndex].play;
    }
    return false;
}

void R3D_PlayAnimation(R3D_AnimationPlayer* player, int animIndex)
{
    if (!is_anim_index_valid(player, animIndex)) {
        R3D_TRACELOG(LOG_WARNING, "Failed to play animation %i; Animation index invalid", animIndex);
        return;
    }

    if (is_anim_index_valid(player, player->activeAnimIndex)) {
        player->states[player->activeAnimIndex].play = false;
    }

    player->states[animIndex].play = true;
    player->activeAnimIndex = animIndex;
}

void R3D_PauseAnimation(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) {
        player->states[player->activeAnimIndex].play = false;
    }
}

void R3D_StopAnimation(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) {
        player->states[player->activeAnimIndex].play = false;
        reset_anim_time(player, player->activeAnimIndex);
    }
}

void R3D_RewindAnimation(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) {
        reset_anim_time(player, player->activeAnimIndex);
    }
}

float R3D_GetAnimationTime(R3D_AnimationPlayer player, int animIndex)
{
    if (is_anim_index_valid(&player, animIndex)) {
        return player.states[animIndex].currentTime;
    }
    return 0.0f;
}

void R3D_SetAnimationTime(R3D_AnimationPlayer* player, int animIndex, float time)
{
    if (!is_anim_index_valid(player, animIndex)) return;

    const R3D_Animation* anim = &player->animLib.animations[animIndex];
    float duration = anim->duration / anim->ticksPerSecond;

    player->states[animIndex].currentTime = Wrap(time, 0.0f, duration);
}

float R3D_GetAnimationSpeed(R3D_AnimationPlayer player, int animIndex)
{
    if (is_anim_index_valid(&player, animIndex)) {
        return player.states[animIndex].speed;
    }
    return 0.0f;
}

void R3D_SetAnimationSpeed(R3D_AnimationPlayer* player, int animIndex, float speed)
{
    if (is_anim_index_valid(player, animIndex)) {
        player->states[animIndex].speed = speed;
    }
}

bool R3D_GetAnimationLoop(R3D_AnimationPlayer player, int animIndex)
{
    if (is_anim_index_valid(&player, animIndex)) {
        return player.states[animIndex].loop;
    }
    return false;
}

void R3D_SetAnimationLoop(R3D_AnimationPlayer* player, int animIndex, bool loop)
{
    if (is_anim_index_valid(player, animIndex)) {
        player->states[animIndex].loop = loop;
    }
}

void R3D_AdvanceAnimationTime(R3D_AnimationPlayer* player, float dt)
{
    int animIndex = player->activeAnimIndex;
    if (!is_anim_index_valid(player, animIndex)) {
        return;
    }

    R3D_AnimationState* state = &player->states[animIndex];
    if (!state->play) return;

    state->currentTime += state->speed * dt;

    const R3D_Animation* anim = &player->animLib.animations[animIndex];
    float duration = anim->duration / anim->ticksPerSecond;

    if ((state->speed > 0.0f && state->currentTime >= duration) || (state->speed < 0.0f && state->currentTime <= 0.0f)) {
        if ((state->play = state->loop)) {
            emit_event(player, R3D_ANIM_EVENT_LOOPED, animIndex);
            state->currentTime -= copysignf(duration, state->speed);
        }
        else {
            emit_event(player, R3D_ANIM_EVENT_FINISHED, animIndex);
            state->currentTime = Clamp(state->currentTime, 0.0f, duration);
        }
    }
}

void R3D_ComputeAnimationLocalPose(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) compute_local_matrices(player);
    else memcpy(player->localPose, player->skeleton.localBind, player->skeleton.boneCount * sizeof(Matrix));
}

void R3D_ComputeAnimationModelPose(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) r3d_anim_matrices_compute(player);
    else memcpy(player->modelPose, player->skeleton.modelBind, player->skeleton.boneCount * sizeof(Matrix));
}

void R3D_ComputeAnimationPose(R3D_AnimationPlayer* player)
{
    if (is_anim_index_valid(player, player->activeAnimIndex)) {
        compute_local_matrices(player);
        r3d_anim_matrices_compute(player);
    }
    else {
        memcpy(player->localPose, player->skeleton.localBind, player->skeleton.boneCount * sizeof(Matrix));
        memcpy(player->modelPose, player->skeleton.modelBind, player->skeleton.boneCount * sizeof(Matrix));
    }
}

void R3D_UploadAnimationPose(R3D_AnimationPlayer* player)
{
    for (int i = 0; i < player->skeleton.boneCount; i++) {
        player->skinBuffer[i] = MatrixMultiply(player->skeleton.invBind[i], player->modelPose[i]);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, player->skinTexture);
    glTexSubImage1D(GL_TEXTURE_1D, 0, 0, 4 * player->skeleton.boneCount, GL_RGBA, GL_FLOAT, player->skinBuffer);
    glBindTexture(GL_TEXTURE_1D, 0);
}

void R3D_UpdateAnimationPlayer(R3D_AnimationPlayer* player, float dt)
{
    R3D_ComputeAnimationPose(player);
    R3D_UploadAnimationPose(player);

    R3D_AdvanceAnimationTime(player, dt);
}

// ========================================
// INTERNAL FUNCTIONS DEFINITIONS
// ========================================

void emit_event(R3D_AnimationPlayer* player, R3D_AnimationEvent event, int animIndex)
{
    if (player->eventCallback != NULL) {
        player->eventCallback(player, event, animIndex, player->eventUserData);
    }
}

bool is_anim_index_valid(R3D_AnimationPlayer* player, int animIndex)
{
    return animIndex >= 0 && animIndex < player->animLib.count;
}

void reset_anim_time(R3D_AnimationPlayer* player, int animIndex)
{
    assert(is_anim_index_valid(player, animIndex));

    R3D_AnimationState* state = &player->states[animIndex];
    const R3D_Animation* anim = &player->animLib.animations[animIndex];

    state->currentTime = (state->speed >= 0.0f)
        ? 0.0f : anim->duration / anim->ticksPerSecond;
}

void compute_local_matrices(R3D_AnimationPlayer* player)
{
    assert(is_anim_index_valid(player, player->activeAnimIndex));

    const R3D_AnimationState* state = &player->states[player->activeAnimIndex];
    const R3D_Animation* anim = &player->animLib.animations[player->activeAnimIndex];

    float tick = state->currentTime * anim->ticksPerSecond;
    int boneCount = player->skeleton.boneCount;
    Matrix* localPose = player->localPose;

    for (int iBone = 0; iBone < boneCount; iBone++) {
        const R3D_AnimationChannel* channel = r3d_anim_channel_find(anim, iBone);
        if (channel == NULL) {
            localPose[iBone] = player->skeleton.localBind[iBone];
            continue;
        }
        Transform local = r3d_anim_channel_lerp(channel, tick, NULL, NULL);
        localPose[iBone] = r3d_matrix_srt_quat(local.scale, QuaternionNormalize(local.rotation), local.translation);
    }
}
