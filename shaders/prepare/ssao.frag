/* ssao.frag -- Scalable Ambient Occlusion fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Adapted from the methods proposed by Morgan McGuire et al. in "AlchemyAO" and "Scalable Ambient Obscurance"

// SEE AlchemyAO: https://casual-effects.com/research/McGuire2011AlchemyAO/VV11AlchemyAO.pdf
// SEE SAO:       https://research.nvidia.com/sites/default/files/pubs/2012-06_Scalable-Ambient-Obscurance/McGuire12SAO.pdf

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform int uSampleCount;
uniform float uRadius;
uniform float uBias;
uniform float uIntensity;
uniform float uPower;

/* === Constants === */

const float SAMPLES_PER_TURN = 5.0;

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec2 TapLocation(int i, float turns, float spin, out float rNorm)
{
    float alpha = (float(i) + 0.5) / float(uSampleCount);
    float angle = alpha * turns + spin;

    rNorm = alpha;

    return vec2(cos(angle), sin(angle));
}

/* === Main program === */

void main()
{
    vec3 position = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec3 normal = V_GetViewNormal(uNormalTex, ivec2(gl_FragCoord.xy));

    // Compute the radius in screen space
    float ssRadius = uRadius * uView.proj[1][1] / -position.z;

    // Clamping the screen space radius could avoid big cache misses
    // and possible artifacts when very close to an object. To test
    //ssRadius = min(ssRadius, 0.1); // 10% of the screen

    // For the spin, the AlchemyAO method did this: float((3*px.x^px.y+px.x*px.y)*10)
    // But I found that using a simple IGN produce a really more stable result
    float turns = M_TAU * max(1.0, float(uSampleCount) / SAMPLES_PER_TURN);
    float spin = M_TAU * M_HashIGN(gl_FragCoord.xy);
    float radiusSq = uRadius * uRadius;
    float bias = uBias * ssRadius;

    float aoSum = 0.0;
    for (int i = 0; i < uSampleCount; ++i)
    {
        float rNorm;
        vec2 dir = TapLocation(i, turns, spin, rNorm);
        vec2 offset = vTexCoord + dir * ssRadius * rNorm;
        if (V_OffScreen(offset)) continue;

        // The "SAO" paper recommends using mipmaps of the linearized depth here, but hey
        vec3 samplePos = V_GetViewPosition(uDepthTex, offset);
        vec3 v = samplePos - position;

        float vv = dot(v, v);
        float vn = dot(v, normal);
        if (vv > radiusSq) continue; // Reject samples beyond the world space radius

        // AlchemyAO formula (original, then our own with adaptive bias)
        //aoSum += max(vn + position.z * uBias, 0.0) / (vv + 0.01);
        aoSum += max(vn - bias, 0.0) / (vv + 0.01);
    }

    // Ignore the paper's factor of 2, it comes from
    // hemispherical integration but just over darkens in practice...
    float A = (uIntensity / float(uSampleCount)) * aoSum;

    // Conversion to accessibility factor then apply contrast
    FragOcclusion = pow(max(1.0 - A, 0.0), uPower);
}
