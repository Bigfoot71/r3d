/* ambient.frag -- Fragment shader for applying ambient lighting for deferred shading
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

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uAlbedoTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;
uniform sampler2D uSsaoTex;
uniform sampler2D uSsilTex;
uniform sampler2D uSsrTex;
uniform sampler2D uOrmTex;

uniform samplerCubeArray uIrradianceTex;
uniform samplerCubeArray uPrefilterTex;
uniform sampler2D uBrdfLutTex;

uniform float uMipCountSSR;

/* === Blocks === */

#include "../include/blocks/view.glsl"
#include "../include/blocks/env.glsl"

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Main === */

void main()
{
    vec3 albedo = texture(uAlbedoTex, vTexCoord).rgb;
    vec3 orm = texture(uOrmTex, vTexCoord).rgb;

    vec4 ssr = textureLod(uSsrTex, vTexCoord, orm.g * uMipCountSSR);
    float ssao = texture(uSsaoTex, vTexCoord).r;
    vec4 ssil = texture(uSsilTex, vTexCoord);

    orm.x *= ssao * ssil.w;

    vec3 F0 = PBR_ComputeF0(orm.z, 0.5, albedo);
    vec3 P = V_GetWorldPosition(uDepthTex, vTexCoord);
    vec3 N = V_GetWorldNormal(uNormalTex, vTexCoord);
    vec3 V = normalize(uView.position - P);
    float NdotV = max(dot(N, V), 0.0);
    vec3 kD = albedo * (1.0 - orm.z);

    vec3 irradiance = vec3(0.0);
    vec3 radiance = vec3(0.0);

    E_ComputeAmbientAndProbes(irradiance, radiance, kD, orm, F0, P, N, V, NdotV);
    irradiance += ssil.rgb * kD * ssil.rgb * orm.x;

    vec3 kS_approx = F0 * (1.0 - orm.y * 0.5);
    radiance = mix(radiance, kS_approx * ssr.rgb, ssr.w);

    FragDiffuse = vec4(irradiance, 1.0);
    FragSpecular = vec4(radiance, 1.0);
}
