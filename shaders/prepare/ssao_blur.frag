/* ssao_blur.frag -- SSAO bilateral blur fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSsaoTex;
uniform sampler2D uDepthTex;
uniform vec2 uDirection;

/* === Fragments === */

layout(location = 0) out float FragColor;

/* === Constants === */

const float DEPTH_SHARPNESS = 600.0;

// Generated using the Two-pass Gaussian blur coeffifients generator made by Lisyarus
// Link: https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html

// Used parameters:
// Blur radius: 3
// Blur sigma: 4

const int SAMPLE_COUNT = 4;

const float OFFSETS[4] = float[4](
    -2.4225046348141883,
    -0.4843800842769843,
    1.453261848015386,
    3
);

const float WEIGHTS[4] = float[4](
    0.24185531173974772,
    0.3478148096900533,
    0.3081448884057253,
    0.10218499016447377
);

/* === Main Functions === */

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uSsaoTex, 0));
    float centerDepth = texture(uDepthTex, vTexCoord).r;

    float result = 0.0;
    float wieghtSum = 0.0;

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        vec2 offset = uDirection * OFFSETS[i] * texelSize;
        float sampleAO = texture(uSsaoTex, vTexCoord + offset).r;
        float sampleDepth = texture(uDepthTex, vTexCoord + offset).r;

        float depthDiff = abs(centerDepth - sampleDepth);
        float depthWeight = exp(-depthDiff * depthDiff * DEPTH_SHARPNESS);
        float w = WEIGHTS[i] * depthWeight;

        result += sampleAO * w;
        wieghtSum += w;
    }

    FragColor = result / wieghtSum;
}
