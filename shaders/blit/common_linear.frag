/* common_linear.frag -- Blit with linear scaling
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
    FragColor = texture(uSourceTex, vTexCoord);
    gl_FragDepth = texture(uDepthTex, vTexCoord).r;
}
