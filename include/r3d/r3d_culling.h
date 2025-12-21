/* r3d_culling.h -- R3D Culling Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_CULLING_H
#define R3D_CULLING_H

#include "./r3d_platform.h"
#include <raylib.h>

/**
 * @defgroup Culling
 * @brief Defines manual culling functions, note that r3d has also an automatic culling system.
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
 * Call this only between `R3D_Begin` and `R3D_End`.
 *
 * Useful when automatic frustum culling is disabled and you're using a custom spatial structure
 * (e.g., octree, BVH, etc.).
 *
 * @param position The 3D point to test.
 * @return `true` if inside the frustum, `false` otherwise.
 *
 * @note This performs an exact plane-point test. Slower than bounding box tests.
 * @warning Frustum culling may incorrectly discard objects casting visible shadows.
 * @todo Improve shadow-aware culling in future versions.
 *
 * @see R3D_IsPointInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsPointInFrustum(Vector3 position);

/**
 * @brief Checks if a sphere is inside the view frustum.
 *
 * Tests whether a sphere intersects the camera's frustum using plane-sphere tests.
 * Call this only between `R3D_Begin` and `R3D_End`.
 *
 * Useful when managing visibility manually.
 *
 * @param position The center of the sphere.
 * @param radius The sphere's radius (must be positive).
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 *
 * @note More accurate but slower than bounding box approximations.
 * @warning May cause visual issues with shadow casters being culled too early.
 * @todo Add support for shadow-aware visibility.
 *
 * @see R3D_IsSphereInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsSphereInFrustum(Vector3 position, float radius);

/**
 * @brief Checks if an AABB is inside the view frustum.
 *
 * Determines whether an axis-aligned bounding box intersects the frustum.
 * Call between `R3D_Begin` and `R3D_End`.
 *
 * For use in custom culling strategies or spatial partitioning systems.
 *
 * @param aabb The bounding box to test.
 * @return `true` if at least partially inside the frustum, `false` otherwise.
 *
 * @note Exact but more costly than AABB pre-tests.
 * @warning May prematurely cull objects casting visible shadows.
 * @todo Add support for light-aware visibility tests.
 *
 * @see R3D_IsAABBInFrustumBoundingBox()
 */
R3DAPI bool R3D_IsAABBInFrustum(BoundingBox aabb);

/**
 * @brief Checks if an OBB is inside the view frustum.
 *
 * Tests an oriented bounding box (transformed AABB) for frustum intersection.
 * Must be called between `R3D_Begin` and `R3D_End`.
 *
 * Use this for objects with transformations when doing manual culling.
 *
 * @param aabb Local-space bounding box.
 * @param transform World-space transform matrix.
 * @return `true` if the transformed box intersects the frustum, `false` otherwise.
 *
 * @note More expensive than AABB checks due to matrix operations.
 * @warning May incorrectly cull shadow casters.
 * @todo Consider shadow-aware culling improvements.
 *
 * @see R3D_IsAABBInFrustum()
 */
R3DAPI bool R3D_IsOBBInFrustum(BoundingBox aabb, Matrix transform);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Culling

#endif // R3D_CULLING_H
