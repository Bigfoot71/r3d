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
uniform sampler2D uTexSSIL;
uniform sampler2D uTexSSR;
uniform sampler2D uTexORM;

uniform samplerCube uCubeIrradiance;
uniform samplerCube uCubePrefilter;
uniform sampler2D uTexBrdfLut;

uniform float uAmbientEnergy;
uniform float uReflectEnergy;
uniform float uMipCountSSR;
uniform vec4 uQuatSkybox;

uniform vec3 uViewPosition;
uniform mat4 uMatInvProj;
uniform mat4 uMatInvView;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Misc functions === */

vec3 GetWorldPosition(vec2 texCoord)
{
    float depth = texture(uTexDepth, texCoord).r;

    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    viewPos *= 1.0 / viewPos.w;

    return (uMatInvView * viewPos).xyz;
}

/* === Main === */

void main()
{
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    vec4 ssr = textureLod(uTexSSR, vTexCoord, orm.g * uMipCountSSR);
    float ssao = texture(uTexSSAO, vTexCoord).r;
    vec4 ssil = texture(uTexSSIL, vTexCoord);

    float occlusion = orm.r * ssao * ssil.a;
    float roughness = orm.g;
    float metalness = orm.b;

    vec3 position = GetWorldPosition(vTexCoord);

    vec3 N = M_DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);
    vec3 V = normalize(uViewPosition - position);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);

    /* --- Diffuse --- */

    vec3 kS = IBL_FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD = (1.0 - kS) * (1.0 - metalness);

    vec3 Nr = M_Rotate3D(N, uQuatSkybox);
    vec3 irradiance = texture(uCubeIrradiance, Nr).rgb;
    vec3 diffuse = albedo * kD * (irradiance + ssil.rgb);

    FragDiffuse = vec4(diffuse * occlusion * uAmbientEnergy, 1.0);

    /* --- Specular --- */

    vec3 R = M_Rotate3D(reflect(-V, N), uQuatSkybox);

    const float MAX_REFLECTION_LOD = 7.0;
    vec3 radiance = textureLod(uCubePrefilter, R, roughness * MAX_REFLECTION_LOD).rgb;
    float specularOcclusion = IBL_GetSpecularOcclusion(NdotV, occlusion, roughness);
    vec3 specularBRDF = IBL_GetMultiScatterBRDF(uTexBrdfLut, NdotV, roughness, F0);

    vec3 specular = mix(radiance, ssr.rgb, ssr.a) * specularBRDF;
    specular *= specularOcclusion * uReflectEnergy;
    FragSpecular = vec4(specular, 1.0);
}

#else

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexSSAO;
uniform sampler2D uTexSSIL;
uniform sampler2D uTexSSR;
uniform sampler2D uTexORM;

uniform vec3 uAmbientColor;
uniform float uAmbientEnergy;
uniform float uMipCountSSR;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Main === */

void main()
{
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    vec4 ssr = textureLod(uTexSSR, vTexCoord, orm.g * uMipCountSSR);
    float ssao = texture(uTexSSAO, vTexCoord).r;
    vec4 ssil = texture(uTexSSIL, vTexCoord);

    vec3 ambient = albedo * (uAmbientColor + ssil.rgb);
    ambient *= orm.r * ssao * ssil.a;
    ambient *= (1.0 - orm.b);

    FragDiffuse = vec4(ambient * uAmbientEnergy, 1.0);

    // Small tweak to get a consistent SSR
    vec3 F0 = mix(vec3(0.04), albedo, orm.b);
    vec3 specular = ssr.rgb * F0 * ssr.a * orm.r;
    FragSpecular = vec4(specular, 1.0);
}

#endif
