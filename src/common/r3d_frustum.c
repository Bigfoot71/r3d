/* r3d_frustum.c -- Common R3D Frustum Functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "./r3d_frustum.h"

#include <raymath.h>
#include <float.h>

/* === Internal functions === */

static inline Vector4 r3d_frustum_normalize_plane(Vector4 plane)
{
    float len = sqrtf(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    if (len <= 1e-6f) return (Vector4) { 0 };

    float invLen = 1.0f / len;
    plane.x *= invLen;
    plane.y *= invLen;
    plane.z *= invLen;
    plane.w *= invLen;

    return plane;
}

static inline float r3d_frustum_distance_to_plane(const Vector4* plane, const Vector3* position)
{
    return plane->x * position->x + plane->y * position->y + plane->z * position->z + plane->w;
}

/* === Public functions === */

r3d_frustum_t r3d_frustum_create(Matrix viewProj)
{
    r3d_frustum_t frustum = { 0 };

    frustum.planes[R3D_PLANE_RIGHT] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 - viewProj.m0,
        viewProj.m7 - viewProj.m4,
        viewProj.m11 - viewProj.m8,
        viewProj.m15 - viewProj.m12
    });

    frustum.planes[R3D_PLANE_LEFT] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 + viewProj.m0,
        viewProj.m7 + viewProj.m4,
        viewProj.m11 + viewProj.m8,
        viewProj.m15 + viewProj.m12
    });

    frustum.planes[R3D_PLANE_TOP] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 - viewProj.m1,
        viewProj.m7 - viewProj.m5,
        viewProj.m11 - viewProj.m9,
        viewProj.m15 - viewProj.m13
    });

    frustum.planes[R3D_PLANE_BOTTOM] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 + viewProj.m1,
        viewProj.m7 + viewProj.m5,
        viewProj.m11 + viewProj.m9,
        viewProj.m15 + viewProj.m13
    });

    frustum.planes[R3D_PLANE_BACK] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 - viewProj.m2,
        viewProj.m7 - viewProj.m6,
        viewProj.m11 - viewProj.m10,
        viewProj.m15 - viewProj.m14
    });

    frustum.planes[R3D_PLANE_FRONT] = r3d_frustum_normalize_plane((Vector4) {
        viewProj.m3 + viewProj.m2,
        viewProj.m7 + viewProj.m6,
        viewProj.m11 + viewProj.m10,
        viewProj.m15 + viewProj.m14
    });

    return frustum;
}

BoundingBox r3d_frustum_get_bounding_box(Matrix viewProj)
{
    Matrix matInv = MatrixInvert(viewProj);

    // Points in clip space with correct w component
    Vector4 clipCorners[8] = {
        { -1, -1, -1, 1 }, { 1, -1, -1, 1 }, { 1, 1, -1, 1 }, { -1, 1, -1, 1 },  // Near
        { -1, -1, 1, 1 }, { 1, -1, 1, 1 }, { 1, 1, 1, 1 }, { -1, 1, 1, 1 }       // Far
    };

    BoundingBox bbox = {
        .min = (Vector3){ FLT_MAX, FLT_MAX, FLT_MAX },
        .max = (Vector3){ -FLT_MAX, -FLT_MAX, -FLT_MAX }
    };

    for (int i = 0; i < 8; i++) {
        Vector4 p = clipCorners[i];

        // Transform to world space
        float x = p.x * matInv.m0 + p.y * matInv.m4 + p.z * matInv.m8 + p.w * matInv.m12;
        float y = p.x * matInv.m1 + p.y * matInv.m5 + p.z * matInv.m9 + p.w * matInv.m13;
        float z = p.x * matInv.m2 + p.y * matInv.m6 + p.z * matInv.m10 + p.w * matInv.m14;
        float w = p.x * matInv.m3 + p.y * matInv.m7 + p.z * matInv.m11 + p.w * matInv.m15;

        // Perspective divide
        if (fabsf(w) > 1e-6f) {  // Avoid division by very small numbers
            x /= w;
            y /= w;
            z /= w;
        }

        // Update bounding box
        bbox.min.x = fminf(bbox.min.x, x);
        bbox.min.y = fminf(bbox.min.y, y);
        bbox.min.z = fminf(bbox.min.z, z);
        bbox.max.x = fmaxf(bbox.max.x, x);
        bbox.max.y = fmaxf(bbox.max.y, y);
        bbox.max.z = fmaxf(bbox.max.z, z);
    }

    return bbox;
}

bool r3d_frustum_is_point_in(const r3d_frustum_t* frustum, const Vector3* position)
{
    for (int i = 0; i < R3D_PLANE_COUNT; i++) {
        if (r3d_frustum_distance_to_plane(&frustum->planes[i], position) <= 0) {
            return false;
        }
    }
    return true;
}

bool r3d_frustum_is_points_in(const r3d_frustum_t* frustum, const Vector3* positions, int count)
{
    for (int i = 0; i < count; i++) {
        if (r3d_frustum_is_point_in(frustum, positions)) {
            return true;
        }
    }

    return false;
}

bool r3d_frustum_is_sphere_in(const r3d_frustum_t* frustum, const Vector3* position, float radius)
{
    for (int i = 0; i < R3D_PLANE_COUNT; i++) {
        if (r3d_frustum_distance_to_plane(&frustum->planes[i], position) < -radius) {
            return false;
        }
    }
    return true;
}

bool r3d_frustum_is_aabb_in(const r3d_frustum_t* frustum, const BoundingBox* aabb)
{
    float xMin = aabb->min.x, yMin = aabb->min.y, zMin = aabb->min.z;
    float xMax = aabb->max.x, yMax = aabb->max.y, zMax = aabb->max.z;

    for (int i = 0; i < R3D_PLANE_COUNT; i++)
    {
        const Vector4* plane = &frustum->planes[i];

        // Choose the optimal coordinates according to the sign of the normal
        float distance = r3d_frustum_distance_to_plane(plane, &(Vector3)
            {
                .x = (plane->x >= 0.0f) ? xMax : xMin,
                .y = (plane->y >= 0.0f) ? yMax : yMin,
                .z = (plane->z >= 0.0f) ? zMax : zMin
            }
        );

        if (distance < -EPSILON) {
            return false;
        }
    }
    return true;
}

bool r3d_frustum_is_obb_in(const r3d_frustum_t* frustum, const BoundingBox* aabb, const Matrix* transform)
{
    // Compute OBB center and extents in local space
    float xCenter = (aabb->min.x + aabb->max.x) * 0.5f;
    float yCenter = (aabb->min.y + aabb->max.y) * 0.5f;
    float zCenter = (aabb->min.z + aabb->max.z) * 0.5f;
    float xExtent = (aabb->max.x - aabb->min.x) * 0.5f;
    float yExtent = (aabb->max.y - aabb->min.y) * 0.5f;
    float zExtent = (aabb->max.z - aabb->min.z) * 0.5f;

    // Transform center to world space
    float xWorldCenter = transform->m0 * xCenter + transform->m4 * yCenter + transform->m8 * zCenter + transform->m12;
    float yWorldCenter = transform->m1 * xCenter + transform->m5 * yCenter + transform->m9 * zCenter + transform->m13;
    float zWorldCenter = transform->m2 * xCenter + transform->m6 * yCenter + transform->m10 * zCenter + transform->m14;

    // Test OBB against each frustum plane
    for (int i = 0; i < R3D_PLANE_COUNT; i++)
    {
        const Vector4* plane = &frustum->planes[i];

        // Signed distance from OBB center to plane
        float centerDistance = plane->x * xWorldCenter + plane->y * yWorldCenter + plane->z * zWorldCenter + plane->w;

        // Project OBB extents onto plane normal
        float projectedRadius =
            fabsf(plane->x * transform->m0 + plane->y * transform->m1 + plane->z * transform->m2) * xExtent +
            fabsf(plane->x * transform->m4 + plane->y * transform->m5 + plane->z * transform->m6) * yExtent +
            fabsf(plane->x * transform->m8 + plane->y * transform->m9 + plane->z * transform->m10) * zExtent;

        // If OBB is fully outside the plane, it's outside the frustum
        if (centerDistance + projectedRadius < -EPSILON) {
            return false;
        }
    }

    // OBB is at least partially inside all planes
    return true;
}
