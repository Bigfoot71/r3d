/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#version 330 core

/* === Includes === */

#include "../include/light.glsl"
#include "../include/math.glsl"
#include "../include/pbr.glsl"
#include "../include/ibl.glsl"

/* === Structs === */

struct Light
{
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
    float shadowMapTxlSz;
    float shadowDepthBias;
    float shadowSlopeBias;
    lowp int type;
    bool enabled;
    bool shadow;
};

/* === Varyings === */

in vec3 vPosition;
in vec2 vTexCoord;
in vec4 vColor;
in mat3 vTBN;

in vec4 vPosLightSpace[FORWARD_LIGHT_COUNT];

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexEmission;
uniform sampler2D uTexNormal;
uniform sampler2D uTexORM;

uniform samplerCube uShadowMapCube[FORWARD_LIGHT_COUNT];
uniform sampler2D uShadowMap2D[FORWARD_LIGHT_COUNT];

uniform float uEmissionEnergy;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

uniform vec3 uAmbientColor;
uniform vec3 uEmissionColor;

uniform samplerCube uCubeIrradiance;
uniform samplerCube uCubePrefilter;
uniform sampler2D uTexBrdfLut;
uniform vec4 uQuatSkybox;
uniform bool uHasSkybox;
uniform float uSkyboxAmbientIntensity;
uniform float uSkyboxReflectIntensity;

uniform Light uLights[FORWARD_LIGHT_COUNT];

uniform float uAlphaCutoff;
uniform vec3 uViewPosition;
uniform float uFar;

/* === Constants === */

#define SHADOW_SAMPLES 16

const vec2 POISSON_DISK[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790)
);

/* === Fragments === */

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 FragAlbedo;
layout(location = 2) out vec4 FragNormal;
layout(location = 3) out vec4 FragORM;

/* === Shadow functions === */

float InterleavedGradientNoise(vec2 pos)
{
    // http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
	const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * fract(dot(pos, magic.xy)));
}

float ShadowOmni(int i, float cNdotL)
{
    Light light = uLights[i];

    /* --- Light Vector and Distance Calculation --- */

    vec3 lightToFrag = vPosition - light.position;
    float currentDepth = length(lightToFrag);
    vec3 direction = normalize(lightToFrag);

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = light.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, light.shadowDepthBias * currentDepth);
    currentDepth -= bias;

    /* --- Build orthonormal basis for perturbation --- */

    mat3 TBN = M_OrthonormalBasis(direction);

    /* --- Generate an additional debanding rotation for the poisson disk --- */

    float r = M_TAU * InterleavedGradientNoise(gl_FragCoord.xy);
    float sr = sin(r);
    float cr = cos(r);
    mat2 diskRot = mat2(vec2(cr, -sr), vec2(sr, cr));

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 diskOffset = diskRot * POISSON_DISK[j] * light.shadowSoftness;
        vec3 sampleDir = normalize(TBN * vec3(diskOffset.xy, 1.0));
        float sampleDepth = texture(uShadowMapCube[i], sampleDir).r * light.far;
        shadow += step(currentDepth, sampleDepth);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

float Shadow(int i, float cNdotL)
{
    Light light = uLights[i];

    /* --- Light Space Projection --- */

    vec4 p = vPosLightSpace[i];
    vec3 projCoords = p.xyz / p.w;
    projCoords = projCoords * 0.5 + 0.5;

    /* --- Shadow Map Bounds Check --- */

    if (any(greaterThan(projCoords.xyz, vec3(1.0))) ||
        any(lessThan(projCoords.xyz, vec3(0.0)))) {
        return 1.0;
    }

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float currentDepth = projCoords.z - bias;

    /* --- Generate an additional debanding rotation for the poisson disk --- */

    float r = M_TAU * InterleavedGradientNoise(gl_FragCoord.xy);
    float sr = sin(r);
    float cr = cos(r);
    mat2 diskRot = mat2(vec2(cr, -sr), vec2(sr, cr));

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 offset = diskRot * POISSON_DISK[j] * light.shadowSoftness;
        shadow += step(currentDepth, texture(uShadowMap2D[i], projCoords.xy + offset).r);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

/* === Main === */

void main()
{
    /* Sample albedo texture */

    vec4 albedo = vColor * texture(uTexAlbedo, vTexCoord);

    // TODO: Alpha scissor is unnecessary after a depth pre-pass
    if (albedo.a < uAlphaCutoff) discard;

    /* Sample emission texture */

    vec3 emission = uEmissionEnergy * (uEmissionColor * texture(uTexEmission, vTexCoord).rgb);

    /* Sample ORM texture and extract values */

    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    float occlusion = uOcclusion * orm.x;
    float roughness = uRoughness * orm.y;
    float metalness = uMetalness * orm.z;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo.rgb);

    /* Sample normal and compute view direction vector */

    vec3 N = normalize(vTBN * M_NormalScale(texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0, uNormalScale));
    vec3 V = normalize(uViewPosition - vPosition);

    /* Compute the dot product of the normal and view direction */

    float NdotV = dot(N, V);
    float cNdotV = max(NdotV, 1e-4);  // Clamped to avoid division by zero

    /* Loop through all light sources accumulating diffuse and specular light */

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < FORWARD_LIGHT_COUNT; i++)
    {
        if (!uLights[i].enabled) {
            continue;
        }

        Light light = uLights[i];

        /* Compute light direction */

        vec3 L = vec3(0.0);
        if (light.type == LIGHT_DIR) L = -light.direction;
        else L = normalize(light.position - vPosition);

        /* Compute the dot product of the normal and light direction */

        float NdotL = max(dot(N, L), 0.0);
        float cNdotL = min(NdotL, 1.0); // clamped NdotL

        /* Compute the halfway vector between the view and light directions */

        vec3 H = normalize(V + L);

        float LdotH = max(dot(L, H), 0.0);
        float cLdotH = min(dot(L, H), 1.0);

        float NdotH = max(dot(N, H), 0.0);
        float cNdotH = min(NdotH, 1.0);

        /* Compute light color energy */

        vec3 lightColE = light.color * light.energy;

        /* Compute diffuse lighting */

        vec3 diffLight = L_Diffuse(cLdotH, cNdotV, cNdotL, roughness);
        diffLight *= lightColE * (1.0 - metalness); // 0.0 for pure metal, 1.0 for dielectric

        /* Compute specular lighting */

        vec3 specLight =  L_Specular(F0, cLdotH, cNdotH, cNdotV, cNdotL, roughness);
        specLight *= lightColE * light.specular;

        /* Apply shadow factor if the light casts shadows */

        float shadow = 1.0;
        if (light.shadow) {
            if (light.type == LIGHT_OMNI) {
                shadow = ShadowOmni(i, cNdotL);
            }
            else {
                shadow = Shadow(i, cNdotL);
            }
        }

        /* Apply attenuation based on the distance from the light */

        if (light.type != LIGHT_DIR)
        {
            float dist = length(light.position - vPosition);
            float atten = 1.0 - clamp(dist / light.range, 0.0, 1.0);
            shadow *= atten * light.attenuation;
        }

        /* Apply spotlight effect if the light is a spotlight */

        if (light.type == LIGHT_SPOT)
        {
            float theta = dot(L, -light.direction);
            float epsilon = (light.innerCutOff - light.outerCutOff);
            shadow *= smoothstep(0.0, 1.0, (theta - light.outerCutOff) / epsilon);
        }

        /* Accumulate the diffuse and specular lighting contributions */

        diffuse += diffLight * shadow;
        specular += specLight * shadow;
    }

    /* Compute ambient - (IBL diffuse) */

    vec3 ambient = uAmbientColor;

    if (uHasSkybox)
    {
        vec3 kS = IBL_FresnelSchlickRoughness(cNdotV, F0, roughness);
        vec3 kD = (1.0 - kS) * (1.0 - metalness);

        vec3 Nr = M_Rotate3D(N, uQuatSkybox);
        ambient = kD * texture(uCubeIrradiance, Nr).rgb;
        ambient *= uSkyboxAmbientIntensity;
    }
    else
    {
        // Simplified calculation of diffuse as if NdotV is equal to 1.0 (view facing normal)
        // NOTE: Small tweak here, we also add F0. It's not physically correct, 
        //       but it's to at least simulate some specularity, otherwise the 
        //       result would look poor for metals...
        ambient = (1.0 - F0) * (1.0 - metalness) * ambient;
        ambient += F0 * uAmbientColor;
    }

    /* Compute ambient occlusion map */

    ambient *= occlusion;

    // Light affect should be material-specific
    //float lightAffect = mix(1.0, ao, uValAOLightAffect);
    //specular *= lightAffect;
    //diffuse *= lightAffect;

    /* Skybox reflection - (IBL specular) */

    if (uHasSkybox)
    {
        vec3 R = M_Rotate3D(reflect(-V, N), uQuatSkybox);

        const float MAX_REFLECTION_LOD = 7.0;
        float mipLevel = IBL_GetSpecularMipLevel(roughness, MAX_REFLECTION_LOD + 1.0);
        vec3 prefilteredColor = textureLod(uCubePrefilter, R, mipLevel).rgb;

        float specularOcclusion = IBL_GetSpecularOcclusion(cNdotV, occlusion, roughness);
        vec3 specBRDF = IBL_GetMultiScatterBRDF(uTexBrdfLut, cNdotV, roughness, F0, metalness);
        vec3 spec = prefilteredColor * specBRDF * specularOcclusion;

        // Soft falloff hack at low angles to avoid overly bright effect
        float edgeFade = mix(1.0, pow(cNdotV, 0.5), roughness);
        spec *= edgeFade;

        specular += spec * uSkyboxReflectIntensity;
    }

    /* Compute the final diffuse color, including ambient and diffuse lighting contributions */

    diffuse = albedo.rgb * (ambient + diffuse);

    /* Compute the final fragment color by combining diffuse, specular, and emission contributions */

    FragColor = vec4(diffuse + specular + emission, albedo.a);

    /* Output material data */

    FragAlbedo = vec4(albedo.rgb, 1.0);
    FragNormal = vec4(M_EncodeOctahedral(N), vec2(1.0));
    FragORM = vec4(occlusion, roughness, metalness, 1.0);
}
