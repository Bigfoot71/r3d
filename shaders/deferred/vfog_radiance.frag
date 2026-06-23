/* vfog_radiance.frag -- Fragment shader of volumetric fog radiance
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Extensions === */

#extension GL_ARB_texture_cube_map_array : enable

/* === Includes === */

#include <lib/math.glsl>
#include <ubo/fx.glsl>

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDepthTex;

uniform sampler2DArrayShadow uShadowDirTex;
uniform sampler2DArrayShadow uShadowSpotTex;
uniform samplerCubeArrayShadow uShadowOmniTex;

/* === Blocks === */

#include <wrap/light.glsl>
#include <wrap/view.glsl>

/* === Fragments === */

out vec3 FragRadiance;

/* === Helper Functions === */

float PhaseFunction_Schlick(vec3 w0, vec3 w1)
{
    // Both vectors assumed normalized; skip length division
    float cosTheta = dot(w0, w1);
    float nom = 1.0 - uVFog.anisotropy * uVFog.anisotropy;
    float denom = 4.0 * M_PI * (1.0 + uVFog.anisotropy * cosTheta) * (1.0 + uVFog.anisotropy * cosTheta);
    return nom / denom;
}

// Computes the march range [tEnter, tExit] along rayDir (from rayOrigin),
// clamped to uVFog.length and to the light's range sphere when applicable.
// tEnter/tExit are both 0.0 when the ray never enters the light's range
void VFog_GetMarchRange(vec3 rayOrigin, vec3 rayDir, float maxDist, out float tEnter, out float tExit)
{
    if (uLight.type == LIGHT_DIR) {
        tEnter = 0.0;
        tExit = maxDist;
        return;
    }

    vec3 toLight = uLight.position - rayOrigin;
    float tCenter = dot(toLight, rayDir);
    float distToRaySq = dot(toLight, toLight) - tCenter * tCenter;
    float rangeSq = uLight.range * uLight.range;

    if (distToRaySq >= rangeSq) {
        tEnter = 0.0;
        tExit = 0.0;
        return;
    }

    float halfChord = sqrt(rangeSq - distToRaySq);

    tEnter = max(tCenter - halfChord, 0.0);
    tExit = min(tCenter + halfChord, maxDist);
}

/* === Main Function === */

void main()
{
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    float depth = texelFetch(uDepthTex, pixCoord, 0).r;
    vec3 P = V_GetWorldPosition(vTexCoord, depth);

    vec3 toFrag = P - uView.position;
    float fragDist = length(toFrag);
    vec3 rayDir = toFrag / max(fragDist, 1e-6);

    float tEnter, tExit;
    VFog_GetMarchRange(uView.position, rayDir, uVFog.length, tEnter, tExit);

    tExit = min(fragDist, tExit);
    float rayDist = tExit - tEnter;

    // Early-out: this light's range sphere doesn't intersect the view ray
    if (rayDist <= 0.0) {
        FragRadiance = vec3(0.0);
        return;
    }

    vec3 marchPos = uView.position + rayDir * tEnter;
    vec3 deltaStep = rayDir * uVFog.stepSize;
    vec3 fragToCameraNorm = -rayDir; // view direction along the ray

    // Jitter starting position to break up banding, adjust ray length to avoid overshooting
    float jitter = M_HashIGN(gl_FragCoord.xy);
    marchPos += deltaStep * jitter;
    rayDist -= uVFog.stepSize * jitter;

    vec3 radiance = vec3(0.0);
    float transmittance = 1.0;

    // Extinction coefficient and per-step transmittance factor (constant along the ray)
    float sigmaT = uVFog.scatteringDensity + uVFog.absortionDensity;
    float transmittanceStep = exp(-sigmaT * uVFog.stepSize);

    // Precalculate constant terms
    vec3 lightColor = uLight.color * uLight.energy * uLight.fogEnergy;
    mat2 diskRot = L_ShadowDebandingMatrix(gl_FragCoord.xy);
    bool hasShadow = uLight.shadowLayer >= 0 && uLight.shadowOpacity != 0.0;

    // Precompute inverses used inside the loop
    float invRange = 1.0 / max(uLight.range, 1e-4);
    float invEpsilon = 1.0 / max(uLight.innerCutOff - uLight.outerCutOff, 1e-4);

    for (float l = 0; l < rayDist; l += uVFog.stepSize)
    {
        // Compute light direction and distance at each march step
        vec3 Ldelta = uLight.position - marchPos;
        float Ldist = length(Ldelta);
        float invLdist = 1.0 / max(Ldist, 1e-4);
        vec3 L = (uLight.type == LIGHT_DIR) ? -uLight.direction : Ldelta * invLdist;

        // Shadow visibility at current march position
        float visibility = 1.0;
        if (hasShadow) {
            switch (uLight.type) {
            case LIGHT_DIR:  visibility *= L_SampleShadowDir(marchPos, depth, 1.0, diskRot); break;
            case LIGHT_SPOT: visibility *= L_SampleShadowSpot(marchPos, 1.0, diskRot); break;
            case LIGHT_OMNI: visibility *= L_SampleShadowOmni(marchPos, 1.0, diskRot); break;
            }
        }

        // Range attenuation (omni and spot only)
        if (uLight.type != LIGHT_DIR) {
            float atten = pow(1.0 - clamp(Ldist * invRange, 0.0, 1.0), uLight.falloff);
            visibility *= atten;
        }

        // Angular attenuation (spot only)
        if (uLight.type == LIGHT_SPOT) {
            float theta = dot(L, -uLight.direction);
            visibility *= smoothstep(0.0, 1.0, (theta - uLight.outerCutOff) * invEpsilon);
        }

        // In-scattering: phase function * scattering coefficient * incident radiance
        float phase = PhaseFunction_Schlick(L, fragToCameraNorm);
        vec3 Li = uVFog.scatteringDensity * uVFog.scatteringColor.rgb * phase * lightColor * visibility;

        // Accumulate transmittance then integrate radiance (order matters)
        transmittance *= transmittanceStep;
        radiance += Li * transmittance * uVFog.stepSize;

        marchPos += deltaStep;
    }

    // Attenuate contribution on sky pixels
    FragRadiance = (depth >= uView.far) ? radiance * uVFog.skyAffect : radiance;
}
