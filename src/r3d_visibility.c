/* r3d_visibility.h -- R3D Visibility Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_core_state.h"

#include "./common/r3d_frustum.h"
#include "./common/r3d_math.h"

// ========================================
// PUBLIC API
// ========================================

bool R3D_IsPointVisible(Vector3 position)
{
	return r3d_frustum_is_point_in(&R3D.viewState.frustum, position);
}

bool R3D_IsSphereVisible(Vector3 position, float radius)
{
	return r3d_frustum_is_sphere_in(&R3D.viewState.frustum, position, radius);
}

bool R3D_IsBoundingBoxVisible(BoundingBox aabb)
{
	return r3d_frustum_is_aabb_in(&R3D.viewState.frustum, aabb);
}

bool R3D_IsOrientedBoxVisible(BoundingBox aabb, Matrix transform)
{
	return r3d_frustum_is_obb_in(&R3D.viewState.frustum, r3d_compute_obb(aabb, transform));
}
