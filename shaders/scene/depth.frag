/* depth.frag -- Fragment shader used for dir/spot-lights shadow mapping
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

/* === Main function === */

void main()
{
    // NOTE: The depth is automatically written

    float alpha = vColor.a * texture(uAlbedoMap, vTexCoord).a;
    if (alpha < uAlphaCutoff) discard;
}
