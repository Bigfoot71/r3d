/* dof_down.frag -- Input buffers downsampling for DoF
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
uniform sampler2D uCoCTex;

out vec4 FragColor;
out float FragDepth;

void main()
{
    ivec2 pixCoord = 2 * ivec2(gl_FragCoord.xy);

    ivec2 p0 = pixCoord + ivec2(0, 0);
    ivec2 p1 = pixCoord + ivec2(1, 0);
    ivec2 p2 = pixCoord + ivec2(0, 1);
    ivec2 p3 = pixCoord + ivec2(1, 1);

    vec3 c0 = texelFetch(uSceneTex, p0, 0).rgb;
    vec3 c1 = texelFetch(uSceneTex, p1, 0).rgb;
    vec3 c2 = texelFetch(uSceneTex, p2, 0).rgb;
    vec3 c3 = texelFetch(uSceneTex, p3, 0).rgb;

    float d0 = texelFetch(uDepthTex, p0, 0).r;
    float d1 = texelFetch(uDepthTex, p1, 0).r;
    float d2 = texelFetch(uDepthTex, p2, 0).r;
    float d3 = texelFetch(uDepthTex, p3, 0).r;

    ivec2 selectedPixel = p0;
    float selectedDepth = d0;

    if (d1 < selectedDepth) { selectedPixel = p1; selectedDepth = d1; }
    if (d2 < selectedDepth) { selectedPixel = p2; selectedDepth = d2; }
    if (d3 < selectedDepth) { selectedPixel = p3; selectedDepth = d3; }

    float coc = texelFetch(uCoCTex, selectedPixel, 0).r;

    FragColor = vec4((c0 + c1 + c2 + c3) * 0.25, coc);
    FragDepth = selectedDepth;
}
