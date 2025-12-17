/* dof.frag -- Fragment shader implementing a depth of field effect
 *
 * Originally written by Dennis Gustafsson.
 * Feature adaptation and extension by Jens Roth.
 *
 * Provides a post-processing effect that simulates camera focus,
 * blending sharp and blurred regions according to scene depth.
 *
 * Copyright (c) 2025 Victor Le Juez, Jens Roth
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

// reference impl. from SmashHit iOS game dev blog
// see: https://blog.voxagon.se/2018/05/04/bokeh-depth-of-field-in-single-pass.html

#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform sampler2D uTexDepth;

uniform vec2 uTexelSize;
uniform float uNear;
uniform float uFar;

uniform float uFocusPoint;
uniform float uFocusScale;
uniform float uMaxBlurSize;

uniform int uDebugMode;         //< 0 off, 1 green/black/blue

/* === Output === */

out vec4 FragColor;

const float GOLDEN_ANGLE = 2.39996323;
const float RAD_SCALE = 0.5;            //< Smaller = nicer blur, larger = faster

/* === Helpers === */

float LinearizeDepth(float depth)
{
    return (2.0 * uNear * uFar) / (uFar + uNear - (2.0 * depth - 1.0) * (uFar - uNear));;
}

float GetBlurSize(float depth)
{
    float coc = clamp((1.0 / uFocusPoint - 1.0 / depth) * uFocusScale, -1.0, 1.0);
    return abs(coc) * uMaxBlurSize;
}

/* === Main === */

void main()
{
    vec3 color = texture(uTexColor, vTexCoord).rgb;

    // Center depth and CoC
    float centerDepth = LinearizeDepth(texture(uTexDepth, vTexCoord).r);
    float centerSize  = GetBlurSize(centerDepth);

    //scatter as gather
    float tot = 1.0;

    float radius = RAD_SCALE;
    for (float ang = 0.0; radius < uMaxBlurSize; ang += GOLDEN_ANGLE)
    {
        vec2 tc = vTexCoord + vec2(cos(ang), sin(ang)) * uTexelSize * radius;

        vec3 sampleColor = texture(uTexColor, tc).rgb;
        float sampleDepth = LinearizeDepth(texture(uTexDepth, tc).r);
        float sampleSize  = GetBlurSize(sampleDepth);

        if (sampleDepth > centerDepth) {
            sampleSize = clamp(sampleSize, 0.0, centerSize * 2.0);
        }

        float m = smoothstep(radius - 0.5, radius + 0.5, sampleSize);
        color += mix(color / tot, sampleColor, m);
        radius += RAD_SCALE / max(radius, 0.001);
        tot += 1.0;
    }

    FragColor = vec4(color / tot, 1.0);

    /* --- Debug output --- */

    if (uDebugMode == 1) {
        float cocSigned = clamp((1.0 / uFocusPoint - 1.0 / centerDepth) * uFocusScale, -1.0, 1.0);
        float front = clamp(-cocSigned, 0.0, 1.0); // in front of focus plane (near)
        float back = clamp(cocSigned, 0.0, 1.0); // behind the focus plane (far)
        vec3 tint = vec3(0.0, front, back); // green front, blue back, black at focus
        FragColor = vec4(tint, 1.0);
    }
}
