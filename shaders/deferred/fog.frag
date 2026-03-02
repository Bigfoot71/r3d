/* fog.frag -- Fragment shader for applying fog to the scene
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

#include "../include/blocks/fog.glsl"

noperspective in vec2 vTexCoord;
uniform sampler2D uDepthTex;
out vec4 FragColor;

void main()
{
    float depth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    FragColor = FogColorAlpha(depth);
}
