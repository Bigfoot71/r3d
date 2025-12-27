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

#if defined(SSAO)

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

#elif defined(SSIL)

// Parameters:
//  - Correction: No
//  - Radius: 5.0
//  - Sigma: 4.5

const int SAMPLE_COUNT = 11;

const float OFFSETS[11] = float[11](
    -5,
    -4,
    -3,
    -2,
    -1,
    0,
    1,
    2,
    3,
    4,
    5
);

const float WEIGHTS[11] = float[11](
    0.03976498055891493,
    0.06201839806156053,
    0.08762883376041485,
    0.11217090845611094,
    0.13008288506268811,
    0.13666798820062134,
    0.13008288506268811,
    0.11217090845611094,
    0.08762883376041485,
    0.06201839806156053,
    0.03976498055891493
);

const float NORMAL_POWER = 2.0;
const float DEPTH_SENSITIVITY = 4.0;
const float MIN_WEIGHT = 0.3;

#endif

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

float SigmaScale(float viewDepth)
{
    float dist = abs(viewDepth);

    // Transition distance (in meters)
    const float D0 = 5.0;
    const float D1 = 20.0;

    // Maximum intensity of the far blur
    const float SIGMA_FAR = 2.5;

    float t = clamp((dist - D0) / (D1 - D0), 0.0, 1.0);
    return mix(1.0, SIGMA_FAR, t);
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

#ifdef SSIL
    float sigmaScale = SigmaScale(centerDepth);
    float maxOffset = abs(OFFSETS[SAMPLE_COUNT - 1]);
#endif

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

    #ifdef SSIL
        float tapDist = abs(OFFSETS[i]) / maxOffset;
        float spatialScale = mix(1.0, sigmaScale, tapDist);
        float w = WEIGHTS[i] * spatialScale * wNormal * wDepth;
    #else
        float w = WEIGHTS[i] * wNormal * wDepth;
    #endif

        w = max(w, MIN_WEIGHT * WEIGHTS[i]);
        result += sampleValue * w;
        totalWeight += w;
    }

    FragColor = result / max(totalWeight, 1e-4);
}
