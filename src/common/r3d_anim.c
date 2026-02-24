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
                            uint32_t* out_idx_0, uint32_t* out_idx_1,
                            float* out_t)
{
    // No keys
    if(count == 0) {
        *out_idx_0 = *out_idx_1 = 0;
        *out_t     = 0.0f;
        return;
    }
    // Single key or before first
    if(count == 1 || time <= times[0]) {
        *out_idx_0 = *out_idx_1 = 0;
        *out_t     = 0.0f;
        return;
    }
    // After last
    if(time >= times[count - 1]) {
        *out_idx_0 = *out_idx_1 = count - 1;
        *out_t     = 0.0f;
        return;
    }
    // Binary search
    uint32_t left = 0;
    uint32_t right = count - 1;
    while(right - left > 1) {
        uint32_t mid = (left + right) >> 1;
        if(times[mid] <= time) left  = mid;
        else                   right = mid;
    }
    *out_idx_0 = left;
    *out_idx_1 = right;

    const float t0 = times[left];
    const float t1 = times[right];
    const float dt = t1 - t0;
    *out_t = (dt > 0.0f) ? (time - t0) / dt : 0.0f;
}

// ========================================
// TRANSFORM/MATRIX FUNCTIONS
// ========================================

Transform r3d_anim_transform_lerp(Transform tf_a, Transform tf_b, float value)
{
    Transform result;
    result.translation = Vector3Lerp(tf_a.translation, tf_b.translation, value);
    result.rotation    = QuaternionSlerp(tf_a.rotation, tf_b.rotation, value);
    result.scale       = Vector3Lerp(tf_a.scale, tf_b.scale, value);
    return result;
}

Transform r3d_anim_transform_add(Transform tf_a, Transform tf_b)
{
    Transform result;
    result.translation = Vector3Add(tf_a.translation, tf_b.translation);
    result.rotation    = QuaternionAdd(tf_a.rotation, tf_b.rotation);
    result.scale       = Vector3Add(tf_a.scale, tf_b.scale);
    return result;
}

Transform r3d_anim_transform_add_v(Transform tf_a, Transform tf_b, float value)
{
    Transform result;
    result.translation = Vector3Add(tf_a.translation,
                                    Vector3Scale(tf_b.translation, value));
    result.rotation    = QuaternionAdd(tf_a.rotation,
                                       QuaternionScale(tf_b.rotation, value));
    result.scale       = Vector3Add(tf_a.scale,
                                    Vector3Scale(tf_b.scale, value));
    return result;
}

Transform r3d_anim_transform_addx_v(Transform tf_a, Transform tf_b, float value)
{
    Transform result;
    result.translation = Vector3Add(tf_a.translation,
                                    Vector3Scale(tf_b.translation, value));
    result.rotation    = QuaternionSlerp(tf_a.rotation, tf_b.rotation, value);
    result.scale       = Vector3Add(tf_a.scale,
                                    Vector3Scale(tf_b.scale, value));
    return result;
}

Transform r3d_anim_transform_subtr(Transform tf_a, Transform tf_b)
{
    Transform result;
    result.translation = Vector3Subtract(tf_a.translation, tf_b.translation);
    result.rotation    = QuaternionSubtract(tf_a.rotation, tf_b.rotation);
    result.scale       = Vector3Subtract(tf_a.scale, tf_b.scale);
    return result;
}

Transform r3d_anim_transform_scale(Transform tf, float val)
{
    Transform result;
    result.translation = Vector3Scale(tf.translation, val);
    result.rotation    = QuaternionScale(tf.rotation, val);
    result.scale       = Vector3Scale(tf.scale, val);
    return result;
}

void r3d_anim_matrices_compute(R3D_AnimationPlayer* player)
{
    const R3D_BoneInfo* bones      = player->skeleton.bones;
    const Matrix        root_bind  = player->skeleton.rootBind;
    const Matrix*       local_pose = player->localPose;
    const int           bone_cnt   = player->skeleton.boneCount;

    Matrix* pose = player->modelPose;
    for(int bone_idx = 0; bone_idx < bone_cnt; bone_idx++) {
        int    parent_idx  = bones[bone_idx].parent;
        Matrix parent_pose = parent_idx >= 0 ? pose[parent_idx] : root_bind;

        pose[bone_idx] = MatrixMultiply(local_pose[bone_idx], parent_pose);
    }
}

// ========================================
// ANIMATION CHANNEL FUNCTIONS
// ========================================

const R3D_AnimationChannel* r3d_anim_channel_find(const R3D_Animation* anim, int bone_idx)
{
    for(int i = 0; i < anim->channelCount; i++)
        if(anim->channels[i].boneIndex == bone_idx)
            return &anim->channels[i];
    return NULL;
}

Transform r3d_anim_channel_lerp(const R3D_AnimationChannel* channel, float time,
                                Transform* rest_0, Transform* rest_n)
{
    Transform result = {.translation = {0.0f, 0.0f, 0.0f},
                        .rotation    = {0.0f, 0.0f, 0.0f, 1.0f},
                        .scale       = {1.0f, 1.0f, 1.0f}};
    if(channel->translation.count > 0) {
        const Vector3* values = (const Vector3*)channel->translation.values;
        uint32_t       i0, i1;
        float          t;
        find_key_frames(channel->translation.times,
                        channel->translation.count,
                        time, &i0, &i1, &t);
        result.translation = Vector3Lerp(values[i0], values[i1], t);
        if(rest_0) rest_0->translation = values[0];
        if(rest_n) rest_n->translation = values[channel->translation.count-1];
    }
    if(channel->rotation.count > 0) {
        const Quaternion* values = (const Quaternion*)channel->rotation.values;
        uint32_t          i0, i1;
        float             t;
        find_key_frames(channel->rotation.times,
                        channel->rotation.count,
                        time, &i0, &i1, &t);
        result.rotation = QuaternionSlerp(values[i0], values[i1], t);
        if(rest_0) rest_0->rotation = values[0];
        if(rest_n) rest_n->rotation = values[channel->rotation.count-1];
    }
    if(channel->scale.count > 0) {
        const Vector3* values = (const Vector3*)channel->scale.values;
        uint32_t       i0, i1;
        float          t;
        find_key_frames(channel->scale.times,
                        channel->scale.count,
                        time, &i0, &i1, &t);
        result.scale = Vector3Lerp(values[i0], values[i1], t);
        if(rest_0) rest_0->scale = values[0];
        if(rest_n) rest_n->scale = values[channel->scale.count-1];
    }
    return result;
}

// EOF
