/* down_rgss.frag -- Rotated Grid Super Sampling fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSourceTex;
uniform sampler2D uDepthTex;
uniform vec2 uDestTexel;

out vec4 FragColor;

vec4 SampleColor(sampler2D tex, vec2 texCoord, vec2 dstTexel)
{
    // Rotated grid at ~26.6°, rhombus covering [-3/8, 3/8] on X and Y

    const vec2 o0 = vec2(-3.0,  1.0) / 8.0;
    const vec2 o1 = vec2( 1.0,  3.0) / 8.0;
    const vec2 o2 = vec2( 3.0, -1.0) / 8.0;
    const vec2 o3 = vec2(-1.0, -3.0) / 8.0;

    vec4 c0 = texture(tex, texCoord + o0 * dstTexel);
    vec4 c1 = texture(tex, texCoord + o1 * dstTexel);
    vec4 c2 = texture(tex, texCoord + o2 * dstTexel);
    vec4 c3 = texture(tex, texCoord + o3 * dstTexel);

    return (c0 + c1 + c2 + c3) * 0.25;
}

void main()
{
    FragColor = SampleColor(uSourceTex, vTexCoord, uDestTexel);
    gl_FragDepth = texture(uDepthTex, vTexCoord).r;
}
