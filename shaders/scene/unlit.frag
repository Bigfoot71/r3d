/* unlit.frag -- Fragment shader used for unlit objects
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings === */

smooth in vec2 vTexCoord;
smooth in vec4 vColor;

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform float uAlphaCutoff;

/* === Fragments === */

layout(location = 0) out vec4 FragColor;

/* === User override === */

#include "../include/user/scene.frag"

/* === Main function === */

void main()
{
    SceneFragment(vTexCoord, mat3(1.0), uAlphaCutoff);
    FragColor = vec4(ALBEDO, ALPHA);
}
