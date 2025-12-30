/* bilateral_blur.frag -- Bilateral blur (depth + normal aware) used for SSAO and SSIL
 *
 * Copyright (c) 2025 Le Juez Victor
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

uniform sampler2D uTexSource;
uniform sampler2D uTexNormal;
uniform sampler2D uTexDepth;

uniform vec2 uDirection;

/* === Fragments === */

out vec4 FragColor;

/* === Blur Coefficients === */

// NOTE: Generated using https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html

// Parameters:
//  - Correction: Yes
//  - Radius: 3.0
//  - Sigma: 1.4

const int SAMPLE_COUNT = 7;

const float OFFSETS[7] = float[7](
    -3,
    -2,
    -1,
    0,
    1,
    2,
    3
);

const float WEIGHTS[7] = float[7](
    0.031251155234634016,
    0.10623507713342698,
    0.2212518798870372,
    0.28252377548980373,
    0.2212518798870372,
    0.10623507713342698,
    0.031251155234634016
);

const float NORMAL_POWER = 6.0;         // Controls normal similarity falloff (higher = stricter edge preservation)
const float DEPTH_SENSITIVITY = 3.0;    // Controls depth discontinuity tolerance (higher = more permissive blur across depths)
const float MIN_WEIGHT = 0.0;           // Minimum weight threshold to prevent complete isolation of pixels (ensures some blur even at sharp edges)

/* === Helper Functions === */

float NormalWeight(vec3 n0, vec3 n1)
{
    float d = max(dot(n0, n1), 0.0);
    return pow(d, NORMAL_POWER);
}

float DepthWeight(float d0, float d1)
{
    float diff = abs(d0 - d1);
    return exp(-diff * DEPTH_SENSITIVITY);
}

/* === Main Program === */

void main()
{
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    vec2 size = textureSize(uTexSource, 0);

    vec3 centerNormal = V_GetViewNormal(uTexNormal, vTexCoord);
    vec3 centerPos = V_GetViewPosition(uTexDepth, vTexCoord);
    float centerDepth = centerPos.z;

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        vec2 offset = uDirection * OFFSETS[i] / size;
        vec2 uv = vTexCoord + offset;
        if (V_OffScreen(uv)) continue;

        vec4 sampleValue = texture(uTexSource, uv);

        vec3 sampleNormal = V_GetViewNormal(uTexNormal, uv);
        vec3 samplePos = V_GetViewPosition(uTexDepth, uv);
        float sampleDepth = samplePos.z;

        float wNormal = NormalWeight(centerNormal, sampleNormal);
        float wDepth = DepthWeight(centerDepth, sampleDepth);

        float w = WEIGHTS[i] * wNormal * wDepth;
        w = max(w, MIN_WEIGHT * WEIGHTS[i]);

        result += sampleValue * w;
        totalWeight += w;
    }

    FragColor = result / max(totalWeight, 1e-4);
}
