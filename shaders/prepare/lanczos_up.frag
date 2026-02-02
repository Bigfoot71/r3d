/* lanczos_up.frag -- Lanczos-2 upscaling fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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

uniform sampler2D uSourceTex;
uniform vec2 uSourceTexel;

/* === Fragments === */

out vec4 FragColor;

/* === Lanczos-2 filter === */

vec4 Lanczos(vec4 x)
{
    // Lanczos windowed sinc function
    // sinc(x) * sinc(x/2) where sinc(x) = sin(M_PI*x)/(M_PI*x)
    return (x == vec4(0.0)) ? vec4(M_PI * M_HPI) : sin(x * M_HPI) * sin(x * M_PI) / (x * x);
}

/* === Main program === */

void main()
{
    vec3 color;
    mat4 weights;

    vec2 dx = vec2(1.0, 0.0);
    vec2 dy = vec2(0.0, 1.0);

    vec2 pc = vTexCoord * vec2(textureSize(uSourceTex, 0));
    vec2 tc = floor(pc - vec2(0.5, 0.5)) + vec2(0.5, 0.5);

    weights[0] = Lanczos(vec4(1.0, distance(pc, tc - dy), distance(pc, tc + dx - dy), 1.0));
    weights[1] = Lanczos(vec4(distance(pc, tc - dx), distance(pc, tc), distance(pc, tc + dx), distance(pc, tc + 2.0 * dx)));
    weights[2] = Lanczos(vec4(distance(pc, tc - dx + dy), distance(pc, tc + dy), distance(pc, tc + dx + dy), distance(pc, tc + 2.0 * dx + dy)));
    weights[3] = Lanczos(vec4(1.0, distance(pc, tc + 2.0 * dy), distance(pc, tc + dx + 2.0 * dy), 1.0));

    dx = dx * uSourceTexel;
    dy = dy * uSourceTexel;
    tc = tc * uSourceTexel;

    vec3 c10 = texture(uSourceTex, tc - dy).rgb;
    vec3 c20 = texture(uSourceTex, tc + dx - dy).rgb;
    vec3 c01 = texture(uSourceTex, tc - dx).rgb;
    vec3 c11 = texture(uSourceTex, tc).rgb;
    vec3 c21 = texture(uSourceTex, tc + dx).rgb;
    vec3 c31 = texture(uSourceTex, tc + 2.0 * dx).rgb;
    vec3 c02 = texture(uSourceTex, tc - dx + dy).rgb;
    vec3 c12 = texture(uSourceTex, tc + dy).rgb;
    vec3 c22 = texture(uSourceTex, tc + dx + dy).rgb;
    vec3 c32 = texture(uSourceTex, tc + 2.0 * dx + dy).rgb;
    vec3 c13 = texture(uSourceTex, tc + 2.0 * dy).rgb;
    vec3 c23 = texture(uSourceTex, tc + dx + 2.0 * dy).rgb;

    vec3 o = vec3(0.0);

    color  = weights[0][0] * o   + weights[0][1] * c10 + weights[0][2] * c20 + weights[0][3] * o;
    color += weights[1][0] * c01 + weights[1][1] * c11 + weights[1][2] * c21 + weights[1][3] * c31;
    color += weights[2][0] * c02 + weights[2][1] * c12 + weights[2][2] * c22 + weights[2][3] * c32;
    color += weights[3][0] * o   + weights[3][1] * c13 + weights[3][2] * c23 + weights[3][3] * o;

    float weightSum = 0.0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            weightSum += weights[i][j];
        }
    }

    FragColor = vec4(color / weightSum, 1.0);
}
