/* ssr.frag -- Fragment shader for applying SSR to the scene
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"
#include "../include/pbr.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uLightingTex;
uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uOrmTex;
uniform sampler2D uDepthTex;

uniform int uMaxRaySteps;
uniform int uBinarySearchSteps;
uniform float uRayMarchLength;
uniform float uDepthThickness;
uniform float uDepthTolerance;
uniform float uEdgeFadeStart;
uniform float uEdgeFadeEnd;

uniform vec3 uAmbientColor;
uniform float uAmbientEnergy;

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
    vec3 albedo = texture(uAlbedoTex, texCoord).rgb;
    vec3 light = texture(uLightingTex, texCoord).rgb;
    vec3 orm = texture(uOrmTex, texCoord).rgb;

    vec3 ambient = uAmbientColor * uAmbientEnergy;
    ambient *= albedo * (1.0 - orm.b);

    float fade = ScreenEdgeFade(texCoord);
    return vec4(light + ambient, fade);
}

/* === Raymarching === */

vec3 BinarySearch(vec3 startPos, vec3 endPos)
{
    for (int i = 0; i < uBinarySearchSteps; i++)
    {
        vec3 midPos = (startPos + endPos) * 0.5;
        vec2 uv = V_WorldToScreen(midPos);

        vec3 sampledViewPos = V_GetViewPosition(uDepthTex, uv);
        vec3 midViewPos = (uView.view * vec4(midPos, 1.0)).xyz;

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

        vec2 uv = V_WorldToScreen(currentPos);
        if (V_OffScreen(uv)) break;

        vec3 sampledViewPos = V_GetViewPosition(uDepthTex, uv);
        vec3 currentViewPos = (uView.view * vec4(currentPos, 1.0)).xyz;

        float depthDiff = sampledViewPos.z - currentViewPos.z;

        if (depthDiff > -uDepthTolerance && depthDiff < uDepthThickness) {
            currentPos = BinarySearch(startPos, currentPos);
            uv = V_WorldToScreen(currentPos);
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

    float linearDepth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    if (linearDepth >= uView.far) return;

    vec3 worldNormal = V_GetWorldNormal(uNormalTex, ivec2(gl_FragCoord.xy));
    vec3 worldPos = V_GetWorldPosition(vTexCoord, linearDepth);

    vec3 viewDir = normalize(worldPos - uView.position);
    vec3 reflectionDir = reflect(viewDir, worldNormal);
    if (dot(reflectionDir, worldNormal) < 0.0) return;

    FragColor = TraceReflectionRay(worldPos, reflectionDir);
}
