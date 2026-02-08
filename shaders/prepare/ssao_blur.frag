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

const float DEPTH_SHARPNESS = 200.0;

// Generated using the Two-pass Gaussian blur coeffifients generator made by Lisyarus
// Link: https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html

// Used parameters:
// Blur radius: 4
// Blur sigma: 2

const int SAMPLE_COUNT = 5;

const float OFFSETS[5] = float[5](
    -3.2979348079914823,
    -1.409199877085212,
    0.469433779698372,
    2.351564403533789,
    4
);

const float WEIGHTS[5] = float[5](
    0.095766798098283,
    0.3030531945966437,
    0.3814038792275628,
    0.19124386547413952,
    0.02853226260337099
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
