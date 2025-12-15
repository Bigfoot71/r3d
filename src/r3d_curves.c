/* r3d_curves.h -- R3D Curves Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_curves.h>
#include <raymath.h>
#include <stdlib.h>

#include "./details/containers/r3d_array.h"

// ========================================
// PUBLIC API
// ========================================

R3D_InterpolationCurve R3D_LoadInterpolationCurve(int capacity)
{
    R3D_InterpolationCurve curve;

    curve.keyframes = RL_MALLOC(capacity * sizeof(R3D_Keyframe));
    curve.capacity = capacity;
    curve.count = 0;

    return curve;
}

void R3D_UnloadInterpolationCurve(R3D_InterpolationCurve curve)
{
    RL_FREE(curve.keyframes);
    curve.capacity = 0;
    curve.count = 0;
}

bool R3D_AddKeyframe(R3D_InterpolationCurve* curve, float time, float value)
{
    r3d_array_t array = {
        .data = curve->keyframes,
        .count = curve->count,
        .capacity = curve->capacity,
        .elem_size = sizeof(R3D_Keyframe)
    };

    R3D_Keyframe keyFrame = {
        .time = time,
        .value = value
    };

    int result = r3d_array_push_back(&array, &keyFrame);

    curve->keyframes = array.data;
    curve->capacity = (unsigned int)array.capacity;
    curve->count = (unsigned int)array.count;

    return result == R3D_ARRAY_SUCCESS;
}

float R3D_EvaluateCurve(R3D_InterpolationCurve curve, float time)
{
    if (curve.count == 0) return 0.0f;
    if (time <= curve.keyframes[0].time) return curve.keyframes[0].value;
    if (time >= curve.keyframes[curve.count - 1].time) return curve.keyframes[curve.count - 1].value;

    // Find the two keyframes surrounding the given time
    for (int i = 0; i < (int)curve.count - 1; i++) {
        const R3D_Keyframe* kf1 = &curve.keyframes[i];
        const R3D_Keyframe* kf2 = &curve.keyframes[i + 1];

        if (time >= kf1->time && time <= kf2->time) {
            float t = (time - kf1->time) / (kf2->time - kf1->time); // Normalized time between kf1 and kf2
            return Lerp(kf1->value, kf2->value, t);
        }
    }

    return 0.0f; // Fallback (should not be reached)
}
