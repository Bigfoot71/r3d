/* atrous_wavelet.frag -- À-Trous Wavelet denoiser (depth + normal aware)
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSourceTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform float uNormalSharp;
uniform float uDepthSharp;
uniform int uStepWidth;     // Powers of 2: 1, 2, 4, 8... for each pass

/* === Fragments === */

out vec4 FragColor;

/* === À-Trous Kernel === */

const int KERNEL_SIZE = 9;

const ivec2 OFFSETS[9] = ivec2[9](
    ivec2(-1, -1), ivec2( 0, -1), ivec2( 1, -1),
    ivec2(-1,  0), ivec2( 0,  0), ivec2( 1,  0),
    ivec2(-1,  1), ivec2( 0,  1), ivec2( 1,  1)
);

const float WEIGHTS[9] = float[9](
    0.0625, 0.125, 0.0625,
    0.125,  0.25,  0.125,
    0.0625, 0.125, 0.0625
);

/* === Helper Functions === */

float NormalWeight(vec3 n0, vec3 n1)
{
    vec3 t = n0 - n1;
    float dist2 = max(dot(t, t) / (uStepWidth * uStepWidth), 0.0);
    return min(exp(-dist2 / uNormalSharp), 1.0);
}

float PositionWeight(vec3 p0, vec3 p1)
{
    vec3 t = p0 - p1;
    float dist2 = dot(t, t);
    return min(exp(-dist2 / uDepthSharp), 1.0);
}

ivec2 MirrorCoord(ivec2 coord, ivec2 resolution)
{
    ivec2 result = abs(coord);
    ivec2 wrapped = result % (2 * resolution);
    ivec2 mask = -(wrapped / resolution);
    return (wrapped & ~mask) | ((2 * resolution - wrapped - 1) & mask);
}

/* === Main Program === */

void main()
{
    vec3 centerPosition = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec3 centerNormal = V_GetWorldNormal(uNormalTex, ivec2(gl_FragCoord.xy));

    ivec2 resolution = textureSize(uSourceTex, 0);
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    vec4 result = vec4(0.0);
    float weightSum = 0.0;

    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        ivec2 offset = OFFSETS[i] * uStepWidth;
        ivec2 pixOffset = MirrorCoord(pixCoord + offset, resolution);

        vec3 samplePosition = V_GetViewPosition(uDepthTex, pixOffset);
        vec3 sampleNormal = V_GetWorldNormal(uNormalTex, pixOffset);
        vec4 sampleColor = texelFetch(uSourceTex, pixOffset, 0);

        float wPosition = PositionWeight(centerPosition, samplePosition);
        float wNormal = NormalWeight(centerNormal, sampleNormal);
        float w = WEIGHTS[i] * wPosition * wNormal;

        result += sampleColor * w;
        weightSum += w;
    }

    FragColor = result / max(weightSum, 1e-4);
}
