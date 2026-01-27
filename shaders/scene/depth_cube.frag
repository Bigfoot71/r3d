/* depth_cube.frag -- Fragment shader used for omni-lights shadow mapping
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings === */

smooth in vec3 vPosition;
smooth in vec2 vTexCoord;
smooth in vec4 vColor;

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform float uAlphaCutoff;
uniform vec3 uViewPosition;
uniform float uFar;

/* === User override === */

#include "../include/user/scene.frag"

/* === Main function === */

void main()
{
    SceneFragment(vTexCoord, uAlphaCutoff);
    gl_FragDepth = length(vPosition - uViewPosition) / uFar;
}
