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

/* === Main Function === */

void main()
{
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    float depth = texelFetch(uDepthTex, pixCoord, 0).r;
    vec3 P = V_GetWorldPosition(vTexCoord, depth);

    vec3 rayDir = normalize(P - uView.position);
    float rayLen = (uLight.type != LIGHT_DIR) ? min(uVFog.length, uLight.range) : uVFog.length;
    float rayDist = min(length(P - uView.position), rayLen);

    vec3 marchPos = uView.position;
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

    for (float l = 0; l < rayDist; l += uVFog.stepSize)
    {
        // Compute light direction and distance at each march step
        vec3 Ldelta = uLight.position - marchPos;
        float Ldist = length(Ldelta);
        vec3 L = (uLight.type == LIGHT_DIR) ? -uLight.direction : Ldelta / max(Ldist, 1e-4);

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
            float atten = pow(1.0 - clamp(Ldist / uLight.range, 0.0, 1.0), uLight.falloff);
            visibility *= atten;
        }

        // Angular attenuation (spot only)
        if (uLight.type == LIGHT_SPOT) {
            float theta = dot(L, -uLight.direction);
            float epsilon = uLight.innerCutOff - uLight.outerCutOff;
            visibility *= smoothstep(0.0, 1.0, (theta - uLight.outerCutOff) / epsilon);
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
