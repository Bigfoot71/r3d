/* dof_down.frag -- Input buffers downsampling for DoF
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uCoCTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;
out float FragDepth;

void main()
{
    ivec2 pixCoord = 2 * ivec2(gl_FragCoord.xy);

    ivec2 p0 = pixCoord + ivec2(0, 0);
    ivec2 p1 = pixCoord + ivec2(1, 0);
    ivec2 p2 = pixCoord + ivec2(0, 1);
    ivec2 p3 = pixCoord + ivec2(1, 1);

    vec4 c0 = texelFetch(uCoCTex, p0, 0);
    vec4 c1 = texelFetch(uCoCTex, p1, 0);
    vec4 c2 = texelFetch(uCoCTex, p2, 0);
    vec4 c3 = texelFetch(uCoCTex, p3, 0);

    float d0 = texelFetch(uDepthTex, p0, 0).r;
    float d1 = texelFetch(uDepthTex, p1, 0).r;
    float d2 = texelFetch(uDepthTex, p2, 0).r;
    float d3 = texelFetch(uDepthTex, p3, 0).r;

    float selectedCoC = c0.w;
    float selectedDepth = d0;

    if (d1 < selectedDepth) { selectedCoC = c1.w; selectedDepth = d1; }
    if (d2 < selectedDepth) { selectedCoC = c2.w; selectedDepth = d2; }
    if (d3 < selectedDepth) { selectedCoC = c3.w; selectedDepth = d3; }

    vec3 color = (c0.rgb + c1.rgb + c2.rgb + c3.rgb) * 0.25;

    FragColor = vec4(color, selectedCoC);
    FragDepth = selectedDepth;
}
