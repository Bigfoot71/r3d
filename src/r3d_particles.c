/* r3d_particles.h -- R3D Particles Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include <r3d/r3d_particles.h>

#include <raymath.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#include "./details/r3d_math.h"

/* Helper functions */

static float r3d_randf(void)
{
    static const float INV_65535 = 1.0f / 0xFFFF;
    return (float)GetRandomValue(0, 0xFFFF) * INV_65535;
}

static float r3d_randf_range(float min, float max)
{
    return min + r3d_randf() * (max - min);
}

static float r3d_min3f(float a, float b, float c)
{
    float min = a;
    if (b < min) min = b;
    if (c < min) min = c;
    return min;
}

static float r3d_max3f(float a, float b, float c)
{
    float max = a;
    if (b > max) max = b;
    if (c > max) max = c;
    return max;
}

/* Public functions */

R3D_ParticleSystem R3D_LoadParticleSystem(int maxParticles)
{
    R3D_ParticleSystem system = { 0 };

    system.particles = RL_MALLOC(sizeof(R3D_Particle) * maxParticles);
    system.capacity = maxParticles;
    system.count = 0;

    system.position = (Vector3){ 0, 0, 0 };
    system.gravity = (Vector3){ 0, -9.81f, 0 };

    system.initialScale = Vector3One();
    system.scaleVariance = 0.0f;

    system.initialRotation = Vector3Zero();
    system.rotationVariance = Vector3Zero();

    system.initialColor = WHITE;
    system.colorVariance = BLANK;

    system.initialVelocity = (Vector3){ 0, 0, 0 };
    system.velocityVariance = Vector3Zero();

    system.initialAngularVelocity = Vector3Zero();
    system.angularVelocityVariance = Vector3Zero();

    system.lifetime = 1.0f;
    system.lifetimeVariance = 0.0f;

    system.emissionTimer = 0.0f;
    system.emissionRate = 1.0f;
    system.spreadAngle = 0.0f;

    system.scaleOverLifetime = NULL;
    system.speedOverLifetime = NULL;
    system.opacityOverLifetime = NULL;
    system.angularVelocityOverLifetime = NULL;

    system.aabb = (BoundingBox) {
        .min = (Vector3) { -FLT_MAX, -FLT_MAX, -FLT_MAX },
        .max = (Vector3) { +FLT_MAX, +FLT_MAX, +FLT_MAX }
    };

    system.autoEmission = true;

    return system;
}

void R3D_UnloadParticleSystem(R3D_ParticleSystem* system)
{
    if (system) {
        RL_FREE(system->particles);
    }
}

bool R3D_EmitParticle(R3D_ParticleSystem* system)
{
    if (system->count >= system->capacity) {
        return false;
    }

    // Normalize the initial direction
    Vector3 direction = Vector3Normalize(system->initialVelocity);

    // Generate random angles
    float elevation = r3d_randf_range(0, system->spreadAngle * DEG2RAD);
    float azimuth = r3d_randf_range(0, 2.0f * PI);

    // Precompute trigonometric values for the cone
    float cosElevation = cosf(elevation);
    float sinElevation = sqrtf(1.0f - cosElevation * cosElevation); // Use the trigonometric identity
    float cosAzimuth = cosf(azimuth);
    float sinAzimuth = sinf(azimuth);

    // Calculate the vector within the cone (local coordinate system)
    Vector3 spreadDirection = {
        sinElevation * cosAzimuth,
        sinElevation * sinAzimuth,
        cosElevation
    };

    // Generate the local basis around 'direction'
    Vector3 arbitraryAxis = (fabsf(direction.y) > 0.9999f)
        ? (Vector3) { 0.0f, 0.0f, 1.0f }
        : (Vector3) { 1.0f, 0.0f, 0.0f };

    Vector3 binormal = Vector3Normalize(Vector3CrossProduct(arbitraryAxis, direction));
    Vector3 normal = Vector3CrossProduct(direction, binormal);

    // Transform 'spreadDirection' to the global coordinate system
    Vector3 velocity = {
        spreadDirection.x * binormal.x + spreadDirection.y * normal.x + spreadDirection.z * direction.x,
        spreadDirection.x * binormal.y + spreadDirection.y * normal.y + spreadDirection.z * direction.y,
        spreadDirection.x * binormal.z + spreadDirection.y * normal.z + spreadDirection.z * direction.z
    };

    // Scale the final velocity
    velocity = Vector3Scale(velocity, Vector3Length(system->initialVelocity));

    // Initialize particle
    R3D_Particle particle = { 0 };

    particle.lifetime = system->lifetime + r3d_randf_range(-system->lifetimeVariance, system->lifetimeVariance);

    particle.position = system->position;

    particle.rotation = (Vector3){
        (system->initialRotation.x + r3d_randf_range(-system->rotationVariance.x, system->rotationVariance.x)) * DEG2RAD,
        (system->initialRotation.y + r3d_randf_range(-system->rotationVariance.y, system->rotationVariance.y)) * DEG2RAD,
        (system->initialRotation.z + r3d_randf_range(-system->rotationVariance.z, system->rotationVariance.z)) * DEG2RAD
    };

    particle.scale = particle.baseScale = Vector3AddValue(
        system->initialScale, r3d_randf_range(-system->scaleVariance, system->scaleVariance)
    );

    particle.transform = r3d_matrix_scale_rotxyz_translate(
        particle.scale, particle.rotation, particle.position
    );

    particle.velocity = particle.baseVelocity = (Vector3){
        velocity.x + r3d_randf_range(-system->velocityVariance.x, system->velocityVariance.x),
        velocity.y + r3d_randf_range(-system->velocityVariance.y, system->velocityVariance.y),
        velocity.z + r3d_randf_range(-system->velocityVariance.z, system->velocityVariance.z)
    };

    particle.angularVelocity = particle.baseAngularVelocity = (Vector3){
        system->initialAngularVelocity.x + r3d_randf_range(-system->angularVelocityVariance.x, system->angularVelocityVariance.x),
        system->initialAngularVelocity.y + r3d_randf_range(-system->angularVelocityVariance.y, system->angularVelocityVariance.y),
        system->initialAngularVelocity.z + r3d_randf_range(-system->angularVelocityVariance.z, system->angularVelocityVariance.z)
    };

    particle.color = (Color){
        (unsigned char)(system->initialColor.r + GetRandomValue(-system->colorVariance.r, system->colorVariance.r)),
        (unsigned char)(system->initialColor.g + GetRandomValue(-system->colorVariance.g, system->colorVariance.g)),
        (unsigned char)(system->initialColor.g + GetRandomValue(-system->colorVariance.b, system->colorVariance.b)),
        (unsigned char)(system->initialColor.a + GetRandomValue(-system->colorVariance.a, system->colorVariance.a))
    };

    particle.baseOpacity = particle.color.a;

    // Adding the particle to the system
    system->particles[system->count++] = particle;

    return true;
}

void R3D_UpdateParticleSystem(R3D_ParticleSystem* system, float deltaTime)
{
    if (system->autoEmission && system->emissionRate > 0.0f) {
        system->emissionTimer -= deltaTime;
        while (system->emissionTimer <= 0.0f) {
            system->emissionTimer += 1.0f / system->emissionRate;
            R3D_EmitParticle(system);
        }
    }

    for (int i = system->count - 1; i >= 0; i--) {
        R3D_Particle* particle = &system->particles[i];

        particle->lifetime -= deltaTime;
        if (particle->lifetime <= 0.0f) {
            *particle = system->particles[--system->count];
            continue;
        }

        float t = 1.0f - (particle->lifetime / system->lifetime);

        if (system->scaleOverLifetime) {
            float scale = R3D_EvaluateCurve(*system->scaleOverLifetime, t);
            particle->scale.x = particle->baseScale.x * scale;
            particle->scale.y = particle->baseScale.y * scale;
            particle->scale.z = particle->baseScale.z * scale;
        }

        if (system->opacityOverLifetime) {
            float scale = R3D_EvaluateCurve(*system->opacityOverLifetime, t);
            particle->color.a = (unsigned char)Clamp(particle->baseOpacity * scale, 0.0f, 255.0f);
        }

        if (system->speedOverLifetime) {
            float scale = R3D_EvaluateCurve(*system->speedOverLifetime, t);
            particle->velocity.x = particle->baseVelocity.x * scale;
            particle->velocity.y = particle->baseVelocity.y * scale;
            particle->velocity.z = particle->baseVelocity.z * scale;
        }

        if (system->angularVelocityOverLifetime) {
            float scale = R3D_EvaluateCurve(*system->angularVelocityOverLifetime, t);
            particle->angularVelocity.x = particle->baseAngularVelocity.x * scale;
            particle->angularVelocity.y = particle->baseAngularVelocity.y * scale;
            particle->angularVelocity.z = particle->baseAngularVelocity.z * scale;
        }

        particle->rotation.x += particle->angularVelocity.x * deltaTime * DEG2RAD;
        particle->rotation.y += particle->angularVelocity.y * deltaTime * DEG2RAD;
        particle->rotation.z += particle->angularVelocity.z * deltaTime * DEG2RAD;

        particle->position.x += particle->velocity.x * deltaTime;
        particle->position.y += particle->velocity.y * deltaTime;
        particle->position.z += particle->velocity.z * deltaTime;

        particle->transform = r3d_matrix_scale_rotxyz_translate(
            particle->scale, particle->rotation, particle->position
        );

        particle->velocity.x += system->gravity.x * deltaTime;
        particle->velocity.y += system->gravity.y * deltaTime;
        particle->velocity.z += system->gravity.z * deltaTime;
    }
}

void R3D_CalculateParticleSystemBoundingBox(R3D_ParticleSystem* system)
{
    // Initialize the AABB with extreme values (max for max bounds, min for min bounds)
    Vector3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    // Loop over all particles in the emitter (considering the particle capacity)
    for (int i = 0; i < system->capacity; i++) {
        // Emit a particle for the current iteration
        R3D_EmitParticle(system);

        // Get the current particle from the emitter
        const R3D_Particle* particle = &system->particles[i];

        // Calculate the position of the particle at half its lifetime (intermediate position)
        float halfLifetime = particle->lifetime * 0.5f;
        Vector3 midPosition = {
            particle->transform.m12 + particle->velocity.x * halfLifetime + 0.5f * system->gravity.x * halfLifetime * halfLifetime,
            particle->transform.m13 + particle->velocity.y * halfLifetime + 0.5f * system->gravity.y * halfLifetime * halfLifetime,
            particle->transform.m14 + particle->velocity.z * halfLifetime + 0.5f * system->gravity.z * halfLifetime * halfLifetime
        };

        // Calculate the position of the particle at the end of its lifetime (final position)
        Vector3 futurePosition = {
            particle->transform.m12 + particle->velocity.x * particle->lifetime + 0.5f * system->gravity.x * particle->lifetime * particle->lifetime,
            particle->transform.m13 + particle->velocity.y * particle->lifetime + 0.5f * system->gravity.y * particle->lifetime * particle->lifetime,
            particle->transform.m14 + particle->velocity.z * particle->lifetime + 0.5f * system->gravity.z * particle->lifetime * particle->lifetime
        };

        // Expand the AABB by comparing the current min and max with the calculated positions
        // We include both the intermediate (halfway) and final (end of lifetime) positions
        aabbMin.x = r3d_min3f(aabbMin.x, midPosition.x, futurePosition.x);
        aabbMin.y = r3d_min3f(aabbMin.y, midPosition.y, futurePosition.y);
        aabbMin.z = r3d_min3f(aabbMin.z, midPosition.z, futurePosition.z);

        aabbMax.x = r3d_max3f(aabbMax.x, midPosition.x, futurePosition.x);
        aabbMax.y = r3d_max3f(aabbMax.y, midPosition.y, futurePosition.y);
        aabbMax.z = r3d_max3f(aabbMax.z, midPosition.z, futurePosition.z);
    }

    // Clear any previous state or data in the emitter
    system->count = 0;

    // Update the particle system's AABB with the calculated bounds
    system->aabb = (BoundingBox){ aabbMin, aabbMax };
}
