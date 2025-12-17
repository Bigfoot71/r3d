/* bloom.frag -- Fragment shader for applying bloom to the scene
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Definitions === */

#define BLOOM_MIX           1
#define BLOOM_ADDITIVE      2
#define BLOOM_SCREEN        3

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform sampler2D uTexBloomBlur;

uniform lowp int uBloomMode;
uniform float uBloomIntensity;

/* === Fragments === */

out vec3 FragColor;

/* === Main program === */

void main()
{
    // Sampling scene color texture
    vec3 color = texture(uTexColor, vTexCoord).rgb;

    // Apply bloom
    vec3 bloom = texture(uTexBloomBlur, vTexCoord).rgb;
    bloom *= uBloomIntensity;

    if (uBloomMode == BLOOM_MIX) {
        color = mix(color, bloom, uBloomIntensity);
    }
    else if (uBloomMode == BLOOM_ADDITIVE) {
        color += bloom;
    }
    else if (uBloomMode == BLOOM_SCREEN) {
        bloom = clamp(bloom, vec3(0.0), vec3(1.0));
        color = max((color + bloom) - (color * bloom), vec3(0.0));
    }

    // Final color output
    FragColor = vec3(color);
}
