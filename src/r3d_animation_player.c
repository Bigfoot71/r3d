/* r3d_animation_player.c -- R3D Animation Player Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_animation_player.h>
#include <raymath.h>
#include <stdlib.h>
#include <glad.h>

#include "./common/r3d_anim.h"

// ========================================
// INTERNAL FUNCTIONS DECLARATIONS
// ========================================

static void emit_event(R3D_AnimationPlayer* player, R3D_AnimationEvent event, int animIndex);
static void compute_local_matrices(R3D_AnimationPlayer* player, float totalWeight);

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
            .weight = 0.0f,
            .speed = 1.0f,
            .play = false,
            .loop = false
        };
    }

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

void R3D_CalculateAnimationPlayerLocalPose(R3D_AnimationPlayer* player)
{
    const R3D_AnimationState* states = player->states;
    int boneCount = player->skeleton.boneCount;
    int animCount = player->animLib.count;

    float totalWeight = 0.0f;
    for (int iAnim = 0; iAnim < animCount; iAnim++) {
        totalWeight += states[iAnim].weight;
    }

    if (totalWeight > 0.0f) compute_local_matrices(player, totalWeight);
    else memcpy(player->localPose, player->skeleton.localBind, boneCount * sizeof(Matrix));
}

void R3D_CalculateAnimationPlayerModelPose(R3D_AnimationPlayer* player)
{
    const R3D_AnimationState* states = player->states;
    int boneCount = player->skeleton.boneCount;
    int animCount = player->animLib.count;

    bool hasWeight = false;
    for (int iAnim = 0; iAnim < animCount; iAnim++) {
        if (states[iAnim].weight > 0.0f) {
            hasWeight = true;
            break;
        }
    }

    if (hasWeight) r3d_anim_matrices_compute(player);
    else memcpy(player->modelPose, player->skeleton.modelBind, boneCount * sizeof(Matrix));
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
        compute_local_matrices(player, totalWeight);
        r3d_anim_matrices_compute(player);
    }
    else {
        memcpy(player->localPose, player->skeleton.localBind, boneCount * sizeof(Matrix));
        memcpy(player->modelPose, player->skeleton.modelBind, boneCount * sizeof(Matrix));
    }
}

void R3D_UploadAnimationPlayerPose(R3D_AnimationPlayer* player)
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
    R3D_CalculateAnimationPlayerPose(player);
    R3D_UploadAnimationPlayerPose(player);

    R3D_AdvanceAnimationPlayerTime(player, dt);
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

void compute_local_matrices(R3D_AnimationPlayer* player, float totalWeight)
{
    int boneCount = player->skeleton.boneCount;
    int animCount = player->animLib.count;

    const Matrix* localBind = player->skeleton.localBind;
    Matrix* localPose = player->localPose;

    float invTotalWeight = 1.0f / totalWeight;

    for (int iBone = 0; iBone < boneCount; iBone++)
    {
        Transform blended = {0};
        bool isAnimated = false;

        for (int iAnim = 0; iAnim < animCount; iAnim++)
        {
            const R3D_Animation* anim = &player->animLib.animations[iAnim];
            const R3D_AnimationState* state = &player->states[iAnim];
            if (state->weight <= 0.0f) continue;

            const R3D_AnimationChannel* channel = r3d_anim_channel_find(anim, iBone);
            if (!channel) continue;
            isAnimated = true;

            Transform local = r3d_anim_channel_lerp(channel, state->currentTime * anim->ticksPerSecond,
                                                    NULL, NULL);
            float w = state->weight * invTotalWeight;

            // TODO: test 'add_v' vs 'addx_v'
            blended = r3d_anim_transform_add_v(blended, local, w);
            //blended = r3d_anim_transform_addx_v(blended, local, w);
        }

        if (!isAnimated) {
            localPose[iBone] = localBind[iBone];
            continue;
        }

        blended.rotation = QuaternionNormalize(blended.rotation);
        localPose[iBone] = r3d_matrix_srt_quat(blended.scale, blended.rotation, blended.translation);
    }
}
