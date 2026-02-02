/* r3d_kinematics.c -- R3D Kinematics Module.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_kinematics.h>
#include <raymath.h>
#include <stddef.h>
#include <float.h>

#include "./common/r3d_math.h"

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

bool R3D_CheckCollisionCapsuleMesh(R3D_Capsule capsule, R3D_MeshData mesh, Matrix transform)
{
    Vector3 axis = Vector3Subtract(capsule.end, capsule.start);
    float radiusSq = capsule.radius * capsule.radius;

    bool useIndices = (mesh.indices != NULL);
    int triangleCount = useIndices ? (mesh.indexCount / 3) : (mesh.vertexCount / 3);

    for (int i = 0; i < triangleCount; i++)
    {
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

        Vector3 a = r3d_vector3_transform(mesh.vertices[mesh.indices[i]].position, &transform);
        Vector3 b = r3d_vector3_transform(mesh.vertices[mesh.indices[i+1]].position, &transform);
        Vector3 c = r3d_vector3_transform(mesh.vertices[mesh.indices[i+2]].position, &transform);

        const int samples = 5;
        for (int s = 0; s < samples; s++)
        {
            float t = (float)s / (samples - 1);
            Vector3 p = Vector3Add(capsule.start, Vector3Scale(axis, t));

            Vector3 closest = R3D_ClosestPointOnTriangle(p, a, b, c);
            if (Vector3LengthSqr(Vector3Subtract(closest, p)) <= radiusSq) {
                return true;
            }
        }
    }

    return false;
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

Vector3 R3D_SlideSphereBox(Vector3 center, float radius, Vector3 velocity, BoundingBox box, Vector3* outNormal)
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

Vector3 R3D_SlideSphereMesh(Vector3 center, float radius, Vector3 velocity, R3D_MeshData mesh, Matrix transform, Vector3* outNormal)
{
    R3D_SweepCollision collision = R3D_SweepSphereMesh(center, radius, velocity, mesh, transform);
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

Vector3 R3D_SlideCapsuleBox(R3D_Capsule capsule, Vector3 velocity, BoundingBox box, Vector3* outNormal)
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

Vector3 R3D_SlideCapsuleMesh(R3D_Capsule capsule, Vector3 velocity, R3D_MeshData mesh, Matrix transform, Vector3* outNormal)
{
    R3D_SweepCollision collision = R3D_SweepCapsuleMesh(capsule, velocity, mesh, transform);
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

    Vector3 localOrigin = r3d_vector3_transform(ray.position, &invTransform);
    Vector3 localDirection = Vector3Normalize(r3d_vector3_transform_normal(ray.direction, &invTransform));

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
            Vector3 hitWorld = r3d_vector3_transform(hitLocal, &transform);
            float distance = Vector3Distance(ray.position, hitWorld);

            if (distance < collision.distance)
            {
                collision.hit = true;
                collision.distance = distance;
                collision.point = hitWorld;

                Vector3 normalLocal = Vector3Normalize(Vector3CrossProduct(edge1, edge2));
                collision.normal = Vector3Normalize(r3d_vector3_transform_normal(normalLocal, &normalMatrix));
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

R3D_SweepCollision R3D_SweepSpherePoint(Vector3 center, float radius, Vector3 velocity, Vector3 point)
{
    R3D_SweepCollision result = {0};
    
    Vector3 m = Vector3Subtract(center, point);
    float c = Vector3DotProduct(m, m) - radius * radius;

    if (c <= 0.0f) {
        result.hit = true;
        result.time = 0.0f;
        result.point = Vector3Add(point, Vector3Scale(Vector3Normalize(m), radius));
        result.normal = Vector3Normalize(m);
        return result;
    }

    float a = Vector3DotProduct(velocity, velocity);
    float b = Vector3DotProduct(m, velocity);
    float discr = b * b - a * c;
    
    if (discr < 0.0f) return result;

    float t = (-b - sqrtf(discr)) / a;
    if (t < 0.0f || t > 1.0f) return result;

    Vector3 hit = Vector3Add(center, Vector3Scale(velocity, t));
    
    result.hit = true;
    result.time = t;
    result.point = hit;
    result.normal = Vector3Normalize(Vector3Subtract(hit, point));
    return result;
}

R3D_SweepCollision R3D_SweepSphereSegment(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b)
{
    R3D_SweepCollision result = {0};
    
    Vector3 d = Vector3Subtract(b, a);
    Vector3 m = Vector3Subtract(center, a);

    float dd = Vector3DotProduct(d, d);
    float md = Vector3DotProduct(m, d);
    float nd = Vector3DotProduct(velocity, d);

    float a0 = dd * Vector3DotProduct(velocity, velocity) - nd * nd;
    float b0 = dd * Vector3DotProduct(m, velocity) - md * nd;
    float c0 = dd * (Vector3DotProduct(m, m) - radius * radius) - md * md;

    if (fabsf(a0) < 1e-8f) return result;

    float discr = b0 * b0 - a0 * c0;
    if (discr < 0.0f) return result;

    float t = (-b0 - sqrtf(discr)) / a0;
    if (t < 0.0f || t > 1.0f) return result;

    float s = (md + t * nd) / dd;
    if (s < 0.0f || s > 1.0f) return result;

    Vector3 hit = Vector3Add(center, Vector3Scale(velocity, t));
    Vector3 closest = Vector3Add(a, Vector3Scale(d, s));

    result.hit = true;
    result.time = t;
    result.point = hit;
    result.normal = Vector3Normalize(Vector3Subtract(hit, closest));

    return result;
}

R3D_SweepCollision R3D_SweepSphereTrianglePlane(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b, Vector3 c)
{
    R3D_SweepCollision result = {0};
    
    Vector3 ab = Vector3Subtract(b, a);
    Vector3 ac = Vector3Subtract(c, a);
    Vector3 normal = Vector3Normalize(Vector3CrossProduct(ab, ac));

    float dist = Vector3DotProduct(Vector3Subtract(center, a), normal);
    float denom = Vector3DotProduct(velocity, normal);

    // Moving away or parallel
    if (denom >= 0.0f) return result;

    float t = (radius - dist) / denom;
    if (t < 0.0f || t > 1.0f) return result;

    Vector3 hitPoint = Vector3Add(center, Vector3Scale(velocity, t));
    Vector3 projected = Vector3Subtract(hitPoint, Vector3Scale(normal, radius));

    Vector3 closest = R3D_ClosestPointOnTriangle(projected, a, b, c);
    float d2 = Vector3LengthSqr(Vector3Subtract(projected, closest));

    if (d2 > 1e-6f) return result;

    result.hit = true;
    result.time = t;
    result.point = hitPoint;
    result.normal = normal;

    return result;
}

R3D_SweepCollision R3D_SweepSphereTriangle(Vector3 center, float radius, Vector3 velocity, Vector3 a, Vector3 b, Vector3 c)
{
    R3D_SweepCollision result = {0};
    result.time = 1.0f;

    R3D_SweepCollision faceHit = R3D_SweepSphereTrianglePlane(center, radius, velocity, a, b, c);
    if (faceHit.hit && faceHit.time < result.time) {
        result = faceHit;
    }

    Vector3 edges[3][2] = {{a, b}, {b, c}, {c, a}};
    for (int i = 0; i < 3; i++) {
        R3D_SweepCollision edgeHit = R3D_SweepSphereSegment(center, radius, velocity, edges[i][0], edges[i][1]);
        if (edgeHit.hit && edgeHit.time < result.time) {
            result = edgeHit;
        }
    }

    Vector3 verts[3] = {a, b, c};
    for (int i = 0; i < 3; i++) {
        R3D_SweepCollision vertHit = R3D_SweepSpherePoint(center, radius, velocity, verts[i]);
        if (vertHit.hit && vertHit.time < result.time) {
            result = vertHit;
        }
    }

    return result;
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

R3D_SweepCollision R3D_SweepSphereMesh(Vector3 center, float radius, Vector3 velocity, R3D_MeshData mesh, Matrix transform)
{
    R3D_SweepCollision result = {0};
    result.time = 1.0f;

    bool useIndices = (mesh.indices != NULL);
    int triangleCount = useIndices ? (mesh.indexCount / 3) : (mesh.vertexCount / 3);

    for (int i = 0; i < triangleCount; i++)
    {
        Vector3 v0, v1, v2;

        if (useIndices) {
            v0 = mesh.vertices[mesh.indices[i * 3    ]].position;
            v1 = mesh.vertices[mesh.indices[i * 3 + 1]].position;
            v2 = mesh.vertices[mesh.indices[i * 3 + 2]].position;
        }
        else {
            v0 = mesh.vertices[i * 3    ].position;
            v1 = mesh.vertices[i * 3 + 1].position;
            v2 = mesh.vertices[i * 3 + 2].position;
        }

        Vector3 a = r3d_vector3_transform(v0, &transform);
        Vector3 b = r3d_vector3_transform(v1, &transform);
        Vector3 c = r3d_vector3_transform(v2, &transform);

        R3D_SweepCollision hit = R3D_SweepSphereTriangle(center, radius, velocity, a, b, c);
        if (hit.hit && hit.time < result.time) result = hit;
    }

    return result;
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

R3D_SweepCollision R3D_SweepCapsuleMesh(R3D_Capsule capsule, Vector3 velocity, R3D_MeshData mesh, Matrix transform)
{
    R3D_SweepCollision result = {0};
    result.time = 1.0f;

    for (int i = 0; i < mesh.indexCount; i += 3)
    {
        Vector3 a = r3d_vector3_transform(mesh.vertices[mesh.indices[i]].position, &transform);
        Vector3 b = r3d_vector3_transform(mesh.vertices[mesh.indices[i+1]].position, &transform);
        Vector3 c = r3d_vector3_transform(mesh.vertices[mesh.indices[i+2]].position, &transform);

        // Face plane test
        R3D_SweepCollision faceHit = R3D_SweepSphereTrianglePlane(capsule.start, capsule.radius, velocity, a, b, c);
        if (faceHit.hit && faceHit.time < result.time) result = faceHit;

        faceHit = R3D_SweepSphereTrianglePlane(capsule.end, capsule.radius, velocity, a, b, c);
        if (faceHit.hit && faceHit.time < result.time) result = faceHit;

        // Segment (cylindre)
        R3D_SweepCollision segHit = R3D_SweepSphereSegment(capsule.start, capsule.radius, velocity, a, b);
        if (segHit.hit && segHit.time < result.time) result = segHit;

        segHit = R3D_SweepSphereSegment(capsule.start, capsule.radius, velocity, b, c);
        if (segHit.hit && segHit.time < result.time) result = segHit;

        segHit = R3D_SweepSphereSegment(capsule.start, capsule.radius, velocity, c, a);
        if (segHit.hit && segHit.time < result.time) result = segHit;

        // Vertices (start)
        R3D_SweepCollision vertHit = R3D_SweepSpherePoint(capsule.start, capsule.radius, velocity, a);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;

        vertHit = R3D_SweepSpherePoint(capsule.start, capsule.radius, velocity, b);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;

        vertHit = R3D_SweepSpherePoint(capsule.start, capsule.radius, velocity, c);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;

        // Vertices (end)
        vertHit = R3D_SweepSpherePoint(capsule.end, capsule.radius, velocity, a);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;

        vertHit = R3D_SweepSpherePoint(capsule.end, capsule.radius, velocity, b);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;

        vertHit = R3D_SweepSpherePoint(capsule.end, capsule.radius, velocity, c);
        if (vertHit.hit && vertHit.time < result.time) result = vertHit;
    }

    return result;
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

Vector3 R3D_ClosestPointOnSegment(Vector3 point, Vector3 start, Vector3 end)
{
    Vector3 dir = Vector3Subtract(end, start);
    float lenSq = Vector3LengthSqr(dir);

    if (lenSq < 1e-10f) return start;

    float t = Vector3DotProduct(Vector3Subtract(point, start), dir) / lenSq;
    t = fmaxf(0.0f, fminf(t, 1.0f));

    return Vector3Add(start, Vector3Scale(dir, t));
}

Vector3 R3D_ClosestPointOnTriangle(Vector3 p, Vector3 a, Vector3 b, Vector3 c)
{
    Vector3 ab = Vector3Subtract(b, a);
    Vector3 ac = Vector3Subtract(c, a);
    Vector3 ap = Vector3Subtract(p, a);

    float d1 = Vector3DotProduct(ab, ap);
    float d2 = Vector3DotProduct(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return a;

    Vector3 bp = Vector3Subtract(p, b);
    float d3 = Vector3DotProduct(ab, bp);
    float d4 = Vector3DotProduct(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return b;

    Vector3 cp = Vector3Subtract(p, c);
    float d5 = Vector3DotProduct(ab, cp);
    float d6 = Vector3DotProduct(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return c;

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return Vector3Add(a, Vector3Scale(ab, v));
    }
        
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float v = d2 / (d2 - d6);
        return Vector3Add(a, Vector3Scale(ac, v));
    }
        
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return Vector3Add(b, Vector3Scale(Vector3Subtract(c, b), v));
    }

    float denom = 1.f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;

    return Vector3Add(a, Vector3Add(Vector3Scale(ab, v), Vector3Scale(ac, w)));
}

Vector3 R3D_ClosestPointOnBox(Vector3 point, BoundingBox box)
{
    Vector3 closest;
    closest.x = fmaxf(box.min.x, fminf(point.x, box.max.x));
    closest.y = fmaxf(box.min.y, fminf(point.y, box.max.y));
    closest.z = fmaxf(box.min.z, fminf(point.z, box.max.z));
    return closest;
}
