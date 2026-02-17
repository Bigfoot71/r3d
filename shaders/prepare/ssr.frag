/* ssr.frag -- Screen Space Reflections fragment shader
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

uniform sampler2D uDiffuseTex;
uniform sampler2D uSpecularTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform int uMaxRaySteps;
uniform int uBinarySteps;
uniform float uStepSize;
uniform float uThickness;
uniform float uMaxDistance;
uniform float uEdgeFade;

/* === Output === */

out vec4 FragColor;

/* === Raymarching === */

vec4 TraceRay(vec3 startViewPos, vec3 reflectionDir)
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

    if (!hit) return vec4(0.0);

    vec3 start = prevPos;
    vec3 end = currentPos;

    for (int i = 0; i < uBinarySteps; i++)
    {
        vec3 mid = mix(start, end, 0.5);
        vec2 uv = V_ViewToScreen(mid);
        if (V_OffScreen(uv)) break;

        float sampleZ = -textureLod(uDepthTex, uv, 0).r;
        float depthDiff = sampleZ - mid.z;

        if (depthDiff > 0.0 && depthDiff < uThickness) {
            hitUV = uv;
            end = mid;
        }
        else {
            start = mid;
        }
    }

    vec3 hitNormal = V_GetViewNormal(uNormalTex, hitUV);
    float d = dot(reflectionDir, hitNormal);
    if (d > 0.0) return vec4(0.0);

    vec3 hitDiff = textureLod(uDiffuseTex, hitUV, 0).rgb;
    vec3 hitSpec = textureLod(uSpecularTex, hitUV, 0).rgb;

    vec2 distToBorder = min(hitUV, 1.0 - hitUV);
    float edgeFade = smoothstep(0.0, uEdgeFade, min(distToBorder.x, distToBorder.y));
    float distFade = 1.0 - smoothstep(0.0, uMaxDistance, sqrt(rayDistanceSq));

    return vec4(hitDiff + hitSpec, edgeFade * distFade);
}

/* === Main Program === */

void main()
{
    // Early depth rejection
    float linearDepth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    if (linearDepth >= uView.far) {
        FragColor = vec4(0.0);
        return;
    }

    // Fetch view-space geometry
    vec3 viewNormal = V_GetViewNormal(uNormalTex, ivec2(gl_FragCoord.xy));
    vec3 viewPos = V_GetViewPosition(vTexCoord, linearDepth);
    vec3 viewDir = normalize(viewPos);
    vec3 reflectionDir = reflect(viewDir, viewNormal);

    // Trace reflection ray
    FragColor = TraceRay(viewPos, reflectionDir);
}
