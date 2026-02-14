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
float S_SampleShadowDir(Light light, sampler2DArrayShadow shadowTex, vec4 projPos, float cNdotL)
{
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    mat2 diskRot = S_DebandingRotationMatrix();

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(shadowTex, vec4(projCoords.xy + offset, light.shadowLayer, compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    return shadow;
}

float S_SampleShadowSpot(Light light, sampler2DArrayShadow shadowTex, vec4 projPos, float cNdotL)
{
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    mat2 diskRot = S_DebandingRotationMatrix();

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(shadowTex, vec4(projCoords.xy + offset, light.shadowLayer, compareDepth));
    }

   return shadow / float(SHADOW_SAMPLES);
}

float S_SampleShadowOmni(Light light, samplerCubeArrayShadow shadowTex, vec3 fragPos, float cNdotL)
{
    vec3 lightToFrag = fragPos - light.position;
    float currentDepth = length(lightToFrag);

    float bias = light.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, light.shadowDepthBias * currentDepth);
    float compareDepth = (currentDepth - bias) / light.far;

    mat3 OBN = M_OrthonormalBasis(lightToFrag / currentDepth);

    mat2 diskRot = S_DebandingRotationMatrix();

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 diskOffset = diskRot * VOGEL_DISK[i] * light.shadowSoftness;
        shadow += texture(shadowTex, vec4(OBN * vec3(diskOffset.xy, 1.0), light.shadowLayer), compareDepth);
    }

    return shadow / float(SHADOW_SAMPLES);
}

