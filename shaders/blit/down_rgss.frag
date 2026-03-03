/* down_rgss.frag -- Rotated Grid Super Sampling fragment shader
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

uniform sampler2D uSourceTex;
uniform vec2 uDestTexel;

/* === Fragments === */

out vec4 FragColor;

/* === Main program === */

void main()
{
    // Rotated grid at ~26.6°, rhombus covering [-3/8, 3/8] on X and Y

    const vec2 o0 = vec2(-3.0,  1.0) / 8.0;
    const vec2 o1 = vec2( 1.0,  3.0) / 8.0;
    const vec2 o2 = vec2( 3.0, -1.0) / 8.0;
    const vec2 o3 = vec2(-1.0, -3.0) / 8.0;

    vec4 c0 = texture(uSourceTex, vTexCoord + o0 * uDestTexel);
    vec4 c1 = texture(uSourceTex, vTexCoord + o1 * uDestTexel);
    vec4 c2 = texture(uSourceTex, vTexCoord + o2 * uDestTexel);
    vec4 c3 = texture(uSourceTex, vTexCoord + o3 * uDestTexel);

    FragColor = (c0 + c1 + c2 + c3) * 0.25;
}
