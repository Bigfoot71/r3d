/* dof_blur.frag -- DoF in single pass
 *
 * Depth of Field done in a single pass.
 * Originally written by Dennis Gustafsson.
 *
 * Copyright (c) 2025 Victor Le Juez, Jens Roth
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

// reference impl. from SmashHit iOS game dev blog
// see: https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html

#version 330 core

#include "../include/math.glsl"

noperspective in vec2 vTexCoord;

uniform sampler2D uCoCTex;
uniform sampler2D uDepthTex;
uniform float uMaxBlurSize;

const float RAD_SCALE = 0.5;

out vec4 FragColor;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(uCoCTex, 0));

    vec4 centerData = texture(uCoCTex, vTexCoord);
    float centerDepth = texture(uDepthTex, vTexCoord).r;
    float centerSize = abs(centerData.a) * uMaxBlurSize;

    vec3 color = centerData.rgb;
    float radius = RAD_SCALE;
    float tot = 1.0;

    for (float ang = 0.0; radius < uMaxBlurSize; ang += M_GOLDEN_ANGLE)
    {
        vec2 uv = vTexCoord + vec2(cos(ang), sin(ang)) * texelSize * radius;

        vec4 sampleData = texture(uCoCTex, uv);
        float sampleDepth = texture(uDepthTex, uv).r;
        float sampleSize = abs(sampleData.a) * uMaxBlurSize;

        if (sampleDepth > centerDepth) {
            sampleSize = clamp(sampleSize, 0.0, centerSize * 2.0);
        }

        float w = smoothstep(radius - 0.5, radius + 0.5, sampleSize);
        color += mix(color / tot, sampleData.rgb, w);

        radius += RAD_SCALE / radius;
        tot += 1.0;
    }

    // Compute a Gaussian-like weight from the central CoC to produce a blur probability for this pixel.
    // After extensive testing, this gives the most physically plausible blending while remaining simple.

    float w = 1.0 - exp(-0.5 * centerSize * centerSize);
    FragColor = vec4(color / tot, clamp(w, 0.0, 1.0));
}
