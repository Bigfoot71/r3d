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

#include "../include/blocks/light.glsl"
#include "../include/blocks/shadow.glsl"
#include "../include/blocks/view.glsl"

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Main === */

void main()
{
    /* Sample albedo and ORM texture and extract values */
    
    vec3 albedo = texelFetch(uAlbedoTex, ivec2(gl_FragCoord).xy, 0).rgb;
    vec3 orm = texelFetch(uOrmTex, ivec2(gl_FragCoord).xy, 0).rgb;
    float roughness = orm.g;
    float metalness = orm.b;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);

    /* Get position and normal in world space */

    vec3 position = V_GetWorldPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec3 N = V_GetWorldNormal(uNormalTex, ivec2(gl_FragCoord.xy));

    /* Compute view direction and the dot product of the normal and view direction */

    vec3 V = normalize(uView.position - position);

    float NdotV = dot(N, V);
    float cNdotV = max(NdotV, 1e-4); // Clamped to avoid division by zero

    /* Compute light direction and the dot product of the normal and light direction */

    vec3 L = (uLight.type == LIGHT_DIR) ? -uLight.direction : normalize(uLight.position - position);

    float NdotL = max(dot(N, L), 0.0);
    float cNdotL = min(NdotL, 1.0); // Clamped to avoid division by zero

    /* Compute the halfway vector between the view and light directions */

    vec3 H = normalize(V + L);

    float LdotH = max(dot(L, H), 0.0);
    float cLdotH = min(LdotH, 1.0);

    float NdotH = max(dot(N, H), 0.0);
    float cNdotH = min(NdotH, 1.0);

    /* Compute light color energy */

    vec3 lightColE = uLight.color * uLight.energy;

    /* Compute diffuse lighting */

    vec3 diffuse = L_Diffuse(cLdotH, cNdotV, cNdotL, roughness);
    diffuse *= albedo * lightColE * (1.0 - metalness);

    /* Compute specular lighting */

    vec3 specular = L_Specular(F0, cLdotH, cNdotH, cNdotV, cNdotL, roughness);
    specular *= lightColE * uLight.specular;

    /* --- Calculating a random rotation matrix for shadow debanding --- */

    mat2 diskRot = S_DebandingRotationMatrix();

    /* Apply shadow factor if the light casts shadows */

    float shadow = 1.0;

    if (uLight.shadowLayer >= 0) {
        switch (uLight.type) {
        case LIGHT_DIR:  shadow = S_SampleShadowDir(uLight.viewProj * vec4(position, 1.0), cNdotL, diskRot); break;
        case LIGHT_SPOT: shadow = S_SampleShadowSpot(uLight.viewProj * vec4(position, 1.0), cNdotL, diskRot); break;
        case LIGHT_OMNI: shadow = S_SampleShadowOmni(position, cNdotL, diskRot); break;
        }
    }

    /* Apply attenuation based on the distance from the light */

    if (uLight.type != LIGHT_DIR)
    {
        float dist = length(uLight.position - position);
        float atten = 1.0 - clamp(dist / uLight.range, 0.0, 1.0);
        shadow *= atten * uLight.attenuation;
    }

    /* Apply spotlight effect if the light is a spotlight */

    if (uLight.type == LIGHT_SPOT)
    {
        float theta = dot(L, -uLight.direction);
        float epsilon = (uLight.innerCutOff - uLight.outerCutOff);
        shadow *= smoothstep(0.0, 1.0, (theta - uLight.outerCutOff) / epsilon);
    }

    /* Compute final lighting contribution */

    FragDiffuse = vec4(diffuse * shadow, 1.0);
    FragSpecular = vec4(specular * shadow, 1.0);
}
