/* smaa_edge_detection.vert -- SMAA edge detection pass
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0

#include "../include/smaa.glsl"

const vec2 positions[3] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

noperspective out vec2 vTexCoord;
noperspective out vec4 vOffset[3];

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 1.0, 1.0);
    vTexCoord = (gl_Position.xy * 0.5) + 0.5;

    SMAAEdgeDetectionVS(vTexCoord, vOffset);
}
