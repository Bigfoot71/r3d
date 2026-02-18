/* smaa.frag -- Fragment shader of SMAA blend pass
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "../include/smaa.glsl"

uniform sampler2D uSceneTex;
uniform sampler2D uBlendTex;

noperspective in vec2 vTexCoord;
noperspective in vec4 vOffset;

out vec4 FragColor;

void main()
{
    FragColor = SMAANeighborhoodBlendingPS(vTexCoord, vOffset, uSceneTex, uBlendTex);
}
