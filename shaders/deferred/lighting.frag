/* lighting.frag -- Fragment shader for applying direct lighting for deferred shading
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/light.glsl"
#include "../include/math.glsl"
#include "../include/pbr.glsl"

/* === Structs === */

struct Light
{
    mat4 matVP;                     //< View/projection matrix of the light, used for directional and spot shadow projection
    sampler2D shadowMap;            //< 2D shadow map used for directional and spot shadow projection
    samplerCube shadowCubemap;      //< Cube shadow map used for omni-directional shadow projection
    vec3 color;                     //< Light color modulation tint
    vec3 position;                  //< Light position (spot/omni)
    vec3 direction;                 //< Light direction (spot/dir)
    float specular;                 //< Specular factor (not physically accurate but provides more flexibility)
    float energy;                   //< Light energy factor
    float range;                    //< Maximum distance the light can travel before being completely attenuated (spot/omni)
    float size;                     //< Light size, currently used only for shadows (PCSS)
    float near;                     //< Near plane for the shadow map projection
    float far;                      //< Far plane for the shadow map projection
    float attenuation;              //< Additional light attenuation factor (spot/omni)
    float innerCutOff;              //< Spot light inner cutoff angle
    float outerCutOff;              //< Spot light outer cutoff angle
    float shadowSoftness;           //< Softness factor to simulate a penumbra
    float shadowTexelSize;          //< Size of a texel in the 2D shadow map
    float shadowDepthBias;          //< Constant depth bias applied to shadow mapping to reduce shadow acne
    float shadowSlopeBias;          //< Additional bias scaled by surface slope to reduce artifacts on angled geometry
    lowp int type;                  //< Light type (dir/spot/omni)
    bool shadow;                    //< Indicates whether the light generates shadows
};

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexDepth;
uniform sampler2D uTexSSAO;
uniform sampler2D uTexORM;

uniform Light uLight;

uniform float uSSAOLightAffect;

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

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

/* === Shadow functions === */

float ShadowDir(vec3 position, float cNdotL, mat2 diskRot)
{
    /* --- Light Space Projection --- */

    vec4 projPos = uLight.matVP * vec4(position, 1.0);
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = uLight.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, uLight.shadowDepthBias * projCoords.z);
    float currentDepth = projCoords.z - bias;

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * uLight.shadowSoftness;
        shadow += step(currentDepth, texture(uLight.shadowMap, projCoords.xy + offset).r);
    }
    shadow /= float(SHADOW_SAMPLES);

    /* --- Apply a fade to the edges of the projection --- */

    vec3 distToBorder = min(projCoords, 1.0 - projCoords);
    float edgeFade = smoothstep(0.0, 0.15, min(distToBorder.x, min(distToBorder.y, distToBorder.z)));
    shadow = mix(1.0, shadow, edgeFade);

    /* --- Final Shadow Value --- */

    return shadow;
}

float ShadowSpot(vec3 position, float cNdotL, mat2 diskRot)
{
    /* --- Light Space Projection --- */

    vec4 projPos = uLight.matVP * vec4(position, 1.0);
    vec3 projCoords = projPos.xyz / projPos.w * 0.5 + 0.5;

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = uLight.shadowSlopeBias * (1.0 - cNdotL);
    bias = max(bias, uLight.shadowDepthBias * projCoords.z);
    float currentDepth = projCoords.z - bias;

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 offset = diskRot * VOGEL_DISK[i] * uLight.shadowSoftness;
        shadow += step(currentDepth, texture(uLight.shadowMap, projCoords.xy + offset).r);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

float ShadowOmni(vec3 position, float cNdotL, mat2 diskRot)
{
    /* --- Light Vector and Distance Calculation --- */

    vec3 lightToFrag = position - uLight.position;
    float currentDepth = length(lightToFrag);
    vec3 direction = normalize(lightToFrag);

    /* --- Shadow Bias and Depth Adjustment --- */

    float bias = uLight.shadowSlopeBias * (1.0 - cNdotL * 0.5);
    bias = max(bias, uLight.shadowDepthBias * currentDepth);
    currentDepth -= bias;

    /* --- Build orthonormal basis for perturbation --- */

    mat3 OBN = M_OrthonormalBasis(direction);

    /* --- Poisson Disk PCF Sampling --- */

    float shadow = 0.0;
    for (int i = 0; i < SHADOW_SAMPLES; ++i) {
        vec2 diskOffset = diskRot * VOGEL_DISK[i] * uLight.shadowSoftness;
        vec3 sampleDir = normalize(OBN * vec3(diskOffset.xy, 1.0));
        float sampleDepth = texture(uLight.shadowCubemap, sampleDir).r * uLight.far;
        shadow += step(currentDepth, sampleDepth);
    }

    /* --- Final Shadow Value --- */

    return shadow / float(SHADOW_SAMPLES);
}

/* === Main === */

void main()
{
    /* Sample albedo and ORM texture and extract values */
    
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 orm = texture(uTexORM, vTexCoord).rgb;
    float roughness = orm.g;
    float metalness = orm.b;

    /* Compute F0 (reflectance at normal incidence) based on the metallic factor */

    vec3 F0 = PBR_ComputeF0(metalness, 0.5, albedo);

    /* Get position and normal in world space */

    vec3 position = V_GetWorldPosition(uTexDepth, vTexCoord);
    vec3 N = V_GetWorldNormal(uTexNormal, vTexCoord);

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

    float r = M_TAU * M_HashIGN(gl_FragCoord.xy);
    float sr = sin(r), cr = cos(r);

    mat2 diskRot = mat2(vec2(cr, -sr), vec2(sr, cr));

    /* Apply shadow factor if the light casts shadows */

    float shadow = 1.0;

    if (uLight.shadow) {
        switch (uLight.type) {
        case LIGHT_DIR: shadow = ShadowDir(position, cNdotL, diskRot); break;
        case LIGHT_SPOT: shadow = ShadowSpot(position, cNdotL, diskRot); break;
        case LIGHT_OMNI: shadow = ShadowOmni(position, cNdotL, diskRot); break;
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

	/* Apply SSAO to diffuse lighting (accordingly to light affect) */

    diffuse *= mix(1.0, texture(uTexSSAO, vTexCoord).r, uSSAOLightAffect);

    /* Compute final lighting contribution */

    FragDiffuse = vec4(diffuse * shadow, 1.0);
    FragSpecular = vec4(specular * shadow, 1.0);
}
