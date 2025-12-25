/* ssr.frag -- Fragment shader for applying SSR to the scene
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"
#include "../include/pbr.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexORM;
uniform sampler2D uTexDepth;

uniform int uMaxRaySteps;
uniform int uBinarySearchSteps;
uniform float uRayMarchLength;
uniform float uDepthThickness;
uniform float uDepthTolerance;
uniform float uEdgeFadeStart;
uniform float uEdgeFadeEnd;

uniform vec3 uAmbientColor;
uniform float uAmbientEnergy;

uniform mat4 uMatView;
uniform mat4 uMatInvProj;
uniform mat4 uMatInvView;
uniform mat4 uMatViewProj;
uniform vec3 uViewPosition;

/* === Output === */

out vec4 FragColor;

/* === Helper Functions === */

float ScreenEdgeFade(vec2 uv)
{
    vec2 fade = max(vec2(0.0), abs(uv - 0.5) * 2.0 - vec2(uEdgeFadeStart));
    fade = fade / (uEdgeFadeEnd - uEdgeFadeStart);

    return 1.0 - clamp(max(fade.x, fade.y), 0.0, 1.0);
}

vec4 SampleScene(vec2 texCoord)
{
    vec3 albedo = texture(uTexAlbedo, texCoord).rgb;
    vec3 light = texture(uTexColor, texCoord).rgb;
    vec3 orm = texture(uTexORM, texCoord).rgb;

    vec3 ambient = uAmbientColor * uAmbientEnergy;
    ambient *= albedo * (1.0 - orm.b);

    float fade = ScreenEdgeFade(texCoord);
    return vec4(light + ambient, fade);
}

vec3 GetViewPosition(vec2 texCoord, float depth)
{
    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

vec3 GetWorldPosition(vec2 texCoord, float depth)
{
    vec3 viewPos = GetViewPosition(texCoord, depth);
    return (uMatInvView * vec4(viewPos, 1.0)).xyz;
}

vec2 WorldToScreenSpace(vec3 worldPos)
{
    vec4 projPos = uMatViewProj * vec4(worldPos, 1.0);
    projPos /= projPos.w;
    return projPos.xy * 0.5 + 0.5;
}

bool OffScreen(vec2 uv)
{
    return any(greaterThan(uv, vec2(1.0))) || any(lessThan(uv, vec2(0.0)));
}

/* === Raymarching === */

vec3 BinarySearch(vec3 startPos, vec3 endPos)
{
    for (int i = 0; i < uBinarySearchSteps; i++)
    {
        vec3 midPos = (startPos + endPos) * 0.5;

        vec2 uv = WorldToScreenSpace(midPos);
        float sampledDepth = texture(uTexDepth, uv).r;

        vec3 sampledViewPos = GetViewPosition(uv, sampledDepth);
        vec3 midViewPos = (uMatView * vec4(midPos, 1.0)).xyz;

        float depthDiff = sampledViewPos.z - midViewPos.z;

        if (depthDiff > -uDepthTolerance) {
            endPos = midPos;    // surface in front of us
        }
        else {
            startPos = midPos;  // surface behind
        }
    }

    return endPos; // refined final position
}

vec4 TraceReflectionRay(vec3 startPos, vec3 reflectionDir)
{
    float minStep = uRayMarchLength / float(uMaxRaySteps);
    float stepSize = minStep;

    vec3 currentPos = startPos;

    for (int i = 0; i < uMaxRaySteps; i++)
    {
        currentPos += reflectionDir * stepSize;

        vec2 uv = WorldToScreenSpace(currentPos);
        if (OffScreen(uv)) break;

        float sampledDepth = texture(uTexDepth, uv).r;

        vec3 sampledViewPos  = GetViewPosition(uv, sampledDepth);
        vec3 currentViewPos = (uMatView * vec4(currentPos, 1.0)).xyz;

        float depthDiff = sampledViewPos.z - currentViewPos.z;

        if (depthDiff > -uDepthTolerance && depthDiff < uDepthThickness) {
            currentPos = BinarySearch(startPos, currentPos);
            uv = WorldToScreenSpace(currentPos);
            return SampleScene(uv);
        }

        stepSize = max(depthDiff * 0.9, minStep);
    }

    return vec4(0.0);
}

/* === Main Program === */

void main()
{
    FragColor = vec4(0.0);

    float depth = texture(uTexDepth, vTexCoord).r;
    if (depth > 1.0 - 1e-5) return;

    vec2 encodedNormal = texture(uTexNormal, vTexCoord).rg;
    vec3 worldNormal = M_DecodeOctahedral(encodedNormal);
    vec3 worldPos = GetWorldPosition(vTexCoord, depth);

    vec3 viewDir = normalize(worldPos - uViewPosition);
    vec3 reflectionDir = reflect(viewDir, worldNormal);
    if (dot(reflectionDir, worldNormal) < 0.0) return;

    FragColor = TraceReflectionRay(worldPos, reflectionDir);
}
