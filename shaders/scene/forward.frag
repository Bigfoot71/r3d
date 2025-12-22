/* forward.frag -- Fragment shader used for forward shading
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
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
    float shadowTexelSize;
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

in vec4 vPosLightSpace[LIGHT_FORWARD_COUNT];

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexEmission;
uniform sampler2D uTexNormal;
uniform sampler2D uTexORM;

uniform samplerCube uShadowMapCube[LIGHT_FORWARD_COUNT];
uniform sampler2D uShadowMap2D[LIGHT_FORWARD_COUNT];

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
uniform float uAmbientEnergy;
uniform float uReflectEnergy;

uniform Light uLights[LIGHT_FORWARD_COUNT];

uniform float uAlphaCutoff;
uniform vec3 uViewPosition;
uniform float uFar;

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

/* === Fragments === */

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 FragAlbedo;
layout(location = 2) out vec4 FragNormal;
layout(location = 3) out vec4 FragORM;

/* === Shadow functions === */

float ShadowDir(int i, float cNdotL, mat2 diskRot)
{
    Light light = uLights[i];

    /* --- Light Space Projection --- */

    vec4 p = vPosLightSpace[i];
    vec3 projCoords = p.xyz / p.w;
    projCoords = projCoords * 0.5 + 0.5;

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float currentDepth = projCoords.z - bias;

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 offset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        shadow += step(currentDepth, texture(uShadowMap2D[i], projCoords.xy + offset).r);
    }
    shadow /= float(SHADOW_SAMPLES);

    /* --- Apply a fade to the edges of the projection --- */

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    /* --- Final Shadow Value --- */

    return shadow;
}

float ShadowSpot(int i, float cNdotL, mat2 diskRot)
{
    Light light = uLights[i];

    /* --- Light Space Projection --- */

    vec4 p = vPosLightSpace[i];
    vec3 projCoords = p.xyz / p.w;
    projCoords = projCoords * 0.5 + 0.5;

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float currentDepth = projCoords.z - bias;

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 offset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        shadow += step(currentDepth, texture(uShadowMap2D[i], projCoords.xy + offset).r);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

float ShadowOmni(int i, float cNdotL, mat2 diskRot)
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

    mat3 OBN = M_OrthonormalBasis(direction);

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 diskOffset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        vec3 sampleDir = normalize(OBN * vec3(diskOffset.xy, 1.0));
        float sampleDepth = texture(uShadowMapCube[i], sampleDir).r * light.far;
        shadow += step(currentDepth, sampleDepth);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

/* === Main === */

void main()
{
    /* Sample material maps */

    vec4 albedo = vColor * texture(uTexAlbedo, vTexCoord);
    vec3 emission = uEmissionEnergy * (uEmissionColor * texture(uTexEmission, vTexCoord).rgb);
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    float occlusion = uOcclusion * orm.x;
    float roughness = uRoughness * orm.y;
    float metalness = uMetalness * orm.z;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo.rgb);

    /* Sample normal and compute view direction vector */

    vec3 N = normalize(vTBN * M_NormalScale(texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0, uNormalScale));
    if (!gl_FrontFacing) N = -N; // Flip for back facing triangles with double sided meshes

    vec3 V = normalize(uViewPosition - vPosition);

    /* Compute the dot product of the normal and view direction */

    float NdotV = dot(N, V);
    float cNdotV = max(NdotV, 1e-4);  // Clamped to avoid division by zero

    /* Loop through all light sources accumulating diffuse and specular light */

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < LIGHT_FORWARD_COUNT; i++)
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

        float NdotL = dot(N, L);
        if (NdotL <= 0.0) continue;
        float cNdotL = min(NdotL, 1.0); // clamped NdotL

        /* Compute the halfway vector between the view and light directions */

        vec3 H = normalize(V + L);

        float LdotH = max(dot(L, H), 0.0);
        float cLdotH = min(LdotH, 1.0);

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

        /* --- Calculating a random rotation matrix for shadow debanding --- */

        float r = M_TAU * M_HashIGN(gl_FragCoord.xy);
        float sr = sin(r), cr = cos(r);

        mat2 diskRot = mat2(vec2(cr, -sr), vec2(sr, cr));

        /* Apply shadow factor if the light casts shadows */

        float shadow = 1.0;

        if (light.shadow) {
            switch (light.type) {
            case LIGHT_DIR: shadow = ShadowDir(i, cNdotL, diskRot); break;
            case LIGHT_SPOT: shadow = ShadowSpot(i, cNdotL, diskRot); break;
            case LIGHT_OMNI: shadow = ShadowOmni(i, cNdotL, diskRot); break;
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
        ambient *= uAmbientEnergy;
    }
    else
    {
        // Simplified calculation of diffuse as if NdotV is equal to 1.0 (view facing normal)
        // NOTE: Small tweak here, we also add F0. It's not physically correct, 
        //       but it's to at least simulate some specularity, otherwise the 
        //       result would look poor for metals...
        ambient = (1.0 - F0) * (1.0 - metalness) * ambient;
        ambient += F0 * uAmbientColor * uAmbientEnergy;
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

        specular += spec * uReflectEnergy;
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
