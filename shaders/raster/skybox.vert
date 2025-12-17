/* skybox.vert -- Vertex shader used to render skyboxes
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Attributes === */

layout (location = 0) in vec3 aPosition;

/* === Uniforms === */

uniform mat4 uMatProj;
uniform mat4 uMatView;
uniform vec4 uRotation;

/* === Varyings === */

out vec3 vPosition;

/* === Program === */

void main()
{
    vPosition = M_Rotate3D(aPosition, uRotation);

    mat4 rotView = mat4(mat3(uMatView));
    gl_Position = (uMatProj * rotView * vec4(aPosition, 1.0)).xyww;
}
