/* r3d_camera.h -- Common R3D Camera Functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_CAMERA_H
#define R3D_COMMON_CAMERA_H

#include "./r3d_math.h"
#include "raymath.h"
#include <rlgl.h>

// ========================================
// STRUCT TYPES
// ========================================

typedef struct {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    double aspect;
    double fovy;
    double near;
    double far;
    int projection;
} r3d_camera_t;

// ========================================
// STRUCT TYPES
// ========================================

static inline r3d_camera_t r3d_camera_init(Camera3D rCamera, int wTarget, int hTarget)
{
    r3d_camera_t camera = {0};
    camera.position = rCamera.position;
    camera.target = rCamera.target;
    camera.up = rCamera.up;
    camera.aspect = (double)wTarget / hTarget;
    camera.fovy = rCamera.fovy;
    camera.near = rlGetCullDistanceNear();
    camera.far = rlGetCullDistanceFar();
    camera.projection = rCamera.projection;
    return camera;
}

static inline Matrix r3d_camera_view(r3d_camera_t camera)
{
    return MatrixLookAt(camera.position, camera.target, camera.up);
}

static inline Matrix r3d_camera_proj(r3d_camera_t camera)
{
    Matrix proj = R3D_MATRIX_IDENTITY;

    if (camera.projection == CAMERA_PERSPECTIVE) {
        proj = MatrixPerspective(camera.fovy * DEG2RAD, camera.aspect, camera.near, camera.far);
    }
    else if (camera.projection == CAMERA_ORTHOGRAPHIC) {
        double top = camera.fovy / 2.0, right = top * camera.aspect;
        proj = MatrixOrtho(-right, right, -top, top, camera.near, camera.far);
    }

    return proj;
}

#endif // R3D_COMMON_CAMERA_H
