/* common_copy.frag -- Fast blit copy
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// NOTE: Must only be used between two targets of the same dimensions

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSourceTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;

void main()
{
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);
    FragColor = texelFetch(uSourceTex, pixCoord, 0);
    gl_FragDepth = texelFetch(uDepthTex, pixCoord, 0).r;
}
