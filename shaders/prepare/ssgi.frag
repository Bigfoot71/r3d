/* ssgi.frag -- Screen Space Global Illumination fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

// Adapted from SSVGI by Alexander Sannikov, available in his LegitEngine repository:
// SEE: https://github.com/Raikiri/LegitEngine/tree/master/bin/data/Shaders/glsl/SSVGI

/* === Includes === */

#include <wrap/view.glsl>
#include <lib/math.glsl>
#include <ubo/fx.glsl>

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

out vec4 FragColor;

/* === Helper Functions === */

// Analytically integrates dot(N,w)*sin(th) over the horizon
// arc [h0, h1] in the slice plane defined by eyeDir and tangent
float HorizonContribution(float NdotE, float NdotT, float h0, float h1)
{
    return 0.25 * NdotE * (cos(2.0 * h0) - cos(2.0 * h1))
         + 0.25 * NdotT * (2.0 * (h1 - h0) - sin(2.0 * h1) + sin(2.0 * h0));
}

/* === Main Function === */

void main()
{
    vec2 viewport = vec2(textureSize(uDiffuseTex, 0));
    vec2 invViewport = 1.0 / viewport;

    // Receiver geometry
    float depth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    vec3 viewPos = V_GetViewPosition(vTexCoord, depth);
    vec3 viewNorm = V_GetViewNormal(uNormalTex, vTexCoord);
    vec3 eyeDir = normalize(-viewPos);
    float NdotE = dot(viewNorm, eyeDir);

    // Per-pixel jitter
    float jitter = M_HashIGN(gl_FragCoord.xy);
    float angOffset = M_TAU * jitter;
    float linOffset = jitter;

    // Exponential step size parameters
    float startStep = viewport.x / 1000.0;
    float invStartStep = 1.0 / startStep;
    float stepGrowth = M_TAU / float(uSsgi.sliceCount) + 1.0;
    float invLogStepGrowth = 1.0 / log(stepGrowth);
    float pixelDistBase = startStep * pow(stepGrowth, linOffset);
    float pixelDistOffset = 1.0 - startStep;

    // Slice constants
    float sliceStep = M_TAU / float(uSsgi.sliceCount);
    float sliceWeight = 2.0 / float(uSsgi.sliceCount);

    vec3 gi = vec3(0.0);

    for (int slice = 0; slice < uSsgi.sliceCount; slice++)
    {
        // Slice direction in screen space
        float sliceAngle = angOffset + sliceStep * float(slice);
        vec2 sliceDir = vec2(cos(sliceAngle), sin(sliceAngle));

        // Tangent in view space derived from a small screen-space offset at the same depth
        vec2 offsetUV = (gl_FragCoord.xy + sliceDir * 0.1) * invViewport;
        vec3 offsetPos = V_GetViewPosition(offsetUV, depth);
        vec3 tangent = normalize(normalize(offsetPos) + eyeDir);
        float NdotT = dot(viewNorm, tangent);

        // Initial horizon angle: projection of the surface normal into the slice plane
        float horizonAngle = atan(NdotE, -NdotT);

        // Distance in pixels to the nearest screen edge along sliceDir
        vec2 t = mix((viewport - gl_FragCoord.xy) / sliceDir, -gl_FragCoord.xy / sliceDir, lessThan(sliceDir, vec2(0.0)));
        int stepCount = int(log(max(min(t.x, t.y), 1.0) * invStartStep) * invLogStepGrowth) + 1;

        vec3 giSlice = vec3(0.0);
        float pixelDistMult = 1.0;

        for (int step = 0; step < stepCount; step++)
        {
            // Exponentially growing pixel offset + linear jitter
            float pixelDist = pixelDistBase * pixelDistMult + pixelDistOffset;
            pixelDistMult *= stepGrowth;

            vec2 sampleUV = (gl_FragCoord.xy + sliceDir * pixelDist) * invViewport;
            vec3 sampleViewPos = V_GetViewPosition(uDepthTex, sampleUV);
            vec3 sampleNorm = V_GetViewNormal(uNormalTex, sampleUV);
            vec3 delta = sampleViewPos - viewPos;

            // Skip samples on the same continuous surface (coplanar + similar normal)
            if (abs(dot(delta, viewNorm)) < 0.03 && dot(sampleNorm, viewNorm) > 0.95) continue;

            float sampleAngle = atan(dot(tangent, delta), dot(eyeDir, delta));

            // Only samples above the current horizon contribute
            if (sampleAngle < horizonAngle)
            {
                vec3 light = textureLod(uDiffuseTex, sampleUV, 0.0).rgb;
                float contrib = max(0.0, HorizonContribution(NdotE, NdotT, sampleAngle, horizonAngle));

                vec2 edge = min(sampleUV, 1.0 - sampleUV);
                float edgeFade = smoothstep(0.0, uSsgi.edgeFade, min(edge.x, edge.y));

                float dist2 = dot(delta, delta);
                float distFade = 1.0 / (1.0 + dist2 * uSsgi.distanceFalloff);

                // Reject light from back-facing emitters
                float facing = -dot(sampleNorm, delta * inversesqrt(max(dist2, 1e-8)));
                float normalFade = mix(1.0, smoothstep(0.0, 0.1, facing), uSsgi.normalRejection);

                giSlice += light * contrib * edgeFade * distFade * normalFade;
                horizonAngle = sampleAngle; // tighten horizon
            }
        }

        gi += giSlice * sliceWeight;
    }

    FragColor = vec4(gi, 1.0);
}
