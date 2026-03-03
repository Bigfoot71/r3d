/* down_pdss.frag -- Poisson Disk Super Sampling fragment shader
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
    const vec2 o00 = vec2( 0.176777,  0.000000) * 0.5;
    const vec2 o01 = vec2(-0.225772,  0.206826) * 0.5;
    const vec2 o02 = vec2( 0.034558, -0.393771) * 0.5;
    const vec2 o03 = vec2( 0.284571,  0.371173) * 0.5;
    const vec2 o04 = vec2(-0.522223, -0.092374) * 0.5;
    const vec2 o05 = vec2( 0.494695, -0.314685) * 0.5;
    const vec2 o06 = vec2(-0.165466,  0.615525) * 0.5;
    const vec2 o07 = vec2(-0.315561, -0.607594) * 0.5;
    const vec2 o08 = vec2( 0.684642,  0.250030) * 0.5;
    const vec2 o09 = vec2(-0.712256,  0.294009) * 0.5;
    const vec2 o10 = vec2( 0.343354, -0.733729) * 0.5;
    const vec2 o11 = vec2( 0.253730,  0.808932) * 0.5;
    const vec2 o12 = vec2(-0.764746, -0.443186) * 0.5;
    const vec2 o13 = vec2( 0.897134, -0.197232) * 0.5;
    const vec2 o14 = vec2(-0.547507,  0.778772) * 0.5;
    const vec2 o15 = vec2(-0.126487, -0.976090) * 0.5;

    vec4 c  = texture(uSourceTex, vTexCoord + o00 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o01 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o02 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o03 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o04 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o05 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o06 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o07 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o08 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o09 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o10 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o11 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o12 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o13 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o14 * uDestTexel);
         c += texture(uSourceTex, vTexCoord + o15 * uDestTexel);

    FragColor = c * (1.0 / 16.0);
}
