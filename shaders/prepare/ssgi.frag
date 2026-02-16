/* ssgi.frag -- Screen Space Global Illumination fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uHistoryTex;
uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform int uSampleCount;
uniform int uMaxRaySteps;
uniform float uStepSize;
uniform float uThickness;
uniform float uMaxDistance;
uniform float uFadeStart;
uniform float uFadeEnd;

/* === Output === */

out vec4 FragColor;

/* === Raymarching === */

vec3 GetCosHemisphereSample(vec3 hitNorm, float u1, float u2)
{
    mat3 TBN = M_OrthonormalBasis(hitNorm);

    float r = sqrt(u1);
    float phi = M_TAU * u2;

    vec3 sample = vec3(
        r * cos(phi),
        r * sin(phi),
        sqrt(max(0.0, 1.0 - u1))
    );

    return TBN * sample;
}

vec3 TraceRay(vec3 startViewPos, vec3 reflectionDir)
{
    vec3 dirStep = reflectionDir * uStepSize;
    float stepDistanceSq = dot(dirStep, dirStep);
    float maxDistanceSq = uMaxDistance * uMaxDistance;

    vec3 currentPos = startViewPos + dirStep;
    vec3 prevPos = startViewPos;
    float rayDistanceSq = stepDistanceSq;

    vec2 hitUV = vec2(0.0);
    bool hit = false;

    for (int i = 1; i < uMaxRaySteps; i++)
    {
        if (rayDistanceSq > maxDistanceSq) break;
        vec2 uv = V_ViewToScreen(currentPos);
        if (V_OffScreen(uv)) break;

        float sampleZ = -textureLod(uDepthTex, uv, 0).r;
        float depthDiff = sampleZ - currentPos.z;

        if (depthDiff > 0.0 && depthDiff < uThickness) {
            hitUV = uv;
            hit = true;
            break;
        }

        prevPos = currentPos;
        currentPos += dirStep;
        rayDistanceSq += stepDistanceSq;
    }

    if (!hit) return vec3(0.0);

    vec3 hitHist = textureLod(uHistoryTex, hitUV, 0).rgb;
    vec3 hitDiff = textureLod(uDiffuseTex, hitUV, 0).rgb;

    float distFade = 1.0 - smoothstep(0.0, uMaxDistance, sqrt(rayDistanceSq));

    return (hitDiff + hitHist) * distFade;
}

/* === Main Program === */

void main()
{
    float linearDepth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    if (linearDepth >= uFadeEnd) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 giColor = vec3(0.0);
    float totalWeight = 0.0;

    vec3 viewNormal = V_GetViewNormal(uNormalTex, ivec2(gl_FragCoord.xy));
    vec3 viewPos = V_GetViewPosition(vTexCoord, linearDepth);
    vec3 viewDir = normalize(viewPos);

    for (int i = 0; i < uSampleCount; i++)
    {
        float u1 = fract(127.0 * M_HashIGN(gl_FragCoord.xy, float(i)));                 // moiré
        float u2 = fract(227.0 * M_HashIGN(gl_FragCoord.xy, float(uSampleCount - i)));  // bands

        vec3 rayDir = GetCosHemisphereSample(viewNormal, u1, u2);
        vec3 result = TraceRay(viewPos, rayDir);

        float weight = max(0.0, dot(rayDir, viewNormal));
        giColor += result * weight;
        totalWeight += weight;
    }

    float fade = smoothstep(uFadeEnd, uFadeStart, linearDepth);
    FragColor = vec4(giColor / max(totalWeight, 1e-4) * fade, 1.0);
}
