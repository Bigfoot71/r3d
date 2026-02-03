/* r3d_visibility.h -- R3D Visibility Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_VISIBILITY_H
#define R3D_VISIBILITY_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup Visibility
 * @brief Defines screen visibility query functions.
 * @note Do not use these functions to cull objects yourself; r3d handles this correctly internally.
 * @{
 */

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if a point is inside the view frustum.
 *
 * Tests whether a 3D point lies within the camera's frustum by checking against all six planes.
 * You can call this only after `R3D_Begin`, which updates the internal frustum state.
 *
 * @param position The 3D point to test.
 * @return `true` if inside the frustum, `false` otherwise.
 */
R3DAPI bool R3D_IsPointVisible(Vector3 position);

/**
 * @brief Checks if a sphere is inside the view frustum.
 *
 * Tests whether a sphere intersects the camera's frustum using plane-sphere tests.
 * You can call this only after `R3D_Begin`, which updates the internal frustum state.
 *
 * @param position The center of the sphere.
 * @param radius The sphere's radius (must be positive).
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 */
R3DAPI bool R3D_IsSphereVisible(Vector3 position, float radius);

/**
 * @brief Checks if an AABB is inside the view frustum.
 *
 * Determines whether an axis-aligned bounding box intersects the frustum.
 * You can call this only after `R3D_Begin`, which updates the internal frustum state.
 *
 * @param aabb The bounding box to test.
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 */
R3DAPI bool R3D_IsBoundingBoxVisible(BoundingBox aabb);

/**
 * @brief Checks if an OBB is inside the view frustum.
 *
 * Tests an oriented bounding box (transformed AABB) for frustum intersection.
 * You can call this only after `R3D_Begin`, which updates the internal frustum state.
 *
 * @param aabb Local-space bounding box.
 * @param transform World-space transform matrix.
 * @return `true` if the transformed box intersects the frustum, `false` otherwise.
 */
R3DAPI bool R3D_IsOrientedBoxVisible(BoundingBox aabb, Matrix transform);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Visibility

#endif // R3D_VISIBILITY_H
