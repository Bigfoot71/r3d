/* dof.frag -- Depth of Field composition shader
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

noperspective in vec2 vTexCoord;

uniform sampler2D uSceneTex;
uniform sampler2D uBlurTex;

out vec4 FragColor;

vec4 Upsample(sampler2D tex)
{
    // There can be a tiny 1-pixel "bleeding" around sharp edges.
    // This comes from the half-res linear filtering of sharp pixels during upsampling,
    // not from the blur itself.
    //
    // One way to completely eliminate this bleeding is to use texelFetch:
    // it samples exactly the center of the half-res pixel, but this creates a blocky pixelated look.
    //
    // The approach below uses a linear sample centered on the half-res pixel,
    // which keeps the smoothness of the image but can leave a very subtle bleeding at edges.

    //ivec2 halfPix = ivec2(gl_FragCoord.xy * 0.5);
    //return texelFetch(tex, halfPix, 0);

    vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    vec2 uv = (gl_FragCoord.xy * 0.5 + 0.25) * texel;
    return texture(tex, uv);
}

void main()
{
    vec4 sharp = texelFetch(uSceneTex, ivec2(gl_FragCoord), 0);
    vec4 blur = Upsample(uBlurTex);

    FragColor = vec4(mix(sharp.rgb, blur.rgb, blur.a), 1.0);
}
