/* r3d_culling.h -- R3D Culling Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_state.h"

// ========================================
// PUBLIC API
// ========================================

bool R3D_IsPointInFrustum(Vector3 position)
{
	return r3d_frustum_is_point_in(&R3D.state.frustum.shape, &position);
}

bool R3D_IsSphereInFrustum(Vector3 position, float radius)
{
	return r3d_frustum_is_sphere_in(&R3D.state.frustum.shape, &position, radius);
}

bool R3D_IsAABBInFrustum(BoundingBox aabb)
{
	return r3d_frustum_is_aabb_in(&R3D.state.frustum.shape, &aabb);
}

bool R3D_IsOBBInFrustum(BoundingBox aabb, Matrix transform)
{
	return r3d_frustum_is_obb_in(&R3D.state.frustum.shape, &aabb, &transform);
}

bool R3D_IsPointInFrustumBoundingBox(Vector3 position)
{
	const BoundingBox* box = &R3D.state.frustum.aabb;

	return
        position.x >= box->min.x && position.x <= box->max.x &&
        position.y >= box->min.y && position.y <= box->max.y &&
        position.z >= box->min.z && position.z <= box->max.z;
}

bool R3D_IsSphereInFrustumBoundingBox(Vector3 position, float radius)
{
	const BoundingBox* box = &R3D.state.frustum.aabb;

    return
        position.x + radius >= box->min.x && position.x - radius <= box->max.x &&
        position.y + radius >= box->min.y && position.y - radius <= box->max.y &&
        position.z + radius >= box->min.z && position.z - radius <= box->max.z;
}

bool R3D_IsAABBInFrustumBoundingBox(BoundingBox aabb)
{
	const BoundingBox* box = &R3D.state.frustum.aabb;

    return
        aabb.max.x >= box->min.x && aabb.min.x <= box->max.x &&
        aabb.max.y >= box->min.y && aabb.min.y <= box->max.y &&
        aabb.max.z >= box->min.z && aabb.min.z <= box->max.z;
}
