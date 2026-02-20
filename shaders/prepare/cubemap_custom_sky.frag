/* screen.frag -- Base of custom screen fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/frame.glsl"
#include "../include/blocks/view.glsl"

/* === Varyings === */

in vec3 vPosition;
in vec2 vTexCoord;

/* === Fragments === */

out vec4 FragColor;

/* === Built-In Input Variables === */

vec3 POSITION = vec3(0.0);
vec2 TEXCOORD = vec2(0.0);
vec3 EYEDIR = vec3(0.0);
int FRAME_INDEX = 0;
float TIME = 0.0;

/* === Built-In Output Variables === */

vec3 COLOR = vec3(0.0);

/* === Main function === */

vec2 GetSphericalCoord(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, -0.3183); // negative Y, to flip axis
    uv += 0.5;
    return uv;
}

/* === Main function === */

#define fragment()

void main()
{
    POSITION = vPosition;
    TEXCOORD = vTexCoord;
    EYEDIR = normalize(vPosition);
    FRAME_INDEX = uFrame.index;
    TIME = uFrame.time;

    fragment();

    FragColor = vec4(COLOR, 1.0);
}
