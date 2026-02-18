/* smaa_edge_detection.frag -- SMAA blending weigths calculation pass
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

uniform sampler2D uEdgesTex;
uniform sampler2D uAreaTex;
uniform sampler2D uSearchTex;

noperspective in vec2 vTexCoord;
noperspective in vec2 vPixCoord;
noperspective in vec4 vOffset[3];

out vec4 FragColor;

void main()
{
    FragColor = SMAABlendingWeightCalculationPS(
        vTexCoord, vPixCoord, vOffset,
        uEdgesTex, uAreaTex, uSearchTex,
        ivec4(0) // subsample indices, 0 for SMAA 1x
    );
}
