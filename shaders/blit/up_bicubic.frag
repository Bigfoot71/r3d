/* up_bicubic.frag -- Catmull-Rom bicubic upscaling fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Catmull-Rom bicubic filtering (B=0, C=0.5) using 9 bilinear taps.
// Based on: https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSourceTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;

vec4 SampleBicubic(sampler2D tex, vec2 uv)
{
    vec2 texSize = vec2(textureSize(tex, 0));
    vec2 texel = 1.0 / texSize;

    vec2 pos = uv * texSize;
    vec2 tc = floor(pos - 0.5) + 0.5;
    vec2 f = pos - tc;
    vec2 f2 = f * f;
    vec2 f3 = f2 * f;

    vec2 w0 = f2 - 0.5 * (f3 + f);
    vec2 w1 = 1.5 * f3 - 2.5 * f2 + 1.0;
    vec2 w3 = 0.5 * (f3 - f2);
    vec2 w2 = 1.0 - w0 - w1 - w3;

    vec2 w12 = w1 + w2;
    vec2 tc0  = (tc - 1.0) * texel;
    vec2 tc3  = (tc + 2.0) * texel;
    vec2 tc12 = (tc + w2 / w12) * texel;

    vec4 c = vec4(0.0);
    c += textureLod(tex, vec2(tc0.x,  tc0.y),  0.0) * w0.x  * w0.y;
    c += textureLod(tex, vec2(tc12.x, tc0.y),  0.0) * w12.x * w0.y;
    c += textureLod(tex, vec2(tc3.x,  tc0.y),  0.0) * w3.x  * w0.y;

    c += textureLod(tex, vec2(tc0.x,  tc12.y), 0.0) * w0.x  * w12.y;
    c += textureLod(tex, vec2(tc12.x, tc12.y), 0.0) * w12.x * w12.y;
    c += textureLod(tex, vec2(tc3.x,  tc12.y), 0.0) * w3.x  * w12.y;

    c += textureLod(tex, vec2(tc0.x,  tc3.y),  0.0) * w0.x  * w3.y;
    c += textureLod(tex, vec2(tc12.x, tc3.y),  0.0) * w12.x * w3.y;
    c += textureLod(tex, vec2(tc3.x,  tc3.y),  0.0) * w3.x  * w3.y;

    return c;
}

void main()
{
    FragColor = SampleBicubic(uSourceTex, vTexCoord);
    gl_FragDepth = texture(uDepthTex, vTexCoord).r;
}
