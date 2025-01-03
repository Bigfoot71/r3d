/*
 * Copyright (c) 2024 Le Juez Victor
 * 
 * This software is provided "as-is", without any express or implied warranty. In no event 
 * will the authors be held liable for any damages arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose, including commercial 
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 * 
 *   1. The origin of this software must not be misrepresented; you must not claim that you 
 *   wrote the original software. If you use this software in a product, an acknowledgment 
 *   in the product documentation would be appreciated but is not required.
 * 
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 * 
 *   3. This notice may not be removed or altered from any source distribution.
 */

#include "r3d.h"

#include "../detail/RandomGenerator.hpp"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cstdlib>
#include <cfloat>
#include <cmath>

R3D_ParticleSystemCPU* R3D_LoadParticleEmitterCPU(const Mesh* mesh, const R3D_Material* material, int maxParticles)
{
    R3D_ParticleSystemCPU *system = new R3D_ParticleSystemCPU{};

    system->particles = new R3D_Particle[maxParticles];
    system->maxParticles = maxParticles;
    system->particleCount = 0;

    system->surface = (R3D_Surface) {
        .material = *material,
        .mesh = *mesh
    };

    system->position = { 0, 0, 0 };
    system->gravity = { 0, -9.81f, 0 };

    system->initialScale = Vector3One();
    system->scaleVariance = 0.0f;

    system->initialRotation = Vector3Zero();
    system->rotationVariance = Vector3Zero();

    system->initialColor = WHITE;
    system->colorVariance = BLANK;

    system->initialVelocity = { 0, 0, 0 };
    system->velocityVariance = Vector3Zero();

    system->initialAngularVelocity = Vector3Zero();
    system->angularVelocityVariance = Vector3Zero();

    system->lifetime = 1.0f;
    system->lifetimeVariance = 0.0f;

    system->emissionTimer = 0.0f;
    system->emissionRate = 1.0f;
    system->spreadAngle = 0.0f;

    system->aabb = {
        { -10.0f, -10.0f, -10.0f },
        { 10.0f, 10.0f, 10.0f }
    };

    system->scaleOverLifetime = nullptr;
    system->speedOverLifetime = nullptr;
    system->opacityOverLifetime = nullptr;
    system->angularVelocityOverLifetime = nullptr;

    system->shadow = R3D_CAST_OFF;
    system->billboard = R3D_BILLBOARD_ENABLED;
    system->layer = R3D_LAYER_1;

    system->autoEmission = true;

    return system;
}

void R3D_UnloadParticleEmitterCPU(R3D_ParticleSystemCPU* system)
{
    if (system) {
        delete[] system->particles;
        delete system;
    }
}

bool R3D_EmitParticleCPU(R3D_ParticleSystemCPU* system)
{
    if (system->particleCount >= system->maxParticles) {
        return false;
    }

    // Normalize the initial direction
    Vector3 direction = Vector3Normalize(system->initialVelocity);

    // Generate random angles
    float elevation = r3d::RandomGenerator::singleton().rand(0.0f, DEG2RAD * system->spreadAngle);
    float azimuth = r3d::RandomGenerator::singleton().rand(0.0f, 2.0f * PI);

    // Precompute trigonometric values for the cone
    float cosElevation = std::cos(elevation);
    float sinElevation = std::sqrt(1.0f - cosElevation * cosElevation); // Use the trigonometric identity
    float cosAzimuth = std::cos(azimuth);
    float sinAzimuth = std::sin(azimuth);

    // Calculate the vector within the cone (local coordinate system)
    Vector3 spreadDirection = {
        sinElevation * cosAzimuth,
        sinElevation * sinAzimuth,
        cosElevation
    };

    // Generate the local basis around 'direction'
    Vector3 arbitraryAxis = (std::abs(direction.y) > 0.9999f)
        ? Vector3 { 0.0f, 0.0f, 1.0f }
        : Vector3 { 1.0f, 0.0f, 0.0f };

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
    R3D_Particle particle{};
    particle.lifetime = system->lifetime + r3d::RandomGenerator::singleton().rand(-system->lifetimeVariance, system->lifetimeVariance);
    particle.position = system->position;
    particle.rotation = {
        (system->initialRotation.x + r3d::RandomGenerator::singleton().rand(-system->rotationVariance.x, system->rotationVariance.x)) * DEG2RAD,
        (system->initialRotation.y + r3d::RandomGenerator::singleton().rand(-system->rotationVariance.y, system->rotationVariance.y)) * DEG2RAD,
        (system->initialRotation.z + r3d::RandomGenerator::singleton().rand(-system->rotationVariance.z, system->rotationVariance.z)) * DEG2RAD
    };
    particle.scale = particle.baseScale = Vector3AddValue(
        system->initialScale, r3d::RandomGenerator::singleton().rand(-system->scaleVariance, system->scaleVariance)
    );
    particle.velocity = particle.baseVelocity = {
        velocity.x + r3d::RandomGenerator::singleton().rand(-system->velocityVariance.x, system->velocityVariance.x),
        velocity.y + r3d::RandomGenerator::singleton().rand(-system->velocityVariance.y, system->velocityVariance.y),
        velocity.z + r3d::RandomGenerator::singleton().rand(-system->velocityVariance.z, system->velocityVariance.z)
    };
    particle.angularVelocity = particle.baseAngularVelocity = {
        system->initialAngularVelocity.x + r3d::RandomGenerator::singleton().rand(-system->angularVelocityVariance.x, system->angularVelocityVariance.x),
        system->initialAngularVelocity.y + r3d::RandomGenerator::singleton().rand(-system->angularVelocityVariance.y, system->angularVelocityVariance.y),
        system->initialAngularVelocity.z + r3d::RandomGenerator::singleton().rand(-system->angularVelocityVariance.z, system->angularVelocityVariance.z)
    };
    particle.color = {
        static_cast<uint8_t>(system->initialColor.r + r3d::RandomGenerator::singleton().rand<int>(-system->colorVariance.r, system->colorVariance.r)),
        static_cast<uint8_t>(system->initialColor.g + r3d::RandomGenerator::singleton().rand<int>(-system->colorVariance.g, system->colorVariance.g)),
        static_cast<uint8_t>(system->initialColor.g + r3d::RandomGenerator::singleton().rand<int>(-system->colorVariance.b, system->colorVariance.b)),
        static_cast<uint8_t>(system->initialColor.a + r3d::RandomGenerator::singleton().rand<int>(-system->colorVariance.a, system->colorVariance.a))
    };
    particle.baseOpacity = particle.color.a;

    // Adding the particle to the system
    system->particles[system->particleCount++] = particle;

    return true;
}

void R3D_UpdateParticleEmitterCPU(R3D_ParticleSystemCPU* system, float deltaTime)
{
    system->emissionTimer -= deltaTime;

    if (system->emissionRate > 0.0f) {
        while (system->emissionTimer <= 0.0f) {
            R3D_EmitParticleCPU(system);
            system->emissionTimer += 1.0f / system->emissionRate;
            if (system->emissionRate <= 0.0f) {
                break;
            }
        }
    }

    for (int i = system->particleCount - 1; i >= 0; i--) {
        R3D_Particle& particle = system->particles[i];

        particle.lifetime -= deltaTime;
        if (particle.lifetime <= 0.0f) {
            particle = system->particles[--system->particleCount];
            continue;
        }

        float t = 1.0f - (particle.lifetime / system->lifetime);

        if (system->scaleOverLifetime) {
            float scale = R3D_EvaluateCurve(system->scaleOverLifetime, t);
            particle.scale.x = particle.baseScale.x *  scale;
            particle.scale.y = particle.baseScale.y *  scale;
            particle.scale.z = particle.baseScale.z *  scale;
        }

        if (system->opacityOverLifetime) {
            float scale = R3D_EvaluateCurve(system->opacityOverLifetime, t);
            particle.color.a = std::clamp(particle.baseOpacity * scale, 0.0f, 255.0f);
        }

        if (system->speedOverLifetime) {
            float scale = R3D_EvaluateCurve(system->speedOverLifetime, t);
            particle.velocity.x = particle.baseVelocity.x * scale;
            particle.velocity.y = particle.baseVelocity.y * scale;
            particle.velocity.z = particle.baseVelocity.z * scale;
        }

        if (system->angularVelocityOverLifetime) {
            float scale = R3D_EvaluateCurve(system->angularVelocityOverLifetime, t);
            particle.angularVelocity.x = particle.baseAngularVelocity.x * scale;
            particle.angularVelocity.y = particle.baseAngularVelocity.y * scale;
            particle.angularVelocity.z = particle.baseAngularVelocity.z * scale;
        }

        particle.rotation.x += particle.angularVelocity.x * deltaTime * DEG2RAD;
        particle.rotation.y += particle.angularVelocity.y * deltaTime * DEG2RAD;
        particle.rotation.z += particle.angularVelocity.z * deltaTime * DEG2RAD;

        particle.position.x += particle.velocity.x * deltaTime;
        particle.position.y += particle.velocity.y * deltaTime;
        particle.position.z += particle.velocity.z * deltaTime;

        particle.velocity.x += system->gravity.x * deltaTime;
        particle.velocity.y += system->gravity.y * deltaTime;
        particle.velocity.z += system->gravity.z * deltaTime;
    }
}

void R3D_UpdateParticleEmitterCPUAABB(R3D_ParticleSystemCPU* system)
{
    // Initialize the AABB with extreme values (max for max bounds, min for min bounds)
    Vector3 aabbMin = { FLT_MAX, FLT_MAX, FLT_MAX };
    Vector3 aabbMax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

    // Loop over all particles in the emitter (considering the particle capacity)
    for (int i = 0; i < system->maxParticles; i++) {
        // Emit a particle for the current iteration
        R3D_EmitParticleCPU(system);

        // Get the current particle from the emitter
        const R3D_Particle& particle = system->particles[i];

        // Calculate the position of the particle at half its lifetime (intermediate position)
        float halfLifetime = particle.lifetime * 0.5f;
        Vector3 midPosition = {
            particle.position.x + particle.velocity.x * halfLifetime + 0.5f * system->gravity.x * halfLifetime * halfLifetime,
            particle.position.y + particle.velocity.y * halfLifetime + 0.5f * system->gravity.y * halfLifetime * halfLifetime,
            particle.position.z + particle.velocity.z * halfLifetime + 0.5f * system->gravity.z * halfLifetime * halfLifetime
        };

        // Calculate the position of the particle at the end of its lifetime (final position)
        Vector3 futurePosition = {
            particle.position.x + particle.velocity.x * particle.lifetime + 0.5f * system->gravity.x * particle.lifetime * particle.lifetime,
            particle.position.y + particle.velocity.y * particle.lifetime + 0.5f * system->gravity.y * particle.lifetime * particle.lifetime,
            particle.position.z + particle.velocity.z * particle.lifetime + 0.5f * system->gravity.z * particle.lifetime * particle.lifetime
        };

        // Expand the AABB by comparing the current min and max with the calculated positions
        // We include both the intermediate (halfway) and final (end of lifetime) positions
        aabbMin.x = std::min({aabbMin.x, midPosition.x, futurePosition.x});
        aabbMin.y = std::min({aabbMin.y, midPosition.y, futurePosition.y});
        aabbMin.z = std::min({aabbMin.z, midPosition.z, futurePosition.z});

        aabbMax.x = std::max({aabbMax.x, midPosition.x, futurePosition.x});
        aabbMax.y = std::max({aabbMax.y, midPosition.y, futurePosition.y});
        aabbMax.z = std::max({aabbMax.z, midPosition.z, futurePosition.z});
    }

    // Clear any previous state or data in the emitter
    system->particleCount = 0;

    // Update the particle system's AABB with the calculated bounds
    system->aabb = { aabbMin, aabbMax };
}
