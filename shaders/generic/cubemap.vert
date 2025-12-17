/* cube.vert -- Generic vertex shader for rendering a cube
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

layout (location = 0) in vec3 aPosition;

uniform mat4 uMatProj;
uniform mat4 uMatView;

out vec3 vPosition;

void main()
{
    vPosition = aPosition;
    gl_Position = uMatProj * uMatView * vec4(vPosition, 1.0);
}
