/* smaa_edge_detection.frag -- SMAA edge detection pass
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

noperspective in vec2 vTexCoord;
noperspective in vec4 vOffset[3];

out vec2 FragColor;

void main()
{
    FragColor = SMAALumaEdgeDetectionPS(vTexCoord, vOffset, uSceneTex);
    // or SMAAColorEdgeDetectionPS / SMAADepthEdgeDetectionPS
}
