/* dof_coc.frag -- Circle of Confusion calculation shader
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSceneTex;
uniform sampler2D uDepthTex;

uniform float uFocusPoint;
uniform float uFocusScale;

out vec4 FragColor;

float CalculateCoC(float depth)
{
    return clamp((1.0 / uFocusPoint - 1.0 / depth) * uFocusScale, -1.0, 1.0);
}

void main()
{
    vec3 color = texture(uSceneTex, vTexCoord).rgb;
    float depth = texture(uDepthTex, vTexCoord).r;
    float coc = CalculateCoC(depth);
    FragColor = vec4(color, coc);
}
