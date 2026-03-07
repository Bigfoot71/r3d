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

uniform float uInvNormalSharp;
uniform float uInvDepthSharp;
uniform float uInvStepWidth2;   // 1.0 / (uStepWidth*uStepWidth)
uniform int uStepWidth;         // Powers of 2: 1, 2, 4, 8... for each pass

/* === Fragments === */

out vec4 FragColor;

/* === Kernel === */

const int KERNEL_SIZE = 8;

const ivec2 OFFSETS[8] = ivec2[8](
    ivec2(-1,-1), ivec2(0,-1), ivec2(1,-1),
    ivec2(-1, 0),              ivec2(1, 0),
    ivec2(-1, 1), ivec2(0, 1), ivec2(1, 1)
);

const float WEIGHTS[8] = float[8](
    0.0625, 0.125, 0.0625,
    0.125,         0.125,
    0.0625, 0.125, 0.0625
);

/* === Helpers === */

ivec2 MirrorCoord(ivec2 coord, ivec2 resolution)
{
    ivec2 result = abs(coord);
    ivec2 wrapped = result % (2 * resolution);
    ivec2 mask = -(wrapped / resolution);
    return (wrapped & ~mask) | ((2 * resolution - wrapped - 1) & mask);
}

/* === Main === */

void main()
{
    ivec2 resolution = textureSize(uSourceTex, 0);
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    vec3 centerNormal = M_DecodeOctahedral(texelFetch(uNormalTex, pixCoord, 0).rg);
    float centerDepth = texelFetch(uDepthTex,  pixCoord, 0).r;
    vec4 centerColor = texelFetch(uSourceTex, pixCoord, 0);

    bool nearBorder = any(lessThan(pixCoord, ivec2(uStepWidth))) || any(greaterThanEqual(pixCoord, resolution - uStepWidth));
    float invDepth = 1.0 / (abs(centerDepth) + 1e-4);
    vec4 result = centerColor * 0.25;
    float weightSum = 0.25;

    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        ivec2 sampleCoord = pixCoord + OFFSETS[i] * uStepWidth;
        if (nearBorder) sampleCoord = MirrorCoord(sampleCoord, resolution);

        vec3 sn = M_DecodeOctahedral(texelFetch(uNormalTex, sampleCoord, 0).rg);
        float sd = texelFetch(uDepthTex,  sampleCoord, 0).r;
        vec4 sc = texelFetch(uSourceTex, sampleCoord, 0);

        vec3 nt = centerNormal - sn;
        float dz = (centerDepth - sd) * invDepth;
        float dist2 = dot(nt, nt) * uInvStepWidth2;

        float w = WEIGHTS[i] * exp(
            -dist2 * uInvNormalSharp
            -dz*dz * uInvDepthSharp
        );

        result += sc * w;
        weightSum += w;
    }

    FragColor = result / max(weightSum, 1e-4);
}
