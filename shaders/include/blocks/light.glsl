/* light.glsl -- Contains everything you need to manage lights
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Includes === */

#include "../math.glsl"
#include "../pbr.glsl"

/* === Macros === */

#ifdef NUM_FORWARD_LIGHTS
#   define SAMPLE_SHADOW_PROJ(name) float name(int lightIndex, vec4 projPos, float cNdotL)
#   define SAMPLE_SHADOW_OMNI(name) float name(int lightIndex, vec3 fragPos, float cNdotL)
#   define LIGHT uLights[lightIndex]
#else
#   define SAMPLE_SHADOW_PROJ(name) float name(vec4 projPos, float cNdotL)
#   define SAMPLE_SHADOW_OMNI(name) float name(vec3 fragPos, float cNdotL)
#   define LIGHT uLight
#endif

/* === Constants === */

// Should be defined in client side
// #define NUM_FORWARD_LIGHTS 8

#define LIGHT_DIR   0
#define LIGHT_SPOT  1
#define LIGHT_OMNI  2

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

/* === Structures === */

struct Light {
    mat4 viewProj;
    vec3 color;
    vec3 position;
    vec3 direction;
    float specular;
    float energy;
    float range;
    float near;
    float far;
    float attenuation;
    float innerCutOff;
    float outerCutOff;
    float shadowSoftness;
    float shadowDepthBias;
    float shadowSlopeBias;
    int shadowLayer;        //< less than zero if no shadows
    int type;
};

/* === Uniform Block === */

#ifdef NUM_FORWARD_LIGHTS
layout(std140) uniform LightArrayBlock {
    Light uLights[NUM_FORWARD_LIGHTS];
    int uNumLights;
};
#else
layout(std140) uniform LightBlock {
    Light uLight;
};
#endif

/* === Functions === */

vec3 L_Diffuse(float cLdotH, float cNdotV, float cNdotL, float roughness)
{
    float FD90_minus_1 = 2.0 * cLdotH * cLdotH * roughness - 0.5;
    float FdV = 1.0 + FD90_minus_1 * PBR_SchlickFresnel(cNdotV);
    float FdL = 1.0 + FD90_minus_1 * PBR_SchlickFresnel(cNdotL);

    return vec3(M_INV_PI * (FdV * FdL * cNdotL)); // Diffuse BRDF (Burley)
}

vec3 L_Specular(vec3 F0, float cLdotH, float cNdotH, float cNdotV, float cNdotL, float roughness)
{
    roughness = max(roughness, 0.02); // >0.01 to avoid FP16 overflow after GGX distribution

    float alphaGGX = roughness * roughness;
    float D = PBR_DistributionGGX(cNdotH, alphaGGX);
    float G = PBR_GeometryGGX(cNdotL, cNdotV, alphaGGX);

    float cLdotH5 = PBR_SchlickFresnel(cLdotH);
    float F90 = clamp(50.0 * F0.g, 0.0, 1.0);
    vec3 F = F0 + (F90 - F0) * cLdotH5;

    return cNdotL * D * F * G; // Specular BRDF (Schlick GGX)
}

#ifdef L_SHADOW_IMPL

mat2 L_DebandingRotationMatrix()
{
    float r = M_TAU * M_HashIGN(gl_FragCoord.xy);
    float sr = sin(r), cr = cos(r);
    return mat2(vec2(cr, -sr), vec2(sr, cr));
}

SAMPLE_SHADOW_PROJ(L_SampleShadowDir)
{
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = LIGHT.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, LIGHT.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    mat2 diskRot = L_DebandingRotationMatrix();

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowDirTex, vec4(projCoords.xy + offset, LIGHT.shadowLayer, compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    return shadow;
}

SAMPLE_SHADOW_PROJ(L_SampleShadowSpot)
{
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    float bias = LIGHT.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, LIGHT.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    mat2 diskRot = L_DebandingRotationMatrix();

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowSpotTex, vec4(projCoords.xy + offset, LIGHT.shadowLayer, compareDepth));
    }

   return shadow / float(SHADOW_SAMPLES);
}

SAMPLE_SHADOW_OMNI(L_SampleShadowOmni)
{
    vec3 lightToFrag = fragPos - LIGHT.position;
    float currentDepth = length(lightToFrag);

    float bias = LIGHT.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, LIGHT.shadowDepthBias * currentDepth);
    float compareDepth = (currentDepth - bias) / LIGHT.far;

    mat2 diskRot = L_DebandingRotationMatrix();

    mat3 OBN = M_OrthonormalBasis(lightToFrag / currentDepth);

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 diskOffset = diskRot * VOGEL_DISK[i] * LIGHT.shadowSoftness;
        shadow += texture(uShadowOmniTex, vec4(OBN * vec3(diskOffset.xy, 1.0), LIGHT.shadowLayer), compareDepth);
    }

    return shadow / float(SHADOW_SAMPLES);
}

#endif // L_SHADOW_IMPL

/* === Undefs === */

#undef SAMPLE_SHADOW_PROJ
#undef SAMPLE_SHADOW_OMNI
#undef LIGHT
