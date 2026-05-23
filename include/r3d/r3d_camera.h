/* r3d_camera.h -- R3D Camera Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty.
 * For conditions of distribution and use, see the accompanying LICENSE file.
 */

#ifndef R3D_CAMERA_H
#define R3D_CAMERA_H

#include "./r3d_platform.h"

#include <raylib.h>
#include <raymath.h>
#include <stdint.h>

/**
 * @defgroup Camera Camera
 * @{
 */

// ========================================
// CONSTANTS
// ========================================

#define R3D_CAMERA_BASE                                 \
    R3D_LITERAL(R3D_Camera) {                           \
        .position   = {0, 0, 0},                        \
        .rotation   = {0, 0, 0, 1},                     \
        .fovy       = 60.0,                             \
        .nearPlane  = 0.05,                             \
        .farPlane   = 4000.0,                           \
        .layers     = R3D_LAYER_ALL,                    \
        .projection = R3D_PROJECTION_PERSPECTIVE,       \
    }

// ========================================
// ENUM TYPES
// ========================================

/**
 * @brief Projection mode used by an R3D camera.
 */
typedef enum R3D_Projection {
    R3D_PROJECTION_PERSPECTIVE = 0, ///< Perspective projection.
    R3D_PROJECTION_ORTHOGRAPHIC     ///< Orthographic projection.
} R3D_Projection;

// ========================================
// BITFIELD TYPES
// ========================================

/**
 * @brief Bitfield type used to specify rendering layers.
 *
 * This type is used by cameras and renderable objects to control visibility.
 * A renderable object is visible to a camera when at least one bit is shared
 * between the object's layer mask and the camera's layer mask.
 *
 * For convenience, 16 predefined layers are provided here, but the underlying
 * type supports up to 32 layers.
 */
typedef uint32_t R3D_Layer;

#define R3D_LAYER_01    (1u << 0)
#define R3D_LAYER_02    (1u << 1)
#define R3D_LAYER_03    (1u << 2)
#define R3D_LAYER_04    (1u << 3)
#define R3D_LAYER_05    (1u << 4)
#define R3D_LAYER_06    (1u << 5)
#define R3D_LAYER_07    (1u << 6)
#define R3D_LAYER_08    (1u << 7)
#define R3D_LAYER_09    (1u << 8)
#define R3D_LAYER_10    (1u << 9)
#define R3D_LAYER_11    (1u << 10)
#define R3D_LAYER_12    (1u << 11)
#define R3D_LAYER_13    (1u << 12)
#define R3D_LAYER_14    (1u << 13)
#define R3D_LAYER_15    (1u << 14)
#define R3D_LAYER_16    (1u << 15)

#define R3D_LAYER_ALL   UINT32_C(0xFFFFFFFF)

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief Quaternion-based 3D camera used by R3D.
 *
 * Unlike raylib's Camera3D, this camera stores its orientation directly as a
 * quaternion instead of using a target/up pair. It also stores near/far clipping
 * planes and a layer mask used to filter visible renderables.
 */
typedef struct R3D_Camera {
    Vector3 position;               ///< Camera world-space position.
    Quaternion rotation;            ///< Camera world-space orientation.
    double fovy;                    ///< Vertical field of view in degrees for perspective projection; vertical size for orthographic projection.
    double nearPlane;               ///< Distance to the near clipping plane.
    double farPlane;                ///< Distance to the far clipping plane.
    R3D_Layer layers;               ///< Camera visibility layer mask.
    R3D_Projection projection;      ///< Camera projection mode.
} R3D_Camera;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Creates an R3D camera from a raylib camera.
 *
 * The camera position, orientation, field of view and projection mode are
 * derived from the given raylib camera.
 *
 * Since raylib's Camera3D does not store near/far clipping planes or layer
 * masks, the near/far planes are initialized from the current rlgl culling
 * distances, while the layer mask is set to the default R3D camera layer mask.
 *
 * @param camera Raylib camera to convert.
 *
 * @return Converted R3D camera.
 */
R3DAPI R3D_Camera R3D_CameraFromRL(Camera camera);

/**
 * @brief Converts an R3D camera to a raylib camera.
 *
 * The raylib target/up vectors are derived from the camera quaternion.
 * Near/far clipping planes and layer masks are not represented by Camera3D.
 */
R3DAPI Camera R3D_CameraToRL(R3D_Camera camera);

/**
 * @brief Sets the camera orientation so it looks at a target point.
 */
R3DAPI void R3D_CameraLookAt(R3D_Camera* camera, Vector3 target, Vector3 up);

/**
 * @brief Returns the camera forward direction in world space.
 */
R3DAPI Vector3 R3D_GetCameraForward(R3D_Camera camera);

/**
 * @brief Returns the camera right direction in world space.
 */
R3DAPI Vector3 R3D_GetCameraRight(R3D_Camera camera);

/**
 * @brief Returns the camera up direction in world space.
 */
R3DAPI Vector3 R3D_GetCameraUp(R3D_Camera camera);

/**
 * @brief Returns the camera view matrix.
 */
R3DAPI Matrix R3D_GetCameraView(R3D_Camera camera);

/**
 * @brief Returns the camera projection matrix.
 */
R3DAPI Matrix R3D_GetCameraProj(R3D_Camera camera, double aspect);

/**
 * @brief Returns the combined view-projection matrix.
 */
R3DAPI Matrix R3D_GetCameraViewProj(R3D_Camera camera, double aspect);

/**
 * @brief Moves the camera in world space.
 */
R3DAPI void R3D_MoveCamera(R3D_Camera* camera, Vector3 delta);

/**
 * @brief Moves the camera in local space.
 */
R3DAPI void R3D_MoveCameraLocal(R3D_Camera* camera, Vector3 delta);

/**
 * @brief Rotates the camera using a quaternion.
 */
R3DAPI void R3D_CameraRotate(R3D_Camera* camera, Quaternion rotation);

/**
 * @brief Rotates the camera around its local X axis.
 */
R3DAPI void R3D_CameraPitch(R3D_Camera* camera, float angle);

/**
 * @brief Rotates the camera around its local Y axis.
 */
R3DAPI void R3D_CameraYaw(R3D_Camera* camera, float angle);

/**
 * @brief Rotates the camera around its local Z axis.
 */
R3DAPI void R3D_CameraRoll(R3D_Camera* camera, float angle);

/**
 * @brief Replaces the camera visibility layer mask.
 */
R3DAPI void R3D_SetCameraLayers(R3D_Camera* camera, R3D_Layer layers);

/**
 * @brief Enables one or more layers in the camera visibility mask.
 */
R3DAPI void R3D_EnableCameraLayers(R3D_Camera* camera, R3D_Layer layers);

/**
 * @brief Disables one or more layers from the camera visibility mask.
 */
R3DAPI void R3D_DisableCameraLayers(R3D_Camera* camera, R3D_Layer layers);

/**
 * @brief Toggles one or more layers in the camera visibility mask.
 */
R3DAPI void R3D_ToggleCameraLayers(R3D_Camera* camera, R3D_Layer layers);

/**
 * @brief Checks whether at least one of the given layers is visible to the camera.
 */
R3DAPI bool R3D_HasCameraLayers(R3D_Camera camera, R3D_Layer layers);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Camera

#endif // R3D_CAMERA_H
