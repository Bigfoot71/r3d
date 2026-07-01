/* common_nearest.frag -- Blit with nearest scaling
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSourceTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;

void main()
{
    ivec2 sourceSize = textureSize(uSourceTex, 0);
    ivec2 pixCoord = ivec2(vTexCoord * vec2(sourceSize));
    pixCoord = clamp(pixCoord, ivec2(0), sourceSize - 1);

    FragColor = texelFetch(uSourceTex, pixCoord, 0);
    gl_FragDepth = texelFetch(uDepthTex, pixCoord, 0).r;
}
