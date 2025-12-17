/* ssao_blur.frag -- Bilateral depth-aware blur fragment shader for SSAO
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// NOTE: The coefficients for the two-pass Gaussian blur were generated using:
//       https://lisyarus.github.io/blog/posts/blur-coefficients-generator.html

#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexture;
uniform vec2 uTexelDir;

/* === Fragments === */

out vec4 FragColor;

/* === Blur Coefs === */

const int SAMPLE_COUNT = 6;

const float OFFSETS[6] = float[6](
    -4.455269417428358,
    -2.4751038298192056,
    -0.4950160492928827,
    1.485055021558738,
    3.465172537482815,
    5
);

const float WEIGHTS[6] = float[6](
    0.14587920530480702,
    0.19230308352110734,
    0.21647621943673803,
    0.20809835496561988,
    0.17082879595769634,
    0.06641434081403137
);

/* === Main Program === */

void main()
{
    vec3 result = vec3(0.0);

    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        result += texture(uTexture, vTexCoord + uTexelDir * OFFSETS[i]).rgb * WEIGHTS[i];
    }

    FragColor = vec4(result, 1.0);
}
