/* depth_pyramid.frag - Depth buffer downsampling shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

uniform sampler2D uDepthTex;

layout(location = 0) out float FragDepth;
layout(location = 1) out uint FragSelect;

void main()
{
    ivec2 pixCoord = 2 * ivec2(gl_FragCoord.xy);

    ivec2 p0 = pixCoord + ivec2(0, 0);
    ivec2 p1 = pixCoord + ivec2(1, 0);
    ivec2 p2 = pixCoord + ivec2(0, 1);
    ivec2 p3 = pixCoord + ivec2(1, 1);

    float d0 = texelFetch(uDepthTex, p0, 0).r;
    float d1 = texelFetch(uDepthTex, p1, 0).r;
    float d2 = texelFetch(uDepthTex, p2, 0).r;
    float d3 = texelFetch(uDepthTex, p3, 0).r;

    FragDepth = d0, FragSelect = 0u;

    if (d1 < FragDepth) { FragDepth = d1; FragSelect = 1u; }
    if (d2 < FragDepth) { FragDepth = d2; FragSelect = 2u; }
    if (d3 < FragDepth) { FragDepth = d3; FragSelect = 3u; }
}
