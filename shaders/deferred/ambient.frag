/* ambient.frag -- Fragment shader for applying ambient lighting for deferred shading
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Common Includes === */

#include "../include/pbr.glsl"

#ifdef IBL

/* === Includes === */

#include "../include/math.glsl"
#include "../include/ibl.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexDepth;
uniform sampler2D uTexSSAO;
uniform sampler2D uTexORM;

uniform samplerCube uCubeIrradiance;
uniform samplerCube uCubePrefilter;
uniform sampler2D uTexBrdfLut;
uniform vec4 uQuatSkybox;
uniform float uAmbientEnergy;
uniform float uReflectEnergy;

uniform vec3 uViewPosition;
uniform mat4 uMatInvProj;
uniform mat4 uMatInvView;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Misc functions === */

vec3 GetPositionFromDepth(float depth)
{
    vec4 ndcPos = vec4(vTexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    viewPos /= viewPos.w;

    return (uMatInvView * viewPos).xyz;
}

/* === Main === */

void main()
{
    /* Sample albedo and ORM texture and extract values */
    
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    float occlusion = orm.r;
    float roughness = orm.g;
    float metalness = orm.b;

    /* Sample SSAO buffer and modulate occlusion value */

    occlusion *= texture(uTexSSAO, vTexCoord).r;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);

    /* Sample world depth and reconstruct world position */

    float depth = texture(uTexDepth, vTexCoord).r;
    vec3 position = GetPositionFromDepth(depth);

    /* Sample and decode normal in world space */

    vec3 N = M_DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);

    /* Compute view direction and the dot product of the normal and view direction */

    vec3 V = normalize(uViewPosition - position);
    float NdotV = max(dot(N, V), 0.0);

    /* Compute ambient - IBL diffuse avec Fresnel amélioré */

    vec3 kS = IBL_FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 Nr = M_Rotate3D(N, uQuatSkybox);
    vec3 diffuse = kD * texture(uCubeIrradiance, Nr).rgb;

    FragDiffuse = vec4(diffuse * occlusion * uAmbientEnergy, 1.0);

    /* Skybox reflection - IBL specular amélioré */

    vec3 R = M_Rotate3D(reflect(-V, N), uQuatSkybox);

    const float MAX_REFLECTION_LOD = 7.0;
    float mipLevel = IBL_GetSpecularMipLevel(roughness, MAX_REFLECTION_LOD + 1.0);
    vec3 prefilteredColor = textureLod(uCubePrefilter, R, mipLevel).rgb;

    float specularOcclusion = IBL_GetSpecularOcclusion(NdotV, occlusion, roughness);
    vec3 specularBRDF = IBL_GetMultiScatterBRDF(uTexBrdfLut, NdotV, roughness, F0, metalness);
    vec3 specular = prefilteredColor * specularBRDF * specularOcclusion;

    // Soft falloff hack at low angles to avoid overly bright effect
    float edgeFade = mix(1.0, pow(NdotV, 0.5), roughness);
    specular *= edgeFade;

    FragSpecular = vec4(specular * uReflectEnergy, 1.0);
}

#else

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexSSAO;
uniform sampler2D uTexORM;
uniform vec3 uAmbientColor;
uniform float uAmbientEnergy;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Main === */

void main()
{
    /* Sample albedo and ORM texture and extract values */

    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    float occlusion = orm.r;
    float roughness = orm.g;
    float metalness = orm.b;

    /* Sample SSAO buffer and modulate occlusion value */

	occlusion *= texture(uTexSSAO, vTexCoord).r;

    /* --- Ambient lighting --- */

    // Simplified calculation of diffuse as if NdotV is equal to 1.0 (view facing normal)
    // NOTE: Small tweak here, we also add F0. It's not physically correct, 
    //       but it's to at least simulate some specularity, otherwise the 
    //       result would look poor for metals...

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);
    vec3 kD = (1.0 - F0) * (1.0 - metalness);
    vec3 ambient = kD * uAmbientColor;
    ambient += F0 * uAmbientColor;
    ambient *= uAmbientEnergy;
    ambient *= occlusion;

    /* --- Output --- */

    FragDiffuse = vec4(ambient, 1.0);
    FragSpecular = vec4(0.0);
}

#endif
