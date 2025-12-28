/* ssao.frag -- Scalable Ambient Occlusion fragment shader
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Adapted from the methods proposed by Morgan McGuire et al. in "Alchemy AO" and "Scalable Ambient Obscurance"

// SEE AlchemyAO: https://casual-effects.com/research/McGuire2011AlchemyAO/VV11AlchemyAO.pdf
// SEE SAO:       https://research.nvidia.com/sites/default/files/pubs/2012-06_Scalable-Ambient-Obscurance/McGuire12SAO.pdf

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexDepth;
uniform sampler2D uTexNormal;

uniform int uSampleCount;
uniform float uRadius;
uniform float uBias;
uniform float uIntensity;
uniform float uPower;

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec2 TapLocation(int i, float spinAngle, out float ssRadius)
{
    float alpha = (float(i) + 0.5) / float(uSampleCount);
    float angle = alpha * (M_TAU * 3.0) + spinAngle;        //< 3 spiral turns

    ssRadius = alpha;
    return vec2(cos(angle), sin(angle));
}

/* === Main program === */

void main()
{
    vec3 position = V_GetViewPosition(uTexDepth, vTexCoord);
    vec3 normal = V_GetViewNormal(uTexNormal, vTexCoord);

    float ssRadius = (-uRadius * uView.proj[0][0]) / position.z;
    float spin = M_TAU * M_HashIGN(gl_FragCoord.xy);

    float occlusion = 0.0;
    float valid = 0.0;

    for (int i = 0; i < uSampleCount; ++i)
    {
        float rNorm;
        vec2 dir = TapLocation(i, spin, rNorm);
        vec2 offset = vTexCoord + dir * ssRadius * rNorm;

        vec3 samplePos = V_GetViewPosition(uTexDepth, offset);
        vec3 v = samplePos - position;
        float dist2 = dot(v, v);

        float NdotV = dot(v, normal) - uBias;
        float rWS2 = uRadius * uRadius;

        occlusion += pow(max(rWS2 - dist2, 0.0) / rWS2, 3.0) * max(NdotV / (0.01 + dist2), 0.0);
        valid += 1.0;
    }

    occlusion = 1.0 - occlusion / float(uSampleCount);
    occlusion = clamp(pow(occlusion, uPower), 0.0, 1.0);

    FragOcclusion = mix(1.0, occlusion, uIntensity);
}
