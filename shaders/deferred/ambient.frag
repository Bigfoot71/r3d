/* ambient.frag -- Fragment shader for applying ambient lighting for deferred shading
 *
 * Copyright (c) 2025-2026 Le Juez Victor
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

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;
uniform sampler2D uSsaoTex;
uniform sampler2D uSsgiTex;
uniform sampler2D uSsilTex;
uniform sampler2D uOrmTex;

uniform samplerCubeArray uIrradianceTex;
uniform samplerCubeArray uPrefilterTex;
uniform sampler2D uBrdfLutTex;

uniform float uSsaoPower;
uniform float uSsilIntensity;
uniform float uSsilAoPower;

/* === Blocks === */

#include "../include/blocks/view.glsl"
#include "../include/blocks/env.glsl"

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Main === */

void main()
{
    vec3 albedo = texelFetch(uAlbedoTex, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 orm = texelFetch(uOrmTex, ivec2(gl_FragCoord).xy, 0).rgb;
    float ssao = texture(uSsaoTex, vTexCoord).r;
    vec4 ssil = texture(uSsilTex, vTexCoord);
    vec4 ssgi = texture(uSsgiTex, vTexCoord);

    // Compute visibility factor, used during IBL
    orm.x *= pow(ssil.a, uSsilAoPower); 
    orm.x *= pow(ssao, uSsaoPower);

    vec3 F0 = PBR_ComputeF0(orm.z, 0.5, albedo);
    vec3 kD = albedo * (1.0 - orm.z);

    vec3 P = V_GetWorldPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec3 N = V_GetWorldNormal(uNormalTex, ivec2(gl_FragCoord.xy));
    vec3 V = normalize(uView.position - P);
    float NdotV = max(dot(N, V), 0.0);

    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    E_ComputeAmbientAndProbes(diffuse, specular, kD, orm, F0, P, N, V, NdotV);

    // Apply AO to SSGI to restore contrast lost in far/blurred regions
    vec3 gi = kD * (ssil.rgb * uSsilIntensity + ssgi.rgb * orm.x);

    FragDiffuse = vec4(diffuse + gi, 1.0);
    FragSpecular = vec4(specular, 1.0);
}
