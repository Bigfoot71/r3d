/* r3d_frustum.h -- R3D Frustum Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_FRUSTUM_H
#define R3D_FRUSTUM_H

#include "./r3d_platform.h"
#include "./r3d_shape.h"
#include <raylib.h>

/**
 * @defgroup Frustum Frustum
 * @brief Frustum extraction and intersection tests.
 *
 * @note
 * R3D_Draw* functions already perform frustum culling internally.
 * This module is intended for advanced use cases (e.g. instancing or custom culling).
 *
 * Be careful when manually culling objects: visibility depends on context.
 * An object outside the camera frustum may still contribute to shadows or probes.
 * Avoid removing instances solely based on camera visibility.
 *
 * @{
 */

// ========================================
// ENUM TYPES
// ========================================

/**
 * @brief Frustum plane indices.
 */
typedef enum {
    R3D_PLANE_BACK = 0,
    R3D_PLANE_FRONT,
    R3D_PLANE_BOTTOM,
    R3D_PLANE_TOP,
    R3D_PLANE_RIGHT,
    R3D_PLANE_LEFT,
    R3D_PLANE_COUNT
} R3D_FrustumPlane;

// ========================================
// STRUCT TYPES
// ========================================

/**
 * @brief View frustum defined by its clipping planes.
 *
 * Planes are stored as Vector4 (xyz = normal, w = distance).
 */
typedef struct {
    Vector4 planes[R3D_PLANE_COUNT];
} R3D_Frustum;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the current frustum.
 *
 * Returns the frustum computed during the last call to R3D_Begin().
 */
R3DAPI R3D_Frustum R3D_GetFrustum(void);

/**
 * @brief Compute a frustum from a view-projection matrix.
 */
R3DAPI R3D_Frustum R3D_ComputeFrustum(Matrix viewProj);

/**
 * @brief Compute the axis-aligned bounding box of a frustum.
 *
 * @param invViewProj Inverse view-projection matrix.
 */
R3DAPI BoundingBox R3D_ComputeFrustumBoundingBox(Matrix invViewProj);

/**
 * @brief Compute the eight corner points of a frustum.
 *
 * @param invViewProj Inverse view-projection matrix.
 * @param corners Output array of 8 points.
 */
R3DAPI void R3D_ComputeFrustumCorners(Matrix invViewProj, Vector3 corners[8]);

/**
 * @brief Check if a point is inside the frustum.
 *
 * @param frustum Frustum to test against. Must not be NULL.
 */
R3DAPI bool R3D_FrustumContainsPoint(const R3D_Frustum* frustum, Vector3 position);

/**
 * @brief Check if any point from a set is inside the frustum.
 *
 * @param frustum Frustum to test against. Must not be NULL.
 */
R3DAPI bool R3D_FrustumContainsAnyPoint(const R3D_Frustum* frustum, const Vector3* positions, int count);

/**
 * @brief Check if a sphere intersects the frustum.
 *
 * @param frustum Frustum to test against. Must not be NULL.
 */
R3DAPI bool R3D_FrustumIntersectsSphere(const R3D_Frustum* frustum, Vector3 position, float radius);

/**
 * @brief Check if a bounding box intersects the frustum.
 *
 * @param frustum Frustum to test against. Must not be NULL.
 */
R3DAPI bool R3D_FrustumIntersectsBoundingBox(const R3D_Frustum* frustum, BoundingBox aabb);

/**
 * @brief Check if an oriented box intersects the frustum.
 *
 * @param frustum Frustum to test against. Must not be NULL.
 */
R3DAPI bool R3D_FrustumIntersectsOrientedBox(const R3D_Frustum* frustum, R3D_OrientedBox obb);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Frustum

#endif // R3D_FRUSTUM_H
