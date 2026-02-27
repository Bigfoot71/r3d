/* r3d_anim.h -- Common R3D Animation Helper Functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_ANIM_H
#define R3D_COMMON_ANIM_H

#include <r3d/r3d_animation_player.h>

#include "./r3d_math.h"

// ========================================
// TRANSFORM/MATRIX FUNCTIONS
// ========================================

Transform r3d_anim_transform_lerp(Transform a, Transform b, float value);
Transform r3d_anim_transform_add(Transform a, Transform b);
Transform r3d_anim_transform_add_v(Transform a, Transform b, float value);
Transform r3d_anim_transform_addx_v(Transform a, Transform b, float value);
Transform r3d_anim_transform_subtr(Transform a, Transform b);
Transform r3d_anim_transform_scale(Transform tf, float val);
void r3d_anim_matrices_compute(R3D_AnimationPlayer* player);

// ========================================
// ANIMATION CHANNEL FUNCTIONS
// ========================================

const R3D_AnimationChannel* r3d_anim_channel_find(const R3D_Animation* anim, int boneIdx);
Transform r3d_anim_channel_lerp(const R3D_AnimationChannel* channel, float time, Transform* rest0, Transform* restN);

#endif // R3D_COMMON_ANIM_H
