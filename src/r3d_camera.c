/* r3d_camera.c -- R3D Camera Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 */

#include <r3d/r3d_camera.h>
#include <stddef.h>

#include "./common/r3d_math.h"

// ========================================
// PUBLIC API
// ========================================

R3D_Camera R3D_CameraFromRL(Camera camera)
{
    R3D_Camera result = R3D_CAMERA_BASE;

    result.position = camera.position;
    result.fovy = camera.fovy;

    result.projection =
        (camera.projection == CAMERA_ORTHOGRAPHIC)
            ? R3D_PROJECTION_ORTHOGRAPHIC
            : R3D_PROJECTION_PERSPECTIVE;

    R3D_CameraLookAt(&result, camera.target, camera.up);

    return result;
}

Camera R3D_CameraToRL(R3D_Camera camera)
{
    Camera result = {0};

    result.position = camera.position;
    result.target = Vector3Add(camera.position, R3D_GetCameraForward(camera));
    result.up = R3D_GetCameraUp(camera);
    result.fovy = camera.fovy;

    result.projection =
        (camera.projection == R3D_PROJECTION_ORTHOGRAPHIC)
            ? CAMERA_ORTHOGRAPHIC
            : CAMERA_PERSPECTIVE;

    return result;
}

void R3D_CameraLookAt(R3D_Camera* camera, Vector3 target, Vector3 up)
{
    if (camera == NULL) return;

    Vector3 forward = Vector3Subtract(target, camera->position);

    if (r3d_vector3_len_sq(forward) <= 1e-12f) {
        forward = (Vector3){0.0f, 0.0f, -1.0f};
        target = Vector3Add(camera->position, forward);
    }

    if (r3d_vector3_len_sq(up) <= 1e-12f) {
        up = (Vector3){0.0f, 1.0f, 0.0f};
    }

    Matrix view = MatrixLookAt(camera->position, target, up);
    Matrix world = MatrixInvert(view);

    camera->rotation = QuaternionFromMatrix(world);
    camera->rotation = r3d_quaternion_normalize_or_id(camera->rotation);
}

Vector3 R3D_GetCameraForward(R3D_Camera camera)
{
    return Vector3RotateByQuaternion((Vector3){0, 0, -1}, camera.rotation);
}

Vector3 R3D_GetCameraRight(R3D_Camera camera)
{
    return Vector3RotateByQuaternion((Vector3){1, 0, 0}, camera.rotation);
}

Vector3 R3D_GetCameraUp(R3D_Camera camera)
{
    return Vector3RotateByQuaternion((Vector3){0, 1, 0}, camera.rotation);
}

Matrix R3D_GetCameraView(R3D_Camera camera)
{
    Vector3 forward = R3D_GetCameraForward(camera);
    Vector3 up = R3D_GetCameraUp(camera);

    return MatrixLookAt(
        camera.position,
        Vector3Add(camera.position, forward),
        up
    );
}

Matrix R3D_GetCameraProj(R3D_Camera camera, double aspect)
{
    if (aspect <= 0.0) aspect = 1.0;

    if (camera.projection == R3D_PROJECTION_ORTHOGRAPHIC) {
        double top = camera.fovy * 0.5;
        double right = top * aspect;

        return MatrixOrtho(
            -right,
            right,
            -top,
            top,
            camera.nearPlane,
            camera.farPlane
        );
    }

    return MatrixPerspective(
        camera.fovy * DEG2RAD,
        aspect,
        camera.nearPlane,
        camera.farPlane
    );
}

Matrix R3D_GetCameraViewProj(R3D_Camera camera, double aspect)
{
    Matrix view = R3D_GetCameraView(camera);
    Matrix proj = R3D_GetCameraProj(camera, aspect);

    return MatrixMultiply(view, proj);
}

void R3D_MoveCamera(R3D_Camera* camera, Vector3 delta)
{
    if (camera == NULL) return;

    camera->position = Vector3Add(camera->position, delta);
}

void R3D_MoveCameraLocal(R3D_Camera* camera, Vector3 delta)
{
    if (camera == NULL) return;

    // Local convention: +X right, +Y up, -Z forward.
    Vector3 world_delta = Vector3RotateByQuaternion(delta, camera->rotation);
    camera->position = Vector3Add(camera->position, world_delta);
}

void R3D_CameraRotate(R3D_Camera* camera, Quaternion rotation)
{
    if (camera == NULL) return;

    rotation = r3d_quaternion_normalize_or_id(rotation);

    // Local-space rotation composition.
    camera->rotation = QuaternionMultiply(camera->rotation, rotation);
    camera->rotation = r3d_quaternion_normalize_or_id(camera->rotation);
}

void R3D_CameraPitch(R3D_Camera* camera, float angle)
{
    if (camera == NULL) return;

    Quaternion q = QuaternionFromAxisAngle((Vector3){1, 0, 0}, angle);
    R3D_CameraRotate(camera, q);
}

void R3D_CameraYaw(R3D_Camera* camera, float angle)
{
    if (camera == NULL) return;

    Quaternion q = QuaternionFromAxisAngle((Vector3){0, 1, 0}, angle);
    R3D_CameraRotate(camera, q);
}

void R3D_CameraRoll(R3D_Camera* camera, float angle)
{
    if (camera == NULL) return;

    // Roll around local forward. Since forward is -Z, the local axis is {0, 0, -1}.
    Quaternion q = QuaternionFromAxisAngle((Vector3){0, 0, -1}, angle);
    R3D_CameraRotate(camera, q);
}

void R3D_SetCameraLayers(R3D_Camera* camera, R3D_Layer layers)
{
    if (camera == NULL) return;
    camera->layers = layers;
}

void R3D_EnableCameraLayers(R3D_Camera* camera, R3D_Layer layers)
{
    if (camera == NULL) return;
    camera->layers |= layers;
}

void R3D_DisableCameraLayers(R3D_Camera* camera, R3D_Layer layers)
{
    if (camera == NULL) return;
    camera->layers &= ~layers;
}

void R3D_ToggleCameraLayers(R3D_Camera* camera, R3D_Layer layers)
{
    if (camera == NULL) return;
    camera->layers ^= layers;
}

bool R3D_HasCameraLayers(R3D_Camera camera, R3D_Layer layers)
{
    return (camera.layers & layers) != 0;
}
