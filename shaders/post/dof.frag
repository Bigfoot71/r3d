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

void main()
{
    // There can be a tiny 1-pixel "bleeding" around sharp edges.
    // This comes from the half-res linear filtering of sharp pixels during upsampling, not from the blur itself.
    // One way to completely eliminate this bleeding is to use texelFetch but this creates a blocky pixelated look.

    vec4 sharp = texelFetch(uSceneTex, ivec2(gl_FragCoord), 0);
    vec4 blur = texture(uBlurTex, vTexCoord);

    FragColor = vec4(mix(sharp.rgb, blur.rgb, blur.a), 1.0);
}
