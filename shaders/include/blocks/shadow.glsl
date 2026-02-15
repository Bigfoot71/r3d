/* light.glsl -- Contains shadow map sampling functions
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Includes === */

#include "light.glsl"
#include "../math.glsl"

/* === Constants === */

#define SHADOW_SAMPLES 8

const vec2 VOGEL_DISK[8] = vec2[8](
    vec2(0.250000, 0.000000),
    vec2(-0.319290, 0.292496),
    vec2(0.048872, -0.556877),
    vec2(0.402444, 0.524918),
    vec2(-0.738535, -0.130636),
    vec2(0.699605, -0.445031),
    vec2(-0.234004, 0.870484),
    vec2(-0.446271, -0.859268)
);

/* === Functions === */

mat2 S_DebandingRotationMatrix()
{
    float r = M_TAU * M_HashIGN(gl_FragCoord.xy);
    float sr = sin(r), cr = cos(r);
    return mat2(vec2(cr, -sr), vec2(sr, cr));
}

#ifdef NUM_FORWARD_LIGHTS
float S_SampleShadowDir(int lightIndex, vec4 projPos, float cNdotL, mat2 diskRot)
{
    #define light uLights[lightIndex]
#else
float S_SampleShadowDir(vec4 projPos, float cNdotL, mat2 diskRot)
{
    #define light uLight
#endif 
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(uShadowDirTex, vec4(projCoords.xy + offset, light.shadowLayer, compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    #undef light

    return shadow;
}

#ifdef NUM_FORWARD_LIGHTS
float S_SampleShadowSpot(int lightIndex, vec4 projPos, float cNdotL, mat2 diskRot)
{
    #define light uLights[lightIndex]
#else
float S_SampleShadowSpot(vec4 projPos, float cNdotL, mat2 diskRot)
{
    #define light uLight
#endif 
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(uShadowSpotTex, vec4(projCoords.xy + offset, light.shadowLayer, compareDepth));
    }

    #undef light

   return shadow / float(SHADOW_SAMPLES);
}

#ifdef NUM_FORWARD_LIGHTS
float S_SampleShadowOmni(int lightIndex, vec3 fragPos, float cNdotL, mat2 diskRot)
{
    #define light uLights[lightIndex]
#else
float S_SampleShadowOmni(vec3 fragPos, float cNdotL, mat2 diskRot)
{
    #define light uLight
#endif 
    vec3 lightToFrag = fragPos - light.position;
    float currentDepth = length(lightToFrag);

    float bias = light.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, light.shadowDepthBias * currentDepth);
    float compareDepth = (currentDepth - bias) / light.far;

    mat3 OBN = M_OrthonormalBasis(lightToFrag / currentDepth);

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 diskOffset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(uShadowOmniTex, vec4(OBN * vec3(diskOffset.xy, 1.0), light.shadowLayer), compareDepth);
    }

    #undef light

    return shadow / float(SHADOW_SAMPLES);
}

