/* r3d_culling.h -- R3D Culling Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./modules/r3d_cache.h"

// ========================================
// PUBLIC API
// ========================================

bool R3D_IsPointInFrustum(Vector3 position)
{
	return r3d_frustum_is_point_in(&R3D_CACHE_GET(viewState.frustum), &position);
}

bool R3D_IsSphereInFrustum(Vector3 position, float radius)
{
	return r3d_frustum_is_sphere_in(&R3D_CACHE_GET(viewState.frustum), &position, radius);
}

bool R3D_IsAABBInFrustum(BoundingBox aabb)
{
	return r3d_frustum_is_aabb_in(&R3D_CACHE_GET(viewState.frustum), &aabb);
}

bool R3D_IsOBBInFrustum(BoundingBox aabb, Matrix transform)
{
	return r3d_frustum_is_obb_in(&R3D_CACHE_GET(viewState.frustum), &aabb, &transform);
}
