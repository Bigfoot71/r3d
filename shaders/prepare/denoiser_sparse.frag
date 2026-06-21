/* denoiser_sparse.frag -- Sparse denoiser
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
uniform float uInvBlurRadius2;  // 1.0 / (uBlurRadius*uBlurRadius)
uniform float uBlurRadius;

/* === Fragments === */

out vec4 FragColor;

/* === Kernel === */

const int KERNEL_SIZE = 8;

const vec2 DISK[8] = vec2[8](
    vec2( 0.8376,  0.1478),
    vec2( 0.3502,  0.2715),
    vec2(-0.1253,  0.8960),
    vec2(-0.3420,  0.2562),
    vec2(-0.8573, -0.1832),
    vec2(-0.3472, -0.3570),
    vec2( 0.1599, -0.9071),
    vec2( 0.3951, -0.2541)
);

/* === Helpers === */

#define OFFSCREEN(sc, res) (any(lessThan(sc, ivec2(0))) || any(greaterThanEqual(sc, res)))

/* === Main === */

void main()
{
    ivec2 resolution = textureSize(uSourceTex, 0);
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);
    vec2 invRes = 1.0 / vec2(resolution);

    vec3 cp = V_GetViewPosition(uDepthTex, pixCoord); 
    vec3 cn = V_GetViewNormal(uNormalTex, pixCoord);
    vec4 cc = texelFetch(uSourceTex, pixCoord, 0);

    vec4 result = cc;
    float weightSum = 1.0;

    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        vec2 offset = DISK[i] * uBlurRadius;
        ivec2 iSampleCoord = ivec2(gl_FragCoord.xy + offset);

        if (OFFSCREEN(iSampleCoord, resolution)) continue;
        vec2 fSampleCoord = (gl_FragCoord.xy + offset) * invRes;

        vec3 sp = V_GetViewPosition(uDepthTex, iSampleCoord);
        vec3 sn = V_GetViewNormal(uNormalTex, iSampleCoord);
        vec4 sc = textureLod(uSourceTex, fSampleCoord, 0);

        float planeDist = dot(sp - cp, cn);
        float normalDiff = 1.0 - dot(cn, sn);

        float w = exp(
            -dot(offset, offset) * uInvBlurRadius2
            -planeDist * planeDist * uDepthSharpness
            -normalDiff * normalDiff * uNormalSharpness
        );

        result += sc * w;
        weightSum += w;
    }

    FragColor = result / weightSum;
}
