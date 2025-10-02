/* depth_cube.frag -- Fragment shader used for omni-lights shadow mapping
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

in vec3 vPosition;
in vec2 vTexCoord;
in float vAlpha;

uniform sampler2D uTexAlbedo;

uniform float uAlphaCutoff;
uniform vec3 uViewPosition;
uniform float uFar;

void main()
{
    float alpha = vAlpha * texture(uTexAlbedo, vTexCoord).a;
    if (alpha < uAlphaCutoff) discard;

    gl_FragDepth = length(vPosition - uViewPosition) / uFar;
}
