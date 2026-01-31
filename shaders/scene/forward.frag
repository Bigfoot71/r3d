/* forward.frag -- Fragment shader used for forward shading
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Extensions === */

#extension GL_ARB_texture_cube_map_array : enable

/* === Includes === */

#include "../include/math.glsl"
#include "../include/pbr.glsl"

/* === Varyings === */

smooth in vec3 vPosition;
smooth in vec2 vTexCoord;
flat   in vec3 vEmission;
smooth in vec4 vColor;
smooth in mat3 vTBN;

in vec4 vPosLightSpace[NUM_FORWARD_LIGHTS];

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform sampler2D uEmissionMap;
uniform sampler2D uNormalMap;
uniform sampler2D uOrmMap;

uniform sampler2DArrayShadow uShadowDirTex;
uniform sampler2DArrayShadow uShadowSpotTex;
uniform samplerCubeArrayShadow uShadowOmniTex;

uniform samplerCubeArray uIrradianceTex;
uniform samplerCubeArray uPrefilterTex;
uniform sampler2D uBrdfLutTex;

uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

uniform vec3 uViewPosition;

#if defined(PROBE)
uniform bool uProbeInterior;
#endif // PROBE

/* === Blocks === */

#include "../include/blocks/light.glsl"
#include "../include/blocks/env.glsl"

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

/* === Shadow functions === */

float ShadowDir(int i, float cNdotL, mat2 diskRot)
{
    Light light = uLights[i];

    vec4 p = vPosLightSpace[i];
    vec3 projCoords = p.xyz / p.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 offset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        shadow += texture(uShadowDirTex, vec4(projCoords.xy + offset, float(light.shadowLayer), compareDepth));
    }
    shadow /= float(SHADOW_SAMPLES);

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    return shadow;
}

float ShadowSpot(int i, float cNdotL, mat2 diskRot)
{
    Light light = uLights[i];

    vec4 p = vPosLightSpace[i];
    vec3 projCoords = p.xyz / p.w * 0.5 + 0.5;

    float bias = light.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, light.shadowDepthBias * projCoords.z);
    float compareDepth = projCoords.z - bias;

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 offset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        shadow += texture(uShadowSpotTex, vec4(projCoords.xy + offset, float(light.shadowLayer), compareDepth));
    }

    return shadow / float(SHADOW_SAMPLES);
}

float ShadowOmni(int i, float cNdotL, mat2 diskRot)
{
    Light light = uLights[i];

    vec3 lightToFrag = vPosition - light.position;
    float currentDepth = length(lightToFrag);

    float bias = light.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, light.shadowDepthBias * currentDepth);
    float compareDepth = (currentDepth - bias) / light.far;

    mat3 OBN = M_OrthonormalBasis(lightToFrag / currentDepth);

    float shadow = 0.0;
    for (int j = 0; j < SHADOW_SAMPLES; ++j) {
        vec2 diskOffset = diskRot * VOGEL_DISK[j] * light.shadowSoftness;
        shadow += texture(uShadowOmniTex, vec4(OBN * vec3(diskOffset.xy, 1.0), float(light.shadowLayer)), compareDepth);
    }

    return shadow / float(SHADOW_SAMPLES);
}

/* === User override === */

#include "../include/user/scene.frag"

/* === Main function === */

void main()
{
    /* Sample material maps */

    SceneFragment(vTexCoord, vTBN, 0.0);

    vec3 ORM = vec3(OCCLUSION, ROUGHNESS, METALNESS);
    mat3 TBN = mat3(TANGENT, BITANGENT, NORMAL);
    float dielectric = (1.0 - METALNESS);

    /* Compute F0 (reflectance at normal incidence) and diffuse coefficient */

    vec3 F0 = PBR_ComputeF0(METALNESS, 0.5, ALBEDO);
    vec3 kD = dielectric * ALBEDO;

    /* Sample normal and compute view direction vector */

    vec3 N = normalize(TBN * M_NormalScale(texture(uNormalMap, vTexCoord).rgb * 2.0 - 1.0, uNormalScale));
    if (!gl_FrontFacing) N = -N; // Flip for back facing triangles with double sided meshes

    vec3 V = normalize(uViewPosition - vPosition);

    /* Compute the dot product of the normal and view direction */

    float NdotV = dot(N, V);
    float cNdotV = max(NdotV, 1e-4);  // Clamped to avoid division by zero

    /* Loop through all light sources accumulating diffuse and specular light */

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < uNumLights; i++)
    {
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

        vec3 diffLight = L_Diffuse(cLdotH, cNdotV, cNdotL, ROUGHNESS);
        diffLight *= lightColE * dielectric;

        /* Compute specular lighting */

        vec3 specLight =  L_Specular(F0, cLdotH, cNdotH, cNdotV, cNdotL, ROUGHNESS);
        specLight *= lightColE * light.specular;

        /*  Calculating a random rotation matrix for shadow debanding */

        float r = M_TAU * M_HashIGN(gl_FragCoord.xy);
        float sr = sin(r), cr = cos(r);

        mat2 diskRot = mat2(vec2(cr, -sr), vec2(sr, cr));

        /* Apply shadow factor if the light casts shadows */

        float shadow = 1.0;

        if (light.shadowLayer >= 0) {
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

    /* Compute ambient */

#if defined(PROBE)
    if (uProbeInterior) E_ComputeAmbientColor(diffuse, kD, OCCLUSION);
    else E_ComputeAmbientOnly(diffuse, specular, kD, ORM, F0, vPosition, N, V, cNdotV);
#else
    E_ComputeAmbientAndProbes(diffuse, specular, kD, ORM, F0, vPosition, N, V, cNdotV);
#endif

    /* Compute the final fragment color */

    FragColor = vec4(ALBEDO * diffuse + specular + EMISSION, ALPHA);
}
