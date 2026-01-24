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
 * @brief Slide capsule along box collision (discrete)
 *
 * Fast collision response with accurate smooth surface behavior.
 * Recommended for character controllers and normal gameplay movement.
 *
 * @warning May tunnel if velocity length exceeds capsule size.
 *          For high-speed objects, use R3D_SlideCapsuleBoxSwept instead.
 *
 * @param capsule Capsule shape
 * @param velocity Desired movement velocity
 * @param box Obstacle box
 * @param outNormal Optional: receives collision normal (NULL if not needed)
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideCapsuleBoxSwept High-speed safe version
 */
R3DAPI Vector3 R3D_SlideCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide sphere along box collision (discrete)
 *
 * Fast collision response suitable for most gameplay scenarios.
 *
 * @warning May tunnel if velocity length exceeds sphere radius.
 *          For high-speed objects, use R3D_SlideSphereBoxSwept instead.
 *
 * @param center Sphere center
 * @param radius Sphere radius
 * @param velocity Desired movement velocity
 * @param box Obstacle box
 * @param outNormal Optional: receives collision normal
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideSphereBoxSwept High-speed safe version
 */
R3DAPI Vector3 R3D_SlideSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide box along another box collision (discrete)
 *
 * Extremely fast AABB collision response.
 *
 * @warning May tunnel if velocity is very large relative to box size.
 *          For high-speed objects, use R3D_SlideBoxBoxSwept instead.
 *
 * @param box Moving box
 * @param velocity Desired movement velocity
 * @param obstacle Obstacle box
 * @param outNormal Optional: receives collision normal
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideBoxBoxSwept High-speed safe version
 */
R3DAPI Vector3 R3D_SlideBoxBox(BoundingBox box, Vector3 velocity, BoundingBox obstacle, Vector3* outNormal);

/**
 * @brief Slide capsule along box collision (swept/continuous)
 *
 * Uses swept collision to prevent tunneling at any velocity.
 * Recommended for projectiles, fast-moving objects, and physics-critical scenarios.
 *
 * @note Capsule curved surfaces may behave as if they have corners
 *       due to discrete sampling. For smooth surface contacts, prefer the
 *       non-swept version (R3D_SlideCapsuleBox) when velocity is moderate.
 *
 * @param capsule Capsule shape
 * @param velocity Desired movement velocity (any magnitude)
 * @param box Obstacle box
 * @param outNormal Optional: receives collision normal
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideCapsuleBox Faster version for normal velocities
 */
R3DAPI Vector3 R3D_SlideCapsuleBoxSwept(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide sphere along box collision (swept/continuous)
 *
 * Uses swept collision to prevent tunneling at any velocity.
 * Recommended for projectiles and fast-moving spherical objects.
 *
 * @note Slightly slower than discrete sphere/box.
 *
 * @param center Sphere center
 * @param radius Sphere radius
 * @param velocity Desired movement velocity (any magnitude)
 * @param box Obstacle box
 * @param outNormal Optional: receives collision normal
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideSphereBox Faster version for normal velocities
 */
R3DAPI Vector3 R3D_SlideSphereBoxSwept(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal);

/**
 * @brief Slide box along another box collision (swept/continuous)
 *
 * Uses swept collision to prevent tunneling at any velocity.
 *
 * @note Discrete box/box is extremely fast and should be preferred.
 *       Swept version is rarely necessary except for very high velocities.
 *
 * @param box Moving box
 * @param velocity Desired movement velocity (any magnitude)
 * @param obstacle Obstacle box
 * @param outNormal Optional: receives collision normal
 * @return Adjusted velocity that slides along surface
 *
 * @see R3D_SlideBoxBox Faster version (recommended for most cases)
 */
R3DAPI Vector3 R3D_SlideBoxBoxSwept(BoundingBox box, Vector3 velocity, BoundingBox obstacle, Vector3* outNormal);

/**
 * @brief Push capsule out of box if penetrating
 * @param capsule Capsule shape (modified in place if penetrating)
 * @param box Obstacle box
 * @param outPenetration Optional: receives penetration depth
 * @return true if depenetration occurred
 */
R3DAPI bool R3D_DepenetrateCapsuleBox(R3D_Capsule* capsule, BoundingBox box, float* outPenetration);

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
 * @brief Push box out of box if penetrating
 * @param box Moving box (modified in place if penetrating)
 * @param obstacle Obstacle box
 * @param outPenetration Optional: receives penetration depth
 * @return true if depenetration occurred
 */
R3DAPI bool R3D_DepenetrateBoxBox(BoundingBox* box, BoundingBox obstacle, float* outPenetration);

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
 * @brief Sweep capsule along velocity vector
 * @param capsule Capsule shape to sweep
 * @param velocity Movement vector (direction and magnitude)
 * @param box Obstacle bounding box
 * @return Sweep collision info (hit, distance, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box);

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
 * @brief Sweep box along velocity vector
 * @param box Moving bounding box
 * @param velocity Movement vector (direction and magnitude)
 * @param obstacle Static obstacle box
 * @return Sweep collision info (hit, distance, point, normal)
 */
R3DAPI R3D_SweepCollision R3D_SweepBoxBox(BoundingBox box, Vector3 velocity, BoundingBox obstacle);

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
 * @brief Check if box is grounded against a box
 * @param box Box to test
 * @param checkDistance How far below to check
 * @param ground Ground box to test against
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsBoxGroundedBox(BoundingBox box, float checkDistance, BoundingBox ground, RayCollision* outGround);

/**
 * @brief Check if box is grounded against mesh geometry
 * @param box Box to test
 * @param checkDistance How far below to check
 * @param mesh Mesh data to test against
 * @param transform Mesh world transform
 * @param outGround Optional: receives raycast hit info
 * @return true if grounded within checkDistance
 */
R3DAPI bool R3D_IsBoxGroundedMesh(BoundingBox box, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision* outGround);

/**
 * @brief Find closest point on line segment to given point
 * @param point Query point
 * @param start Segment start
 * @param end Segment end
 * @return Closest point on segment [start, end]
 */
R3DAPI Vector3 R3D_ClosestPointOnSegment(Vector3 point, Vector3 start, Vector3 end);

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
