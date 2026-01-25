/* r3d_kinematics.h -- R3D Kinematics Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_KINEMATICS_H
#define R3D_KINEMATICS_H

#include <r3d/r3d_mesh_data.h>
#include <r3d/r3d_model.h>

/**
 * @defgroup Kinematics
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @brief Capsule shape defined by two endpoints and radius
 */
typedef struct R3D_Capsule {
    Vector3 start;      ///< Start point of capsule axis
    Vector3 end;        ///< End point of capsule axis
    float radius;       ///< Capsule radius
} R3D_Capsule;

/**
 * @brief Penetration information from an overlap test
 */
typedef struct R3D_Penetration {
    bool collides;      ///< Whether shapes are overlapping
    float depth;        ///< Penetration depth
    Vector3 normal;     ///< Collision normal (direction to resolve penetration)
    Vector3 mtv;        ///< Minimum Translation Vector (normal * depth)
} R3D_Penetration;

/**
 * @brief Collision information from a sweep test
 */
typedef struct R3D_SweepCollision {
    bool hit;           ///< Whether a collision occurred
    float time;         ///< Time of impact [0-1], fraction along velocity vector
    Vector3 point;      ///< World space collision point
    Vector3 normal;     ///< Surface normal at collision point
} R3D_SweepCollision;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if capsule intersects with box
 * @param capsule Capsule shape
 * @param box Bounding box
 * @return true if collision detected
 */
R3DAPI bool R3D_CheckCollisionCapsuleBox(R3D_Capsule capsule, BoundingBox box);

/**
 * @brief Check if capsule intersects with sphere
 * @param capsule Capsule shape
 * @param center Sphere center
 * @param radius Sphere radius
 * @return true if collision detected
 */
R3DAPI bool R3D_CheckCollisionCapsuleSphere(R3D_Capsule capsule, Vector3 center, float radius);

/**
 * @brief Check if two capsules intersect
 * @param a First capsule
 * @param b Second capsule
 * @return true if collision detected
 */
R3DAPI bool R3D_CheckCollisionCapsules(R3D_Capsule a, R3D_Capsule b);

/**
 * @brief Check if capsule intersects with mesh
 * @param capsule Capsule shape
 * @param mesh Mesh data
 * @param transform Mesh transform
 * @return true if collision detected
 */
R3DAPI bool R3D_CheckCollisionCapsuleMesh(R3D_Capsule capsule, R3D_MeshData mesh, Matrix transform);

/**
 * @brief Check penetration between capsule and box
 * @param capsule Capsule shape
 * @param box Bounding box
 * @return Penetration information.
 */
R3DAPI R3D_Penetration R3D_CheckPenetrationCapsuleBox(R3D_Capsule capsule, BoundingBox box);

/**
 * @brief Check penetration between capsule and sphere
 * @param capsule Capsule shape
 * @param center Sphere center
 * @param radius Sphere radius
 * @return Penetration information.
 */
R3DAPI R3D_Penetration R3D_CheckPenetrationCapsuleSphere(R3D_Capsule capsule, Vector3 center, float radius);

/**
 * @brief Check penetration between two capsules
 * @param a First capsule
 * @param b Second capsule
 * @return Penetration information.
 */
R3DAPI R3D_Penetration R3D_CheckPenetrationCapsules(R3D_Capsule a, R3D_Capsule b);

/**
 * @brief Calculate slide velocity along surface
 * @param velocity Original velocity
 * @param normal Surface normal (must be normalized)
 * @return Velocity sliding along surface (perpendicular component removed)
 */
R3DAPI Vector3 R3D_SlideVelocity(Vector3 velocity, Vector3 normal);

/**
 * @brief Calculate bounce velocity after collision
 * @param velocity Incoming velocity
 * @param normal Surface normal (must be normalized)
 * @param bounciness Coefficient of restitution (0=no bounce, 1=perfect bounce)
 * @return Reflected velocity
 */
R3DAPI Vector3 R3D_BounceVelocity(Vector3 velocity, Vector3 normal, float bounciness);

/**
 * @brief Slide sphere along box surface, resolving collisions
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Desired movement vector
 * @param box Obstacle bounding box
 * @param outNormal Optional: receives collision normal if collision occurred
 * @return Actual movement applied (may be reduced/redirected by collision)
 */
R3DAPI Vector3 R3D_SlideSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide sphere along mesh surface, resolving collisions
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Desired movement vector
 * @param mesh Mesh data to collide against
 * @param transform Mesh world transform
 * @param outNormal Optional: receives collision normal if collision occurred
 * @return Actual movement applied (may be reduced/redirected by collision)
 */
R3DAPI Vector3 R3D_SlideSphereMesh(Vector3 center, float radius, Vector3 velocity, R3D_MeshData mesh, Matrix transform, Vector3* outNormal);

/**
 * @brief Slide capsule along box surface, resolving collisions
 * @param capsule Capsule shape
 * @param velocity Desired movement vector
 * @param box Obstacle bounding box
 * @param outNormal Optional: receives collision normal if collision occurred
 * @return Actual movement applied (may be reduced/redirected by collision)
 */
R3DAPI Vector3 R3D_SlideCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide capsule along mesh surface, resolving collisions
 * @param capsule Capsule shape
 * @param velocity Desired movement vector
 * @param mesh Mesh data to collide against
 * @param transform Mesh world transform
 * @param outNormal Optional: receives collision normal if collision occurred
 * @return Actual movement applied (may be reduced/redirected by collision)
 */
R3DAPI Vector3 R3D_SlideCapsuleMesh(R3D_Capsule capsule, Vector3 velocity, R3D_MeshData mesh, Matrix transform, Vector3* outNormal);

/**
 * @brief Push sphere out of box if penetrating
 * @param center Sphere center (modified in place if penetrating)
 * @param radius Sphere radius
 * @param box Obstacle box
 * @param outPenetration Optional: receives penetration depth
 * @return true if depenetration occurred
 */
R3DAPI bool R3D_DepenetrateSphereBox(Vector3* center, float radius, BoundingBox box, float* outPenetration);

/**
 * @brief Push capsule out of box if penetrating
 * @param capsule Capsule shape (modified in place if penetrating)
 * @param box Obstacle box
 * @param outPenetration Optional: receives penetration depth
 * @return true if depenetration occurred
 */
R3DAPI bool R3D_DepenetrateCapsuleBox(R3D_Capsule* capsule, BoundingBox box, float* outPenetration);

/**
 * @brief Cast a ray against mesh geometry
 * @param ray Ray to cast
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @return Ray collision info (hit, distance, point, normal)
 */
R3DAPI RayCollision R3D_RaycastMesh(Ray ray, R3D_MeshData mesh, Matrix transform);

/**
 * @brief Cast a ray against a model (tests all meshes)
 * @param ray Ray to cast
 * @param model Model to test against (must have valid meshData)
 * @param transform Model world transform
 * @return Ray collision info for closest hit (hit=false if no meshData)
 */
R3DAPI RayCollision R3D_RaycastModel(Ray ray, R3D_Model model, Matrix transform);

/**
 * @brief Sweep sphere against single point
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param point Point to test against
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSpherePoint(Vector3 center, float radius, Vector3 velocity, Vector3 point);

/**
 * @brief Sweep sphere against line segment
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param a Segment start point
 * @param b Segment end point
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSphereSegment(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b);

/**
 * @brief Sweep sphere against triangle plane (no edge/vertex clipping)
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param a Triangle vertex A
 * @param b Triangle vertex B
 * @param c Triangle vertex C
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSphereTrianglePlane(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b, Vector3 c);

/**
 * @brief Sweep sphere against triangle with edge/vertex handling
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param a Triangle vertex A
 * @param b Triangle vertex B
 * @param c Triangle vertex C
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSphereTriangle(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b, Vector3 c);

/**
 * @brief Sweep sphere along velocity vector
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param box Obstacle bounding box
 * @return Sweep collision info (hit, distance, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box);

/**
 * @brief Sweep sphere along velocity vector against mesh geometry
 * @param center Sphere center position
 * @param radius Sphere radius
 * @param velocity Movement vector (direction and magnitude)
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepSphereMesh(Vector3 center, float radius, Vector3 velocity, R3D_MeshData mesh, Matrix transform);

/**
 * @brief Sweep capsule along velocity vector
 * @param capsule Capsule shape to sweep
 * @param velocity Movement vector (direction and magnitude)
 * @param box Obstacle bounding box
 * @return Sweep collision info (hit, distance, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box);

/**
 * @brief Sweep capsule along velocity vector against mesh geometry
 * @param capsule Capsule shape to sweep
 * @param velocity Movement vector (direction and magnitude)
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @return Sweep collision info (hit, time, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepCapsuleMesh(R3D_Capsule capsule, Vector3 velocity, R3D_MeshData mesh, Matrix transform);

/**
 * @brief Check if sphere is grounded against a box
 * @param center Sphere center
 * @param radius Sphere radius
 * @param checkDistance How far below to check
 * @param ground Ground box to test against
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsSphereGroundedBox(Vector3 center, float radius, float checkDistance, BoundingBox ground, RayCollision* outGround);

/**
 * @brief Check if sphere is grounded against mesh geometry
 * @param center Sphere center
 * @param radius Sphere radius
 * @param checkDistance How far below to check
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsSphereGroundedMesh(Vector3 center, float radius, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision* outGround);

/**
 * @brief Check if capsule is grounded against a box
 * @param capsule Character capsule
 * @param checkDistance How far below to check (e.g., 0.1)
 * @param ground Ground box to test against
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsCapsuleGroundedBox(R3D_Capsule capsule, float checkDistance, BoundingBox ground, RayCollision* outGround);

/**
 * @brief Check if capsule is grounded against mesh geometry
 * @param capsule Character capsule
 * @param checkDistance How far below to check
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsCapsuleGroundedMesh(R3D_Capsule capsule, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision* outGround);

/**
 * @brief Find closest point on line segment to given point
 * @param point Query point
 * @param start Segment start
 * @param end Segment end
 * @return Closest point on segment [start, end]
 */
R3DAPI Vector3 R3D_ClosestPointOnSegment(Vector3 point, Vector3 start, Vector3 end);

/**
 * @brief Find closest point on triangle to given point
 * @param p Query point
 * @param a Triangle vertex A
 * @param b Triangle vertex B
 * @param c Triangle vertex C
 * @return Closest point on triangle surface
 */
R3DAPI Vector3 R3D_ClosestPointOnTriangle(Vector3 p, Vector3 a, Vector3 b, Vector3 c);

/**
 * @brief Find closest point on box surface to given point
 * @param point Query point
 * @param box Bounding box
 * @return Closest point on/in box (clamped to box bounds)
 */
R3DAPI Vector3 R3D_ClosestPointOnBox(Vector3 point, BoundingBox box);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Kinematics

#endif // R3D_KINEMATICS_H
