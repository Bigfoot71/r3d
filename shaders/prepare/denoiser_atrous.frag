/* denoiser_trous.frag -- À-Trous denoiser
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/wrap/view.glsl"
#include "../include/lib/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSourceTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform float uNormalSharpness;
uniform float uDepthSharpness;
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

#define OFFSCREEN(sc, res) (any(lessThan(sc, ivec2(0))) || any(greaterThanEqual(sc, res)))

/* === Main === */

void main()
{
    ivec2 resolution = textureSize(uSourceTex, 0);
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    vec3 cp = V_GetViewPosition(uDepthTex, pixCoord);
    vec3 cn = V_GetViewNormal(uNormalTex, pixCoord);
    vec4 cc = texelFetch(uSourceTex, pixCoord, 0);

    vec4 result = cc * 0.25;
    float weightSum = 0.25;

    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        ivec2 sampleCoord = pixCoord + OFFSETS[i] * uStepWidth;
        if (OFFSCREEN(sampleCoord, resolution)) continue;

        vec3 sp = V_GetViewPosition(uDepthTex, sampleCoord);
        vec3 sn = V_GetViewNormal(uNormalTex, sampleCoord);
        vec4 sc = texelFetch(uSourceTex, sampleCoord, 0);

        float planeDist = dot(sp - cp, cn);
        float nDiff = dot(cn - sn, cn - sn) * uInvStepWidth2;

        float w = WEIGHTS[i] * exp(
            -nDiff * uNormalSharpness
            -planeDist * planeDist * uDepthSharpness
        );

        result += sc * w;
        weightSum += w;
    }

    FragColor = result / max(weightSum, 1e-4);
}
