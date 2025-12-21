/* r3d_particles.h -- R3D Particles Module.
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#ifndef R3D_PARTICLES_H
#define R3D_PARTICLES_H

#include "./r3d_platform.h"
#include "./r3d_curves.h"
#include <raylib.h>

/**
 * @defgroup Particles
 * @{
 */

// ========================================
// STRUCTS TYPES
// ========================================

/**
 * @struct R3D_Particle
 * @brief Represents a particle in a 3D particle system, with properties
 *        such as position, velocity, rotation, and color modulation.
 */
typedef struct R3D_Particle {

    float lifetime;                 ///< Duration of the particle's existence in seconds.

    Matrix transform;               ///< The particle's current transformation matrix in 3D space.

    Vector3 position;               ///< The current position of the particle in 3D space.
    Vector3 rotation;               ///< The current rotation of the particle in 3D space (Euler angles).
    Vector3 scale;                  ///< The current scale of the particle in 3D space.
    Color color;                    ///< The current color of the particle, representing its color modulation.

    Vector3 velocity;               ///< The current velocity of the particle in 3D space.
    Vector3 angularVelocity;        ///< The current angular velocity of the particle in radians (Euler angles).

    Vector3 baseScale;              ///< The initial scale of the particle in 3D space.
    Vector3 baseVelocity;           ///< The initial velocity of the particle in 3D space.
    Vector3 baseAngularVelocity;    ///< The initial angular velocity of the particle in radians (Euler angles).
    unsigned char baseOpacity;      ///< The initial opacity of the particle, ranging from 0 (fully transparent) to 255 (fully opaque).

} R3D_Particle;

/**
 * @brief Represents a CPU-based particle system with various properties and settings.
 *
 * This structure contains configuration data for a particle system, such as mesh information, initial properties,
 * curves for controlling properties over time, and settings for shadow casting, emission rate, and more.
 */
typedef struct R3D_ParticleSystem {

    R3D_Particle* particles;            ///< Pointer to the array of particles in the system.
    int capacity;                       ///< The maximum number of particles the system can manage.
    int count;                          ///< The current number of active particles in the system.

    Vector3 position;                   ///< The initial position of the particle system. Default: (0, 0, 0).
    Vector3 gravity;                    ///< The gravity applied to the particles. Default: (0, -9.81, 0).

    Vector3 initialScale;               ///< The initial scale of the particles. Default: (1, 1, 1).
    float scaleVariance;                ///< The variance in particle scale. Default: 0.0f.

    Vector3 initialRotation;            ///< The initial rotation of the particles in Euler angles (degrees). Default: (0, 0, 0).
    Vector3 rotationVariance;           ///< The variance in particle rotation in Euler angles (degrees). Default: (0, 0, 0).

    Color initialColor;                 ///< The initial color of the particles. Default: WHITE.
    Color colorVariance;                ///< The variance in particle color. Default: BLANK.

    Vector3 initialVelocity;            ///< The initial velocity of the particles. Default: (0, 0, 0).
    Vector3 velocityVariance;           ///< The variance in particle velocity. Default: (0, 0, 0).

    Vector3 initialAngularVelocity;     ///< The initial angular velocity of the particles in Euler angles (degrees). Default: (0, 0, 0).
    Vector3 angularVelocityVariance;    ///< The variance in angular velocity. Default: (0, 0, 0).

    float lifetime;                     ///< The lifetime of the particles in seconds. Default: 1.0f.
    float lifetimeVariance;             ///< The variance in lifetime in seconds. Default: 0.0f.

    float emissionTimer;                ///< Use to control automatic emission, should not be modified manually.
    float emissionRate;                 ///< The rate of particle emission in particles per second. Default: 10.0f.
    float spreadAngle;                  ///< The angle of propagation of the particles in a cone (degrees). Default: 0.0f.

    R3D_InterpolationCurve* scaleOverLifetime;              ///< Curve controlling the scale evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* speedOverLifetime;              ///< Curve controlling the speed evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* opacityOverLifetime;            ///< Curve controlling the opacity evolution of the particles over their lifetime. Default: NULL.
    R3D_InterpolationCurve* angularVelocityOverLifetime;    ///< Curve controlling the angular velocity evolution of the particles over their lifetime. Default: NULL.

    BoundingBox aabb;                   ///< For frustum culling. Defaults to a large AABB; compute manually via `R3D_CalculateParticleSystemBoundingBox` after setup.

    bool autoEmission;                  /**< Indicates whether particle emission is automatic when calling `R3D_UpdateParticleSystem`.
                                         *   If false, emission is manual using `R3D_EmitParticle`. Default: true.
                                         */

} R3D_ParticleSystem;

// ========================================
// PUBLIC API
// ========================================

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------
// PARTICLES: Particle System Functions
// ----------------------------------------

/**
 * @brief Loads a particle emitter system for the CPU.
 *
 * This function initializes a particle emitter system on the CPU with a specified maximum
 * number of particles. It prepares the necessary data structures and allocates memory
 * for managing the particles.
 *
 * @param maxParticles The maximum number of particles the system can handle at once.
 *                     This value determines the memory allocation and performance constraints.
 * @return A newly initialized `R3D_ParticleSystem` structure.
 *         The caller is responsible for properly managing and freeing the system when no longer needed.
 */
R3DAPI R3D_ParticleSystem R3D_LoadParticleSystem(int maxParticles);

/**
 * @brief Unloads the particle emitter system and frees allocated memory.
 *
 * This function deallocates the memory used by the particle emitter system and clears the associated resources.
 * It should be called when the particle system is no longer needed to prevent memory leaks.
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be unloaded.
 */
R3DAPI void R3D_UnloadParticleSystem(R3D_ParticleSystem* system);

/**
 * @brief Emits a particle in the particle system.
 *
 * This function triggers the emission of a new particle in the particle system. It handles the logic of adding a new
 * particle to the system and initializing its properties based on the current state of the system.
 *
 * @param system A pointer to the `R3D_ParticleSystemCPU` where the particle will be emitted.
 * @return `true` if the particle was successfully emitted, `false` if the system is at full capacity and cannot emit more particles.
 */
R3DAPI bool R3D_EmitParticle(R3D_ParticleSystem* system);

/**
 * @brief Updates the particle emitter system by advancing particle positions.
 *
 * This function updates the positions and properties of particles in the system based on the elapsed time. It handles
 * simulation of particle movement, gravity, and other physics-based calculations.
 *
 * @param system A pointer to the `R3D_ParticleSystem` to be updated.
 * @param deltaTime The time elapsed since the last update (in seconds).
 */
R3DAPI void R3D_UpdateParticleSystem(R3D_ParticleSystem* system, float deltaTime);

/**
 * @brief Computes and updates the AABB (Axis-Aligned Bounding Box) of a particle system.
 *
 * This function simulates the particle system to estimate the region of space it occupies.
 * It considers particle positions at mid-life and end-of-life to approximate the AABB,
 * which is then stored in the system's `aabb` field. This is useful for enabling frustum culling,
 * especially when the bounds are not known beforehand.
 *
 * @param system Pointer to the `R3D_ParticleSystem` to update.
 */
R3DAPI void R3D_CalculateParticleSystemBoundingBox(R3D_ParticleSystem* system);

#ifdef __cplusplus
} // extern "C"
#endif

/** @} */ // end of Particles

#endif // R3D_PARTICLES_H
