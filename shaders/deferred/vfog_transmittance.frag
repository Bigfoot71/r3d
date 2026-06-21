/* vfog_transmittance.frag -- Fragment shader of volumetric fog transmittance
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include <ubo/view.glsl>
#include <lib/math.glsl>

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDepthTex;

uniform float uStepSize;
uniform float uLength;
uniform float uScatteringDensity;
uniform float uAbsortionDensity;
uniform vec3  uEmissionColor;
uniform float uEmissionEnergy;
uniform float uSkyAffect;

/* === Fragments === */

out vec4 FragColor; // rgb = emission, a = transmittance (composed via blend mode)

/* === Main Function === */

void main()
{
    float depth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    float rayDist = min(depth, uLength);

    // Analytic transmittance over the full ray distance
    float sigmaT = uScatteringDensity + uAbsortionDensity;
    float transmittance = exp(-sigmaT * rayDist);

    // Emission fills the volume proportionally to extinction
    float extinction = 1.0 - transmittance;
    vec3 emission = uEmissionColor * (uEmissionEnergy * extinction);

    // Scale effect on sky pixels
    if (depth >= uView.far) {
        transmittance = mix(1.0, transmittance, uSkyAffect);  // 0 = no effect, 1 = full effect
        emission *= uSkyAffect;                               // mix(vec3(0), emission, t) == emission * t
    }

    FragColor = vec4(emission, transmittance);
}
