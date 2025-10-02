/* screen.vert -- Generic vertex shader for rendering a full-screen quad
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

const vec2 positions[3] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

noperspective out vec2 vTexCoord;

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    vTexCoord = (gl_Position.xy * 0.5) + 0.5;
}
