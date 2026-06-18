/* lighting.frag -- Fragment shader for applying direct lighting for deferred shading
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
uniform sampler2D uOrmTex;

uniform sampler2DArrayShadow uShadowDirTex;
uniform sampler2DArrayShadow uShadowSpotTex;
uniform samplerCubeArrayShadow uShadowOmniTex;

/* === Blocks === */

#define L_SHADOW_IMPL   //< Shadow functions in blocks/light.glsl

#include "../include/blocks/light.glsl"
#include "../include/blocks/view.glsl"

/* === Fragments === */

layout(location = 0) out vec4 FragDiff;
layout(location = 1) out vec4 FragSpec;

/* === Main === */

void main()
{
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    /* Sample albedo and ORM texture and extract values */

    vec3 albedo = texelFetch(uAlbedoTex, pixCoord, 0).rgb;
    vec4 orm = texelFetch(uOrmTex, pixCoord, 0);
    float roughness = orm.g;
    float metalness = orm.b;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, orm.w, albedo);

    /* Get position and normal in world space */

    float depth = texelFetch(uDepthTex, pixCoord, 0).r;

    vec3 P = V_GetWorldPosition(vTexCoord, depth);
    vec3 N = V_GetWorldNormal(uNormalTex, pixCoord);

    /* Compute view direction and the dot product of the normal and view direction */

    vec3 V = normalize(uView.position - P);
    float NdotV = max(dot(N, V), 1e-4);

    /* Compute light direction and the dot product of the normal and light direction */

    vec3 Ldelta = uLight.position - P;
    float Ldist = length(Ldelta);

    vec3 L = (uLight.type == LIGHT_DIR) ? -uLight.direction : Ldelta / max(Ldist, 1e-4);
    float NdotL = dot(N, L);

    if (NdotL <= 0.0) {
        FragDiff = vec4(0.0);
        FragSpec = vec4(0.0);
        return;
    }

    /* Compute the halfway vector between the view and light directions */

    vec3 H = normalize(V + L);

    float LdotH = max(dot(L, H), 0.0);
    float NdotH = max(dot(N, H), 0.0);

    /* Compute light color energy */

    vec3 lightColE = uLight.color * uLight.energy;

    /* Compute diffuse lighting */

    vec3 diff = L_Diffuse(LdotH, NdotV, NdotL, roughness);
    diff *= albedo * lightColE * (1.0 - metalness);

    /* Compute specular lighting */

    vec3 spec = L_Specular(F0, LdotH, NdotH, NdotV, NdotL, roughness);
    spec *= lightColE * uLight.specular;

    /* Compute shadow factor */

    float shadow = 1.0;

    if (uLight.type != LIGHT_DIR) {
        float atten = pow(1.0 - clamp(Ldist / uLight.range, 0.0, 1.0), uLight.falloff);
        shadow *= atten;
    }

    if (uLight.type == LIGHT_SPOT) {
        float theta = dot(L, -uLight.direction);
        float epsilon = (uLight.innerCutOff - uLight.outerCutOff);
        shadow *= smoothstep(0.0, 1.0, (theta - uLight.outerCutOff) / epsilon);
    }

    if (uLight.shadowLayer >= 0 && uLight.shadowOpacity != 0.0 && shadow > 1e-4) {
        mat2 diskRot = L_ShadowDebandingMatrix(gl_FragCoord.xy);
        switch (uLight.type) {
        case LIGHT_DIR:  shadow *= L_SampleShadowDir(P, depth, NdotL, diskRot); break;
        case LIGHT_SPOT: shadow *= L_SampleShadowSpot(P, NdotL, diskRot); break;
        case LIGHT_OMNI: shadow *= L_SampleShadowOmni(P, NdotL, diskRot); break;
        }
    }

    /* Compute final lighting contribution */

    FragDiff = vec4(diff * shadow, 1.0);
    FragSpec = vec4(spec * shadow, 1.0);
}
