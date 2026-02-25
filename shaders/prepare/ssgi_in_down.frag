/* ssgi_in_down.frag - G-Buffer downsampling for SSGI
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

uniform usampler2D uSelectorTex;
uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;

layout(location = 0) out vec3 FragDiffuse;
layout(location = 1) out vec2 FragNormal;

const ivec2 OFFSETS[4] = ivec2[4](
    ivec2(0, 0), ivec2(1, 0),
    ivec2(0, 1), ivec2(1, 1)
);

void main()
{
    ivec2 upCoord = 2 * ivec2(gl_FragCoord.xy);
    ivec2 pxCoord =     ivec2(gl_FragCoord.xy);

    uint index = texelFetch(uSelectorTex, pxCoord, 0).r;

    FragDiffuse = texelFetch(uDiffuseTex, upCoord + OFFSETS[index], 0).rgb;
    FragNormal = texelFetch(uNormalTex, upCoord + OFFSETS[index], 0).rg;
}
