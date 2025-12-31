/* atrous_wavelet.frag -- À-Trous Wavelet denoiser (depth + normal aware)
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

uniform int uStepSize;  // Powers of 2: 1, 2, 4, 8... for each pass

/* === Fragments === */

out vec4 FragColor;

/* === À-Trous Kernel === */

const int KERNEL_SIZE = 5;

const ivec2 OFFSETS[5] = ivec2[5](
    ivec2( 0,  0),  // Center
    ivec2(-1,  0),  // Left
    ivec2( 1,  0),  // Right
    ivec2( 0, -1),  // Up
    ivec2( 0,  1)   // Down
);

const float WEIGHTS[5] = float[5](
    0.5,    // Center
    0.125,  // Left
    0.125,  // Right
    0.125,  // Up
    0.125   // Down
);

/*
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
*/

/* === Parameters === */

const float NORMAL_POWER = 6.0;         // Controls normal similarity falloff
const float DEPTH_SENSITIVITY = 3.0;    // Controls depth discontinuity tolerance

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

    vec2 texelSize = 1.0 / vec2(textureSize(uTexSource, 0));
    vec3 centerNormal = V_GetViewNormal(uTexNormal, vTexCoord);
    float centerDepth = V_GetLinearDepth(uTexDepth, vTexCoord);

    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        vec2 offset = vec2(OFFSETS[i] * uStepSize) * texelSize;
        vec2 uv = vTexCoord + offset;

        vec4 sampleValue = texture(uTexSource, uv);
        vec3 sampleNormal = V_GetViewNormal(uTexNormal, uv);
        float sampleDepth = V_GetLinearDepth(uTexDepth, uv);

        float wNormal = NormalWeight(centerNormal, sampleNormal);
        float wDepth = DepthWeight(centerDepth, sampleDepth);
        float w = WEIGHTS[i] * wNormal * wDepth;

        result += sampleValue * w;
        totalWeight += w;
    }

    FragColor = result / max(totalWeight, 1e-4);
}
