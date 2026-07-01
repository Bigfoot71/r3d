/* up_lanczos.frag -- Lanczos-2 upscaling fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// 12-tap Lanczos-2 approximation (4x4 grid minus its 4 outer corners).
// Corner taps are nulled via Lanczos(1.0) == 0.0, avoiding 4 extra fetches.

#version 330 core

#define M_PI  3.1415926535897931
#define M_HPI 1.5707963267948966

noperspective in vec2 vTexCoord;

uniform sampler2D uSourceTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;

vec4 Lanczos(vec4 x)
{
    x += 1e-5; // avoids 0/0 when a sample lands exactly on a texel center
    return sin(x * M_PI) * sin(x * M_HPI) / (x * x);
}

vec4 SampleLanczos(sampler2D tex, vec2 texCoord)
{
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 texel = 1.0 / texSize;

    vec2 dx = vec2(1.0, 0.0);
    vec2 dy = vec2(0.0, 1.0);

    vec2 pc = texCoord * texSize;
    vec2 tc = floor(pc - 0.5) + 0.5;

    mat4 w;
    w[0] = Lanczos(vec4(1.0, distance(pc, tc - dy), distance(pc, tc + dx - dy), 1.0));
    w[1] = Lanczos(vec4(distance(pc, tc - dx), distance(pc, tc), distance(pc, tc + dx), distance(pc, tc + 2.0 * dx)));
    w[2] = Lanczos(vec4(distance(pc, tc - dx + dy), distance(pc, tc + dy), distance(pc, tc + dx + dy), distance(pc, tc + 2.0 * dx + dy)));
    w[3] = Lanczos(vec4(1.0, distance(pc, tc + 2.0 * dy), distance(pc, tc + dx + 2.0 * dy), 1.0));

    dx *= texel;
    dy *= texel;
    tc *= texel;

    vec3 c10 = textureLod(tex, tc - dy, 0.0).rgb;
    vec3 c20 = textureLod(tex, tc + dx - dy, 0.0).rgb;
    vec3 c01 = textureLod(tex, tc - dx, 0.0).rgb;
    vec3 c11 = textureLod(tex, tc, 0.0).rgb;
    vec3 c21 = textureLod(tex, tc + dx, 0.0).rgb;
    vec3 c31 = textureLod(tex, tc + 2.0 * dx, 0.0).rgb;
    vec3 c02 = textureLod(tex, tc - dx + dy, 0.0).rgb;
    vec3 c12 = textureLod(tex, tc + dy, 0.0).rgb;
    vec3 c22 = textureLod(tex, tc + dx + dy, 0.0).rgb;
    vec3 c32 = textureLod(tex, tc + 2.0 * dx + dy, 0.0).rgb;
    vec3 c13 = textureLod(tex, tc + 2.0 * dy, 0.0).rgb;
    vec3 c23 = textureLod(tex, tc + dx + 2.0 * dy, 0.0).rgb;

    vec3 color =
        w[0][1] * c10 + w[0][2] * c20 +
        w[1][0] * c01 + w[1][1] * c11 + w[1][2] * c21 + w[1][3] * c31 +
        w[2][0] * c02 + w[2][1] * c12 + w[2][2] * c22 + w[2][3] * c32 +
        w[3][1] * c13 + w[3][2] * c23;

    float weightSum = 0.0;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            weightSum += w[i][j];

    return vec4(color / weightSum, 1.0);
}

void main()
{
    FragColor = SampleLanczos(uSourceTex, vTexCoord);
    gl_FragDepth = texture(uDepthTex, vTexCoord).r;
}
