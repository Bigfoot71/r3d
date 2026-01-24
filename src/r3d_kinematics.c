/* r3d_kinematics.c -- R3D Kinematics Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_kinematics.h>
#include <raymath.h>
#include <stddef.h>
#include <float.h>

// ========================================
// PUBLIC API
// ========================================

bool R3D_CheckCollisionCapsuleBox(R3D_Capsule capsule, BoundingBox box)
{
    Vector3 closestOnSegment = R3D_ClosestPointOnSegment(
        R3D_ClosestPointOnBox(capsule.start, box),
        capsule.start,
        capsule.end
    );

    Vector3 closestOnBox = R3D_ClosestPointOnBox(closestOnSegment, box);
    float distSq = Vector3DistanceSqr(closestOnBox, closestOnSegment);

    return distSq <= (capsule.radius * capsule.radius);
}

bool R3D_CheckCollisionCapsuleSphere(R3D_Capsule capsule, Vector3 center, float radius)
{
    Vector3 closestPoint = R3D_ClosestPointOnSegment(center, capsule.start, capsule.end);
    float distSq = Vector3DistanceSqr(center, closestPoint);
    float radiusSum = capsule.radius + radius;

    return distSq <= (radiusSum * radiusSum);
}

bool R3D_CheckCollisionCapsules(R3D_Capsule a, R3D_Capsule b)
{
    Vector3 dirA = Vector3Subtract(a.end, a.start);
    Vector3 dirB = Vector3Subtract(b.end, b.start);
    Vector3 r = Vector3Subtract(a.start, b.start);

    float dotAA = Vector3DotProduct(dirA, dirA);
    float dotBB = Vector3DotProduct(dirB, dirB);
    float dotAB = Vector3DotProduct(dirA, dirB);
    float dotAR = Vector3DotProduct(dirA, r);
    float dotBR = Vector3DotProduct(dirB, r);

    float denom = dotAA * dotBB - dotAB * dotAB;
    float s = 0.0f, t = 0.0f;

    if (denom > 1e-6f) {
        s = (dotAB * dotBR - dotBB * dotAR) / denom;
        s = fmaxf(0.0f, fminf(1.0f, s));
        t = (dotAB * s + dotBR) / dotBB;

        if (t < 0.0f) {
            t = 0.0f;
            s = fmaxf(0.0f, fminf(1.0f, -dotAR / dotAA));
        }
        else if (t > 1.0f) {
            t = 1.0f;
            s = fmaxf(0.0f, fminf(1.0f, (dotAB - dotAR) / dotAA));
        }
    }
    else {
        s = 0.5f;
        t = fmaxf(0.0f, fminf(1.0f, dotBR / dotBB));
    }

    Vector3 closestA = Vector3Add(a.start, Vector3Scale(dirA, s));
    Vector3 closestB = Vector3Add(b.start, Vector3Scale(dirB, t));

    float distSq = Vector3DistanceSqr(closestA, closestB);
    float radiusSum = a.radius + b.radius;

    return distSq <= (radiusSum * radiusSum);
}

R3D_Penetration R3D_CheckPenetrationCapsuleBox(R3D_Capsule capsule, BoundingBox box)
{
    R3D_Penetration result = {0};

    Vector3 closestOnSegment = R3D_ClosestPointOnSegment(
        R3D_ClosestPointOnBox(capsule.start, box),
        capsule.start,
        capsule.end
    );

    Vector3 closestOnBox = R3D_ClosestPointOnBox(closestOnSegment, box);
    Vector3 delta = Vector3Subtract(closestOnSegment, closestOnBox);

    float distSq = Vector3LengthSqr(delta);
    float radiusSq = capsule.radius * capsule.radius;

    if (distSq >= radiusSq) return result;

    float dist = sqrtf(distSq);
    result.collides = true;
    result.depth = capsule.radius - dist;

    if (dist > 1e-6f) {
        result.normal = Vector3Scale(delta, 1.0f / dist);
    }
    else {
        Vector3 boxCenter = {
            (box.min.x + box.max.x) * 0.5f,
            (box.min.y + box.max.y) * 0.5f,
            (box.min.z + box.max.z) * 0.5f
        };
        Vector3 toCenter = Vector3Subtract(closestOnSegment, boxCenter);

        float ax = fabsf(toCenter.x);
        float ay = fabsf(toCenter.y);
        float az = fabsf(toCenter.z);

        if (ax >= ay && ax >= az) {
            result.normal = (Vector3){toCenter.x > 0 ? 1.0f : -1.0f, 0, 0};
        }
        else if (ay >= az) {
            result.normal = (Vector3){0, toCenter.y > 0 ? 1.0f : -1.0f, 0};
        }
        else {
            result.normal = (Vector3){0, 0, toCenter.z > 0 ? 1.0f : -1.0f};
        }
    }
    
    result.mtv = Vector3Scale(result.normal, result.depth);
    return result;
}

R3D_Penetration R3D_CheckPenetrationCapsuleSphere(R3D_Capsule capsule, Vector3 center, float radius)
{
    R3D_Penetration result = {0};

    Vector3 closestOnSegment = R3D_ClosestPointOnSegment(center, capsule.start, capsule.end);
    Vector3 delta = Vector3Subtract(center, closestOnSegment);

    float distSq = Vector3LengthSqr(delta);
    float combinedRadius = capsule.radius + radius;
    float combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq >= combinedRadiusSq) return result;

    float dist = sqrtf(distSq);
    result.collides = true;
    result.depth = combinedRadius - dist;

    if (dist > 1e-6f) {
        result.normal = Vector3Scale(delta, 1.0f / dist);
    }
    else {
        Vector3 capsuleDir = Vector3Subtract(capsule.end, capsule.start);
        float capsuleLengthSq = Vector3LengthSqr(capsuleDir);
        
        if (capsuleLengthSq > 1e-6f) {
            result.normal = (Vector3){capsuleDir.y, -capsuleDir.x, 0};
            float normalLengthSq = Vector3LengthSqr(result.normal);

            if (normalLengthSq < 1e-6f) {
                result.normal = (Vector3){0, capsuleDir.z, -capsuleDir.y};
                normalLengthSq = Vector3LengthSqr(result.normal);
            }

            if (normalLengthSq > 1e-6f) {
                result.normal = Vector3Normalize(result.normal);
            }
            else {
                result.normal = (Vector3){0, 1, 0};
            }
        }
        else {
            result.normal = (Vector3){0, 1, 0};
        }
    }

    result.mtv = Vector3Scale(result.normal, result.depth);
    return result;
}

R3D_Penetration R3D_CheckPenetrationCapsules(R3D_Capsule a, R3D_Capsule b)
{
    R3D_Penetration result = {0};

    Vector3 dirA = Vector3Subtract(a.end, a.start);
    Vector3 dirB = Vector3Subtract(b.end, b.start);
    Vector3 r = Vector3Subtract(a.start, b.start);

    float dotAA = Vector3DotProduct(dirA, dirA);
    float dotBB = Vector3DotProduct(dirB, dirB);
    float dotAB = Vector3DotProduct(dirA, dirB);
    float dotAR = Vector3DotProduct(dirA, r);
    float dotBR = Vector3DotProduct(dirB, r);

    float denom = dotAA * dotBB - dotAB * dotAB;

    float s = 0.0f, t = 0.0f;
    
    if (denom > 1e-6f) {
        s = (dotAB * dotBR - dotBB * dotAR) / denom;
        s = fmaxf(0.0f, fminf(1.0f, s));
        t = (dotAB * s + dotBR) / dotBB;

        if (t < 0.0f) {
            t = 0.0f;
            s = fmaxf(0.0f, fminf(1.0f, -dotAR / dotAA));
        }
        else if (t > 1.0f) {
            t = 1.0f;
            s = fmaxf(0.0f, fminf(1.0f, (dotAB - dotAR) / dotAA));
        }
    }
    else {
        s = 0.5f;
        t = fmaxf(0.0f, fminf(1.0f, dotBR / dotBB));
    }

    Vector3 closestA = Vector3Add(a.start, Vector3Scale(dirA, s));
    Vector3 closestB = Vector3Add(b.start, Vector3Scale(dirB, t));

    Vector3 delta = Vector3Subtract(closestA, closestB);
    float distSq = Vector3LengthSqr(delta);
    float combinedRadius = a.radius + b.radius;
    float combinedRadiusSq = combinedRadius * combinedRadius;

    if (distSq >= combinedRadiusSq) return result;

    float dist = sqrtf(distSq);
    result.collides = true;
    result.depth = combinedRadius - dist;

    if (dist > 1e-6f) {
        result.normal = Vector3Scale(delta, 1.0f / dist);
    }
    else {
        Vector3 cross = Vector3CrossProduct(dirA, dirB);
        float crossLengthSq = Vector3LengthSqr(cross);

        if (crossLengthSq > 1e-6f) {
            result.normal = Vector3Normalize(cross);
        }
        else {
            Vector3 perp = (Vector3){dirA.y, -dirA.x, 0};
            float perpLengthSq = Vector3LengthSqr(perp);

            if (perpLengthSq < 1e-6f) {
                perp = (Vector3){0, dirA.z, -dirA.y};
                perpLengthSq = Vector3LengthSqr(perp);
            }

            if (perpLengthSq > 1e-6f) {
                result.normal = Vector3Normalize(perp);
            }
            else {
                result.normal = (Vector3){0, 1, 0};
            }
        }
    }

    result.mtv = Vector3Scale(result.normal, result.depth);
    return result;
}

Vector3 R3D_SlideVelocity(Vector3 velocity, Vector3 normal)
{
    float dot = Vector3DotProduct(velocity, normal);
    return Vector3Subtract(velocity, Vector3Scale(normal, dot));
}

Vector3 R3D_BounceVelocity(Vector3 velocity, Vector3 normal, float bounciness)
{
    float dot = Vector3DotProduct(velocity, normal);
    Vector3 reflection = Vector3Subtract(velocity, Vector3Scale(normal, 2.0f * dot));
    return Vector3Scale(reflection, bounciness);
}

Vector3 R3D_SlideCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal)
{
    R3D_Capsule moved = {
        Vector3Add(capsule.start, velocity),
        Vector3Add(capsule.end, velocity),
        capsule.radius
    };

    if (!R3D_CheckCollisionCapsuleBox(moved, box)) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    Vector3 boxCenter = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    Vector3 pointOnCapsule = R3D_ClosestPointOnSegment(boxCenter, capsule.start, capsule.end);
    Vector3 closestOnBox = R3D_ClosestPointOnBox(pointOnCapsule, box);
    Vector3 penetration = Vector3Subtract(pointOnCapsule, closestOnBox);
    float penetrationDist = Vector3Length(penetration);

    if (penetrationDist < 1e-5f) {
        if (outNormal) *outNormal = Vector3Normalize(Vector3Negate(velocity));
        return (Vector3){0, 0, 0};
    }

    Vector3 normal = Vector3Scale(penetration, 1.0f / penetrationDist);
    if (outNormal) *outNormal = normal;

    return R3D_SlideVelocity(velocity, normal);
}

Vector3 R3D_SlideSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal)
{
    Vector3 newCenter = Vector3Add(center, velocity);

    if (!CheckCollisionBoxSphere(box, newCenter, radius)) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    Vector3 closestOnBox = R3D_ClosestPointOnBox(center, box);
    Vector3 penetration = Vector3Subtract(center, closestOnBox);
    float penetrationDist = Vector3Length(penetration);

    if (penetrationDist < 1e-5f) {
        if (outNormal) *outNormal = Vector3Normalize(Vector3Negate(velocity));
        return (Vector3){0, 0, 0};
    }

    Vector3 normal = Vector3Scale(penetration, 1.0f / penetrationDist);
    if (outNormal) *outNormal = normal;

    return R3D_SlideVelocity(velocity, normal);
}

Vector3 R3D_SlideBoxBox(BoundingBox box, Vector3 velocity, BoundingBox obstacle, Vector3* outNormal)
{
    BoundingBox moved = {
        Vector3Add(box.min, velocity),
        Vector3Add(box.max, velocity)
    };

    if (!CheckCollisionBoxes(moved, obstacle)) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    Vector3 boxCenter = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    Vector3 obstacleCenter = {
        (obstacle.min.x + obstacle.max.x) * 0.5f,
        (obstacle.min.y + obstacle.max.y) * 0.5f,
        (obstacle.min.z + obstacle.max.z) * 0.5f
    };

    Vector3 delta = Vector3Subtract(boxCenter, obstacleCenter);

    Vector3 boxHalf = {
        (box.max.x - box.min.x) * 0.5f,
        (box.max.y - box.min.y) * 0.5f,
        (box.max.z - box.min.z) * 0.5f
    };

    Vector3 obstacleHalf = {
        (obstacle.max.x - obstacle.min.x) * 0.5f,
        (obstacle.max.y - obstacle.min.y) * 0.5f,
        (obstacle.max.z - obstacle.min.z) * 0.5f
    };

    float overlapX = (boxHalf.x + obstacleHalf.x) - fabsf(delta.x);
    float overlapY = (boxHalf.y + obstacleHalf.y) - fabsf(delta.y);
    float overlapZ = (boxHalf.z + obstacleHalf.z) - fabsf(delta.z);

    Vector3 normal = {0, 0, 0};

    if (overlapX < overlapY && overlapX < overlapZ) {
        normal.x = (delta.x > 0) ? 1.0f : -1.0f;
    }
    else if (overlapY < overlapZ) {
        normal.y = (delta.y > 0) ? 1.0f : -1.0f;
    }
    else {
        normal.z = (delta.z > 0) ? 1.0f : -1.0f;
    }

    if (outNormal) *outNormal = normal;

    return R3D_SlideVelocity(velocity, normal);
}

Vector3 R3D_SlideCapsuleBoxSwept(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal)
{
    R3D_SweepCollision collision = R3D_SweepCapsuleBox(capsule, velocity, box);
    if (!collision.hit) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    if (outNormal) *outNormal = collision.normal;

    float safeTime = fmaxf(0.0f, collision.time - 0.001f);
    Vector3 safeVelocity = Vector3Scale(velocity, safeTime);
    Vector3 remainingVelocity = Vector3Scale(velocity, 1.0f - safeTime);
    Vector3 slidedRemaining = R3D_SlideVelocity(remainingVelocity, collision.normal);

    return Vector3Add(safeVelocity, slidedRemaining);
}

Vector3 R3D_SlideSphereBoxSwept(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal)
{
    R3D_SweepCollision collision = R3D_SweepSphereBox(center, radius, velocity, box);
    if (!collision.hit) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    if (outNormal) *outNormal = collision.normal;

    float safeTime = fmaxf(0.0f, collision.time - 0.001f);
    Vector3 safeVelocity = Vector3Scale(velocity, safeTime);
    Vector3 remainingVelocity = Vector3Scale(velocity, 1.0f - safeTime);
    Vector3 slidedRemaining = R3D_SlideVelocity(remainingVelocity, collision.normal);

    return Vector3Add(safeVelocity, slidedRemaining);
}

Vector3 R3D_SlideBoxBoxSwept(BoundingBox box, Vector3 velocity, BoundingBox obstacle, Vector3* outNormal)
{
    R3D_SweepCollision collision = R3D_SweepBoxBox(box, velocity, obstacle);
    if (!collision.hit) {
        if (outNormal) *outNormal = (Vector3){0, 0, 0};
        return velocity;
    }

    if (outNormal) *outNormal = collision.normal;

    float safeTime = fmaxf(0.0f, collision.time - 0.001f);
    Vector3 safeVelocity = Vector3Scale(velocity, safeTime);
    Vector3 remainingVelocity = Vector3Scale(velocity, 1.0f - safeTime);
    Vector3 slidedRemaining = R3D_SlideVelocity(remainingVelocity, collision.normal);

    return Vector3Add(safeVelocity, slidedRemaining);
}

bool R3D_DepenetrateCapsuleBox(R3D_Capsule* capsule, BoundingBox box, float* outPenetration)
{
    Vector3 closestOnSegment = R3D_ClosestPointOnSegment(
        R3D_ClosestPointOnBox(capsule->start, box),
        capsule->start,
        capsule->end
    );

    Vector3 closestOnBox = R3D_ClosestPointOnBox(closestOnSegment, box);
    Vector3 delta = Vector3Subtract(closestOnSegment, closestOnBox);
    float distSq = Vector3LengthSqr(delta);
    float radiusSq = capsule->radius * capsule->radius;

    if (distSq >= radiusSq) return false;

    float dist = sqrtf(distSq);
    float penetration = capsule->radius - dist;

    Vector3 direction = dist > 1e-6f ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    Vector3 correction = Vector3Scale(direction, penetration);

    capsule->start = Vector3Add(capsule->start, correction);
    capsule->end = Vector3Add(capsule->end, correction);

    if (outPenetration) *outPenetration = penetration;
    return true;
}

bool R3D_DepenetrateSphereBox(Vector3* center, float radius, BoundingBox box, float* outPenetration)
{
    Vector3 closestPoint = R3D_ClosestPointOnBox(*center, box);
    Vector3 delta = Vector3Subtract(*center, closestPoint);
    float distSq = Vector3LengthSqr(delta);
    float radiusSq = radius * radius;

    if (distSq >= radiusSq) return false;

    float dist = sqrtf(distSq);
    float penetration = radius - dist;

    Vector3 direction = dist > 1e-6f ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    *center = Vector3Add(*center, Vector3Scale(direction, penetration));

    if (outPenetration) *outPenetration = penetration;
    return true;
}

bool R3D_DepenetrateBoxBox(BoundingBox* box, BoundingBox obstacle, float* outPenetration)
{
    Vector3 boxCenter = {
        (box->min.x + box->max.x) * 0.5f,
        (box->min.y + box->max.y) * 0.5f,
        (box->min.z + box->max.z) * 0.5f
    };

    Vector3 obstacleCenter = {
        (obstacle.min.x + obstacle.max.x) * 0.5f,
        (obstacle.min.y + obstacle.max.y) * 0.5f,
        (obstacle.min.z + obstacle.max.z) * 0.5f
    };

    Vector3 boxHalfSize = {
        (box->max.x - box->min.x) * 0.5f,
        (box->max.y - box->min.y) * 0.5f,
        (box->max.z - box->min.z) * 0.5f
    };

    Vector3 obstacleHalfSize = {
        (obstacle.max.x - obstacle.min.x) * 0.5f,
        (obstacle.max.y - obstacle.min.y) * 0.5f,
        (obstacle.max.z - obstacle.min.z) * 0.5f
    };

    Vector3 delta = Vector3Subtract(boxCenter, obstacleCenter);
    Vector3 overlap = {
        (boxHalfSize.x + obstacleHalfSize.x) - fabsf(delta.x),
        (boxHalfSize.y + obstacleHalfSize.y) - fabsf(delta.y),
        (boxHalfSize.z + obstacleHalfSize.z) - fabsf(delta.z)
    };

    if (overlap.x <= 0 || overlap.y <= 0 || overlap.z <= 0) {
        return false;
    }

    Vector3 correction = {0};
    float minOverlap = overlap.x;
    int axis = 0;

    if (overlap.y < minOverlap) minOverlap = overlap.y, axis = 1;
    if (overlap.z < minOverlap) minOverlap = overlap.z, axis = 2;

    if (axis == 0) correction.x = delta.x > 0 ? minOverlap : -minOverlap;
    else if (axis == 1) correction.y = delta.y > 0 ? minOverlap : -minOverlap;
    else correction.z = delta.z > 0 ? minOverlap : -minOverlap;

    box->min = Vector3Add(box->min, correction);
    box->max = Vector3Add(box->max, correction);

    if (outPenetration) *outPenetration = minOverlap;
    return true;
}

RayCollision R3D_RaycastMesh(Ray ray, R3D_MeshData mesh, Matrix transform)
{
    // Möller-Trumbore ray triangle intersection algorithm
    // See: https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

    RayCollision collision = {0};

    collision.hit = false;
    collision.distance = FLT_MAX;

    if (mesh.vertices == NULL) {
        return collision;
    }

    Matrix invTransform = MatrixInvert(transform);
    Matrix normalMatrix = MatrixTranspose(invTransform);

    Vector3 localOrigin = Vector3Transform(ray.position, invTransform);
    Vector3 localDirection = Vector3Normalize(Vector3Transform(ray.direction, invTransform));

    bool useIndices = (mesh.indices != NULL);
    int triangleCount = useIndices ? (mesh.indexCount / 3) : (mesh.vertexCount / 3);

    for (int i = 0; i < triangleCount; i++)
    {
        // Get triangle vertex positions
        Vector3 v0, v1, v2;

        if (useIndices) {
            v0 = mesh.vertices[mesh.indices[i*3    ]].position;
            v1 = mesh.vertices[mesh.indices[i*3 + 1]].position;
            v2 = mesh.vertices[mesh.indices[i*3 + 2]].position;
        }
        else {
            v0 = mesh.vertices[i*3    ].position;
            v1 = mesh.vertices[i*3 + 1].position;
            v2 = mesh.vertices[i*3 + 2].position;
        }

        Vector3 edge1 = Vector3Subtract(v1, v0);
        Vector3 edge2 = Vector3Subtract(v2, v0);
        Vector3 h = Vector3CrossProduct(localDirection, edge2);
        float a = Vector3DotProduct(edge1, h);

        // Ray is parallel to triangle
        if (fabsf(a) < 1e-5f) continue;

        float f = 1.0f / a;

        // Check if intersection point is outside triangle
        Vector3 s = Vector3Subtract(localOrigin, v0);
        float u = f * Vector3DotProduct(s, h);
        if (u < 0.0f || u > 1.0f) continue;

        // Check if intersection point is outside triangle
        Vector3 q = Vector3CrossProduct(s, edge1);
        float v = f * Vector3DotProduct(localDirection, q);
        if (v < 0.0f || u + v > 1.0f) continue;

        float t = f * Vector3DotProduct(edge2, q);

        if (t > 1e-5f)
        {
            Vector3 hitLocal = Vector3Add(localOrigin, Vector3Scale(localDirection, t));
            Vector3 hitWorld = Vector3Transform(hitLocal, transform);
            float distance = Vector3Distance(ray.position, hitWorld);

            if (distance < collision.distance)
            {
                collision.hit = true;
                collision.distance = distance;
                collision.point = hitWorld;

                Vector3 normalLocal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
                collision.normal = Vector3Normalize(Vector3Transform(normalLocal, normalMatrix));
            }
        }
    }

    return collision;
}

RayCollision R3D_RaycastModel(Ray ray, R3D_Model model, Matrix transform)
{
    RayCollision closestHit = {0};

    closestHit.hit = false;
    closestHit.distance = FLT_MAX;

    if (model.meshData == NULL || model.meshCount <= 0) {
        return closestHit;
    }

    for (int i = 0; i < model.meshCount; i++) {
        RayCollision hit = R3D_RaycastMesh(ray, model.meshData[i], transform);
        if (hit.hit && hit.distance < closestHit.distance) closestHit = hit;
    }

    return closestHit;
}

R3D_SweepCollision R3D_SweepCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box)
{
    R3D_SweepCollision collision = {0};

    float velocityLength = Vector3Length(velocity);
    if (velocityLength < 1e-6f) return collision;

    BoundingBox expandedBox = {
        Vector3Subtract(box.min, (Vector3){capsule.radius, capsule.radius, capsule.radius}),
        Vector3Add(box.max, (Vector3){capsule.radius, capsule.radius, capsule.radius})
    };

    Vector3 velocityDir = Vector3Scale(velocity, 1.0f / velocityLength);
    Vector3 capsuleAxis = Vector3Subtract(capsule.end, capsule.start);

    RayCollision bestHit = {0};
    bestHit.distance = FLT_MAX;
    bool foundHit = false;

    const int samples = 3;
    for (int i = 0; i < samples; i++)
    {
        float t = (float)i / (float)(samples - 1);
        Vector3 samplePoint = Vector3Add(capsule.start, Vector3Scale(capsuleAxis, t));

        Ray ray = {samplePoint, velocityDir};
        RayCollision hit = GetRayCollisionBox(ray, expandedBox);

        if (hit.hit && hit.distance <= velocityLength && hit.distance < bestHit.distance) {
            bestHit = hit;
            foundHit = true;
        }
    }

    if (foundHit) {
        collision.hit = true;
        collision.time = bestHit.distance / velocityLength;
        collision.point = bestHit.point;
        collision.normal = bestHit.normal;
    }

    return collision;
}

R3D_SweepCollision R3D_SweepSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box)
{
    R3D_SweepCollision collision = {0};

    float velocityLength = Vector3Length(velocity);
    if (velocityLength < 1e-6f) return collision;

    BoundingBox expandedBox = {
        Vector3Subtract(box.min, (Vector3){radius, radius, radius}),
        Vector3Add(box.max, (Vector3){radius, radius, radius})
    };

    Ray ray = {center, Vector3Scale(velocity, 1.0f / velocityLength)};
    RayCollision hit = GetRayCollisionBox(ray, expandedBox);

    if (hit.hit && hit.distance <= velocityLength) {
        collision.hit = true;
        collision.time = hit.distance / velocityLength;
        collision.point = hit.point;
        collision.normal = hit.normal;
    }

    return collision;
}

R3D_SweepCollision R3D_SweepBoxBox(BoundingBox box, Vector3 velocity, BoundingBox obstacle)
{
    R3D_SweepCollision collision = {0};

    float velocityLength = Vector3Length(velocity);
    if (velocityLength < 1e-6f) return collision;

    Vector3 boxHalfSize = {
        (box.max.x - box.min.x) * 0.5f,
        (box.max.y - box.min.y) * 0.5f,
        (box.max.z - box.min.z) * 0.5f
    };

    Vector3 boxCenter = {
        (box.min.x + box.max.x) * 0.5f,
        (box.min.y + box.max.y) * 0.5f,
        (box.min.z + box.max.z) * 0.5f
    };

    BoundingBox expandedObstacle = {
        Vector3Subtract(obstacle.min, boxHalfSize),
        Vector3Add(obstacle.max, boxHalfSize)
    };

    Ray ray = {boxCenter, Vector3Scale(velocity, 1.0f / velocityLength)};
    RayCollision hit = GetRayCollisionBox(ray, expandedObstacle);

    if (hit.hit && hit.distance <= velocityLength) {
        collision.hit = true;
        collision.time = hit.distance / velocityLength;
        collision.point = hit.point;
        collision.normal = hit.normal;
    }

    return collision;
}

bool R3D_IsCapsuleGroundedBox(R3D_Capsule capsule, float checkDistance, BoundingBox ground, RayCollision *outGround)
{
    Ray ray = {
        .position = capsule.start,
        .direction = {0, -1, 0}
    };

    RayCollision collision = GetRayCollisionBox(ray, ground);
    bool grounded = collision.hit && collision.distance <= (capsule.radius + checkDistance);

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

bool R3D_IsCapsuleGroundedMesh(R3D_Capsule capsule, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision *outGround)
{
    Ray ray = {
        .position = capsule.start,
        .direction = {0, -1, 0}
    };

    RayCollision collision = R3D_RaycastMesh(ray, mesh, transform);
    bool grounded = collision.hit && collision.distance <= (capsule.radius + checkDistance);

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

bool R3D_IsSphereGroundedBox(Vector3 center, float radius, float checkDistance, BoundingBox ground, RayCollision *outGround)
{
    Ray ray = {
        .position = center,
        .direction = {0, -1, 0}
    };

    RayCollision collision = GetRayCollisionBox(ray, ground);
    bool grounded = collision.hit && collision.distance <= (radius + checkDistance);

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

bool R3D_IsSphereGroundedMesh(Vector3 center, float radius, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision *outGround)
{
    Ray ray = {
        .position = center,
        .direction = {0, -1, 0}
    };

    RayCollision collision = R3D_RaycastMesh(ray, mesh, transform);
    bool grounded = collision.hit && collision.distance <= (radius + checkDistance);

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

bool R3D_IsBoxGroundedBox(BoundingBox box, float checkDistance, BoundingBox ground, RayCollision *outGround)
{
    Vector3 boxCenter = {
        (box.min.x + box.max.x) * 0.5f,
        box.min.y,
        (box.min.z + box.max.z) * 0.5f
    };

    Ray ray = {
        .position = boxCenter,
        .direction = {0, -1, 0}
    };

    RayCollision collision = GetRayCollisionBox(ray, ground);
    bool grounded = collision.hit && collision.distance <= checkDistance;

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

bool R3D_IsBoxGroundedMesh(BoundingBox box, float checkDistance, R3D_MeshData mesh, Matrix transform, RayCollision *outGround)
{
    Vector3 boxCenter = {
        (box.min.x + box.max.x) * 0.5f,
        box.min.y,
        (box.min.z + box.max.z) * 0.5f
    };

    Ray ray = {
        .position = boxCenter,
        .direction = {0, -1, 0}
    };

    RayCollision collision = R3D_RaycastMesh(ray, mesh, transform);
    bool grounded = collision.hit && collision.distance <= checkDistance;

    if (outGround) {
        *outGround = collision;
    }

    return grounded;
}

Vector3 R3D_ClosestPointOnSegment(Vector3 point, Vector3 start, Vector3 end)
{
    Vector3 dir = Vector3Subtract(end, start);
    float lenSq = Vector3LengthSqr(dir);

    if (lenSq < 1e-10f) return start;

    float t = Vector3DotProduct(Vector3Subtract(point, start), dir) / lenSq;
    t = fmaxf(0.0f, fminf(t, 1.0f));

    return Vector3Add(start, Vector3Scale(dir, t));
}

Vector3 R3D_ClosestPointOnBox(Vector3 point, BoundingBox box)
{
    Vector3 closest;
    closest.x = fmaxf(box.min.x, fminf(point.x, box.max.x));
    closest.y = fmaxf(box.min.y, fminf(point.y, box.max.y));
    closest.z = fmaxf(box.min.z, fminf(point.z, box.max.z));
    return closest;
}
