/* light.glsl -- Light helpers wrapping UBO and LIB for direct use in shaders.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "../ubo/light.glsl"
#include "../lib/pbr.glsl"

/* === Lighting === */

vec3 L_Diffuse(float LdotH, float NdotV, float NdotL, float roughness)
{
    float FD90_minus_1 = 2.0 * LdotH * LdotH * roughness - 0.5;
    float FdV = 1.0 + FD90_minus_1 * PBR_SchlickFresnel(NdotV);
    float FdL = 1.0 + FD90_minus_1 * PBR_SchlickFresnel(NdotL);

    return vec3(M_INV_PI * (FdV * FdL * NdotL)); // Diffuse BRDF (Burley)
}

vec3 L_Specular(vec3 F0, float LdotH, float cNdotH, float NdotV, float NdotL, float roughness)
{
    roughness = max(roughness, 0.02); // >0.01 to avoid FP16 overflow after GGX distribution

    float alphaGGX = roughness * roughness;
    float D = PBR_DistributionGGX(cNdotH, alphaGGX);
    float G = PBR_GeometryGGX(NdotL, NdotV, alphaGGX);

    float cLdotH5 = PBR_SchlickFresnel(LdotH);
    float F90 = clamp(50.0 * F0.g, 0.0, 1.0);
    vec3 F = F0 + (F90 - F0) * cLdotH5;

    return NdotL * D * F * G; // Specular BRDF (Schlick GGX)
}

/* === Shadows === */

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

#if defined(MAX_LIGHTS_FORWARD)
#   define DECL_SHADOW_DIR(name)  float name(int lightIndex, vec4 Pls, float Zvs, float NdotL, mat2 diskRot)
#   define DECL_SHADOW_SPOT(name) float name(int lightIndex, vec4 Pls, float NdotL, mat2 diskRot)
#   define DECL_SHADOW_OMNI(name) float name(int lightIndex, vec3 Pws, float NdotL, mat2 diskRot)
#   define LIGHT uLights[lightIndex]
#else
#   define DECL_SHADOW_DIR(name)  float name(vec3 Pws, float Zvs, float NdotL, mat2 diskRot)
#   define DECL_SHADOW_SPOT(name) float name(vec3 Pws, float NdotL, mat2 diskRot)
#   define DECL_SHADOW_OMNI(name) float name(vec3 Pws, float NdotL, mat2 diskRot)
#   define LIGHT uLight
#endif

mat2 L_ShadowDebandingMatrix(vec2 fragCoord)
{
    float r = M_TAU * M_HashIGN(fragCoord);
    float sr = sin(r), cr = cos(r);
    return mat2(vec2(cr, -sr), vec2(sr, cr));
}

DECL_SHADOW_DIR(L_SampleShadowDir)
{
#if !defined(MAX_LIGHTS_FORWARD)
    vec4 Pls = LIGHT.viewProj * vec4(Pws, 1.0);
#endif

    vec3 projCoords = Pls.xyz / Pls.w * 0.5 + 0.5;
    float bias = LIGHT.shadowDepthBias + LIGHT.shadowSlopeBias * (1.0 - NdotL);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowDirTex, vec4(projCoords.xy + offset, LIGHT.shadowLayer, compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.05, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    float distFade = smoothstep(LIGHT.range, LIGHT.range * 0.75, Zvs);

    return mix(1.0, shadow, edgeFade * distFade * LIGHT.shadowOpacity);
}

DECL_SHADOW_SPOT(L_SampleShadowSpot)
{
#if !defined(MAX_LIGHTS_FORWARD)
    vec4 Pls = LIGHT.viewProj * vec4(Pws, 1.0);
#endif

    vec3 projCoords = Pls.xyz / Pls.w * 0.5 + 0.5;
    float bias = LIGHT.shadowDepthBias + LIGHT.shadowSlopeBias * (1.0 - NdotL);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowSpotTex, vec4(projCoords.xy + offset, LIGHT.shadowLayer, compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    return mix(1.0, shadow, LIGHT.shadowOpacity);
}

DECL_SHADOW_OMNI(L_SampleShadowOmni)
{
    vec3 lightToFrag = Pws - LIGHT.position;
    float currentDepth = length(lightToFrag);

    float bias = LIGHT.shadowDepthBias + LIGHT.shadowSlopeBias * (1.0 - NdotL);
    float compareDepth = (currentDepth - bias) / LIGHT.far;

    mat3 OBN = M_OrthonormalBasis(lightToFrag / currentDepth);

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 diskOffset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowOmniTex, vec4(OBN * vec3(diskOffset.xy, 1.0), LIGHT.shadowLayer), compareDepth);
    }
    shadow /= float(SHADOW_SAMPLES);

    return mix(1.0, shadow, LIGHT.shadowOpacity);
}

#undef SAMPLE_SHADOW_PROJ
#undef SAMPLE_SHADOW_OMNI
#undef LIGHT
