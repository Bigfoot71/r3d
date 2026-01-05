/* r3d_frustum.h -- Common R3D Frustum Functions
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_COMMON_FRUSTUM_H
#define R3D_COMMON_FRUSTUM_H

#include <raylib.h>

/* === Types ===  */

typedef enum {
    R3D_PLANE_BACK = 0,
    R3D_PLANE_FRONT,
    R3D_PLANE_BOTTOM,
    R3D_PLANE_TOP,
    R3D_PLANE_RIGHT,
    R3D_PLANE_LEFT,
    R3D_PLANE_COUNT
} r3d_plane_e;

typedef struct {
    Vector4 planes[R3D_PLANE_COUNT];
} r3d_frustum_t;

/* === Functions === */

r3d_frustum_t r3d_frustum_create(Matrix viewProj);
BoundingBox r3d_frustum_get_bounding_box(Matrix viewProj);
bool r3d_frustum_is_point_in(const r3d_frustum_t* frustum, const Vector3* position);
bool r3d_frustum_is_points_in(const r3d_frustum_t* frustum, const Vector3* positions, int count);
bool r3d_frustum_is_sphere_in(const r3d_frustum_t* frustum, const Vector3* position, float radius);
bool r3d_frustum_is_aabb_in(const r3d_frustum_t* frustum, const BoundingBox* aabb);
bool r3d_frustum_is_obb_in(const r3d_frustum_t* frustum, const BoundingBox* aabb, const Matrix* transform);

#endif // R3D_COMMON_FRUSTUM_H
