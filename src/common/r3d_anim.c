/* r3d_anim.c -- Common R3D Animation Helper Functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_anim.h"

// ========================================
// INTERNAL FUNCTION
// ========================================

static void find_key_frames(const float* times, uint32_t count, float time,
                            uint32_t* outIdx0, uint32_t* outIdx1,
                            float* out_t)
{
    // No keys
    if (count == 0) {
        *outIdx0 = *outIdx1 = 0;
        *out_t = 0.0f;
        return;
    }
    // Single key or before first
    if (count == 1 || time <= times[0]) {
        *outIdx0 = *outIdx1 = 0;
        *out_t = 0.0f;
        return;
    }
    // After last
    if (time >= times[count - 1]) {
        *outIdx0 = *outIdx1 = count - 1;
        *out_t = 0.0f;
        return;
    }
    // Binary search
    uint32_t left = 0;
    uint32_t right = count - 1;
    while(right - left > 1) {
        uint32_t mid = (left + right) >> 1;
        if (times[mid] <= time) left  = mid;
        else right = mid;
    }
    *outIdx0 = left;
    *outIdx1 = right;

    const float t0 = times[left];
    const float t1 = times[right];
    const float dt = t1 - t0;
    *out_t = (dt > 0.0f) ? (time - t0) / dt : 0.0f;
}

// ========================================
// TRANSFORM/MATRIX FUNCTIONS
// ========================================

Transform r3d_anim_transform_lerp(Transform a, Transform b, float value)
{
    Transform result;
    result.translation = Vector3Lerp(a.translation, b.translation, value);
    result.rotation = QuaternionSlerp(a.rotation, b.rotation, value);
    result.scale = Vector3Lerp(a.scale, b.scale, value);
    return result;
}

Transform r3d_anim_transform_add(Transform a, Transform b)
{
    Transform result;
    result.translation = Vector3Add(a.translation, b.translation);
    result.rotation = QuaternionAdd(a.rotation, b.rotation);
    result.scale = Vector3Add(a.scale, b.scale);
    return result;
}

Transform r3d_anim_transform_add_v(Transform a, Transform b, float value)
{
    Transform result;
    result.translation = Vector3Add(a.translation, Vector3Scale(b.translation, value));
    result.rotation = QuaternionAdd(a.rotation, QuaternionScale(b.rotation, value));
    result.scale = Vector3Add(a.scale, Vector3Scale(b.scale, value));
    return result;
}

Transform r3d_anim_transform_addx_v(Transform a, Transform b, float value)
{
    Transform result;
    result.translation = Vector3Add(a.translation, Vector3Scale(b.translation, value));
    result.rotation = QuaternionSlerp(a.rotation, b.rotation, value);
    result.scale = Vector3Add(a.scale, Vector3Scale(b.scale, value));
    return result;
}

Transform r3d_anim_transform_subtr(Transform a, Transform b)
{
    Transform result;
    result.translation = Vector3Subtract(a.translation, b.translation);
    result.rotation = QuaternionSubtract(a.rotation, b.rotation);
    result.scale = Vector3Subtract(a.scale, b.scale);
    return result;
}

Transform r3d_anim_transform_scale(Transform tf, float val)
{
    Transform result;
    result.translation = Vector3Scale(tf.translation, val);
    result.rotation = QuaternionScale(tf.rotation, val);
    result.scale = Vector3Scale(tf.scale, val);
    return result;
}

void r3d_anim_matrices_compute(R3D_AnimationPlayer* player)
{
    const R3D_BoneInfo* bones = player->skeleton.bones;
    const Matrix rootBind = player->skeleton.rootBind;
    const Matrix* localPose = player->localPose;
    const int boneCount = player->skeleton.boneCount;

    Matrix* pose = player->modelPose;
    for (int boneIdx = 0; boneIdx < boneCount; boneIdx++) {
        int parentIdx  = bones[boneIdx].parent;
        Matrix parentPose = parentIdx >= 0 ? pose[parentIdx] : rootBind;
        pose[boneIdx] = MatrixMultiply(localPose[boneIdx], parentPose);
    }
}

// ========================================
// ANIMATION CHANNEL FUNCTIONS
// ========================================

const R3D_AnimationChannel* r3d_anim_channel_find(const R3D_Animation* anim, int boneIdx)
{
    for (int i = 0; i < anim->channelCount; i++) {
        if (anim->channels[i].boneIndex == boneIdx) {
            return &anim->channels[i];
        }
    }
    return NULL;
}

Transform r3d_anim_channel_lerp(const R3D_AnimationChannel* channel, float time, Transform* rest0, Transform* restN)
{
    Transform result = {
        .translation = {0.0f, 0.0f, 0.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    };

    if (channel->translation.count > 0) {
        const Vector3* values = (const Vector3*)channel->translation.values;
        uint32_t i0, i1;
        float t;
        find_key_frames(
            channel->translation.times,
            channel->translation.count,
            time, &i0, &i1, &t
        );
        result.translation = Vector3Lerp(values[i0], values[i1], t);
        if (rest0) rest0->translation = values[0];
        if (restN) restN->translation = values[channel->translation.count-1];
    }

    if (channel->rotation.count > 0) {
        const Quaternion* values = (const Quaternion*)channel->rotation.values;
        uint32_t i0, i1;
        float t;
        find_key_frames(
            channel->rotation.times,
            channel->rotation.count,
            time, &i0, &i1, &t
        );
        result.rotation = QuaternionSlerp(values[i0], values[i1], t);
        if (rest0) rest0->rotation = values[0];
        if (restN) restN->rotation = values[channel->rotation.count-1];
    }

    if (channel->scale.count > 0) {
        const Vector3* values = (const Vector3*)channel->scale.values;
        uint32_t i0, i1;
        float t;
        find_key_frames(
            channel->scale.times,
            channel->scale.count,
            time, &i0, &i1, &t
        );
        result.scale = Vector3Lerp(values[i0], values[i1], t);
        if (rest0) rest0->scale = values[0];
        if (restN) restN->scale = values[channel->scale.count-1];
    }

    return result;
}
