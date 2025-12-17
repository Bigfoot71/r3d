/* ssao_blur.frag -- SSAO bilateral blur (depth + normal aware)
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexOcclusion;
uniform sampler2D uTexNormal;
uniform sampler2D uTexDepth;

uniform mat4 uMatInvProj;
uniform vec2 uDirection;

/* === Fragments === */

out vec4 FragColor;

/* === Blur Coefficients === */

// NOTE: Generated using https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html

// Parameters:
//  - Radius: 5.0
//  - Sigma: 3.0

const int SAMPLE_COUNT = 6;

const float OFFSETS[6] = float[6](
    -4.378621204796657,
    -2.431625915613778,
    -0.4862426846689485,
    1.4588111840004858,
    3.4048471718931532,
    5
);

const float WEIGHTS[6] = float[6](
    0.09461172151436463,
    0.20023097066826712,
    0.2760751120037518,
    0.24804559825032563,
    0.14521459357563646,
    0.035822003987654526
);

const float NORMAL_POWER = 4.0;         // Controls normal similarity falloff (higher = stricter edge preservation, good range: 2.0-8.0)
const float DEPTH_SENSITIVITY = 2.0;    // Controls depth discontinuity tolerance (higher = more permissive blur across depths, good range: 1.0-5.0)
const float MIN_WEIGHT = 0.1;           // Minimum weight threshold to prevent complete isolation of pixels (ensures some blur even at sharp edges, range: 0.05-0.2)

/* === Helper Functions === */

vec3 ViewPositionFromDepth(vec2 uv, float depth)
{
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * clipPos;
    return viewPos.xyz / viewPos.w;
}

/* === Main Program === */

void main()
{
    vec4 centerColor = texture(uTexOcclusion, vTexCoord);
    float centerDepth = texture(uTexDepth, vTexCoord).r;

    if (centerDepth > 1.0 - 1e-5) {
        FragColor = centerColor;
        return;
    }

    vec3 centerNormal = M_DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);
    vec3 centerViewPos = ViewPositionFromDepth(vTexCoord, centerDepth);

    vec2 texelSize = 1.0 / vec2(textureSize(uTexOcclusion, 0));
    vec2 texelDir = uDirection * texelSize;

    float linearDepth = abs(centerViewPos.z);
    float adaptiveDepthSensitivity = DEPTH_SENSITIVITY * (1.0 + linearDepth * 0.05);

    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        vec2 sampleUV = vTexCoord + texelDir * OFFSETS[i];
        if (any(lessThan(sampleUV, vec2(0.0))) || any(greaterThan(sampleUV, vec2(1.0)))) {
            continue;
        }

        float sampleDepth = texture(uTexDepth, sampleUV).r;
        if (sampleDepth > 1.0 - 1e-5) continue;

        vec4 sampleColor = texture(uTexOcclusion, sampleUV);
        vec3 sampleNormal = M_DecodeOctahedral(texture(uTexNormal, sampleUV).rg);
        vec3 sampleViewPos = ViewPositionFromDepth(sampleUV, sampleDepth);

        float normalSimilarity = max(0.0, dot(centerNormal, sampleNormal));
        float normalWeight = pow(normalSimilarity, NORMAL_POWER);

        float depthDiff = abs(centerViewPos.z - sampleViewPos.z);
        float depthWeight = exp(-depthDiff / adaptiveDepthSensitivity);

        float bilateralWeight = normalWeight * depthWeight;
        float weight = WEIGHTS[i] * max(MIN_WEIGHT, bilateralWeight);

        result += sampleColor * weight;
        totalWeight += weight;
    }

    FragColor = totalWeight > 0.001 ? result / totalWeight : centerColor;
}
