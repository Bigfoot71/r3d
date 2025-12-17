/* depth_volume.vert -- Vertex shader used to draw volume in depth or stencil buffers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Attributes === */

layout(location = 0) in vec3 aPosition;

/* === Uniforms === */

uniform mat4 uMatMVP;

/* === Main function === */

void main()
{
    gl_Position = uMatMVP * vec4(aPosition, 1.0);
}
