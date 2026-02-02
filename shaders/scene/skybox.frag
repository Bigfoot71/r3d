/* skybox.frag -- Fragment shader used to render skyboxes
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

in vec3 vViewRay;

/* === Uniforms === */

uniform samplerCube uSkyMap;
uniform vec4 uRotation;
uniform float uEnergy;
uniform float uLod;

/* === Fragments === */

layout(location = 0) out vec3 FragColor;

/* === Program === */

void main()
{
    vec3 direction = normalize(vViewRay);
    direction = M_Rotate3D(direction, uRotation);
    FragColor = textureLod(uSkyMap, direction, uLod).rgb * uEnergy;
}
