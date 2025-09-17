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
uniform float uSkyboxAmbientIntensity;
uniform float uSkyboxReflectIntensity;
uniform float uSSAOPower;

uniform vec3 uViewPosition;
uniform mat4 uMatInvProj;
uniform mat4 uMatInvView;

/* === Fragments === */

layout(location = 0) out vec3 FragDiffuse;
layout(location = 1) out vec3 FragSpecular;

/* === Misc functions === */

vec3 GetPositionFromDepth(float depth)
{
    vec4 ndcPos = vec4(vTexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    viewPos /= viewPos.w;

    return (uMatInvView * viewPos).xyz;
}

vec2 OctahedronWrap(vec2 val)
{
    // Reference(s):
    // - Octahedron normal vector encoding
    //   https://web.archive.org/web/20191027010600/https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/comment-page-1/
    return (1.0 - abs(val.yx)) * mix(vec2(-1.0), vec2(1.0), vec2(greaterThanEqual(val.xy, vec2(0.0))));
}

vec3 DecodeOctahedral(vec2 encoded)
{
    encoded = encoded * 2.0 - 1.0;

    vec3 normal;
    normal.z  = 1.0 - abs(encoded.x) - abs(encoded.y);
    normal.xy = normal.z >= 0.0 ? encoded.xy : OctahedronWrap(encoded.xy);
    return normalize(normal);
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

    float ssao = texture(uTexSSAO, vTexCoord).r;

    if (uSSAOPower != 1.0) {
        ssao = pow(ssao, uSSAOPower);
    }

    occlusion *= ssao;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);

    /* Sample world depth and reconstruct world position */

    float depth = texture(uTexDepth, vTexCoord).r;
    vec3 position = GetPositionFromDepth(depth);

    /* Sample and decode normal in world space */

    vec3 N = DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);

    /* Compute view direction and the dot product of the normal and view direction */

    vec3 V = normalize(uViewPosition - position);
    float NdotV = max(dot(N, V), 0.0);

    /* Compute ambient - IBL diffuse avec Fresnel amélioré */

    vec3 kS = IBL_FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 Nr = M_Rotate3D(N, uQuatSkybox);
    FragDiffuse = kD * texture(uCubeIrradiance, Nr).rgb;
    FragDiffuse *= occlusion * uSkyboxAmbientIntensity;

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

    FragSpecular = specular * uSkyboxReflectIntensity;
}

#else

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexSSAO;
uniform sampler2D uTexORM;
uniform vec3 uAmbientColor;
uniform float uSSAOPower;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;

/* === Main === */

void main()
{
    /* --- Material properties --- */

    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    float occlusion = orm.r;
    float roughness = orm.g;
    float metalness = orm.b;

    /* --- Ambient occlusion (SSAO) --- */

    float ssao = texture(uTexSSAO, vTexCoord).r;

	if (uSSAOPower != 1.0) {
        ssao = pow(ssao, uSSAOPower);
    }

	occlusion *= ssao;

    /* --- Ambient lighting --- */

    // Simplified calculation of diffuse as if NdotV is equal to 1.0 (view facing normal)
    // NOTE: Small tweak here, we also add F0. It's not physically correct, 
    //       but it's to at least simulate some specularity, otherwise the 
    //       result would look poor for metals...

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);
    vec3 kD = (1.0 - F0) * (1.0 - metalness);
    vec3 ambient = kD * uAmbientColor;
    ambient += F0 * uAmbientColor;
    ambient *= albedo * occlusion;

    /* --- Output --- */

    FragDiffuse = vec4(ambient, 1.0);
}

#endif
