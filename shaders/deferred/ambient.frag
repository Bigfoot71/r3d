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
uniform float uSsilAoPower;
uniform float uSsilIntensity;
uniform float uSsgiIntensity;

uniform bool uSsaoEnabled;
uniform bool uSsilEnabled;
uniform bool uSsgiEnabled;

/* === Blocks === */

#include "../include/blocks/view.glsl"
#include "../include/blocks/env.glsl"

/* === Fragments === */

layout(location = 0) out vec4 FragDiffuse;
layout(location = 1) out vec4 FragSpecular;

/* === Upsampling functions === */

struct UpsampleWeights {
    ivec2 p00, p10, p01, p11;
    float w00, w10, w01, w11;
    float invWSum;
};

UpsampleWeights ComputeUpsampleWeights(float refDepth, float NdotV)
{
    ivec2 depthRes = textureSize(uDepthTex, 1);
    ivec2 maxCoord = depthRes - ivec2(1);

    vec2 pixLow = vTexCoord * vec2(depthRes) - 0.5;
    vec2 base = floor(pixLow);
    vec2 f = pixLow - base;

    UpsampleWeights r;
    r.p00 = clamp(ivec2(base),               ivec2(0), maxCoord);
    r.p10 = clamp(ivec2(base) + ivec2(1, 0), ivec2(0), maxCoord);
    r.p01 = clamp(ivec2(base) + ivec2(0, 1), ivec2(0), maxCoord);
    r.p11 = clamp(ivec2(base) + ivec2(1, 1), ivec2(0), maxCoord);

    vec4 d = vec4(
        texelFetch(uDepthTex, r.p00, 1).r,
        texelFetch(uDepthTex, r.p10, 1).r,
        texelFetch(uDepthTex, r.p01, 1).r,
        texelFetch(uDepthTex, r.p11, 1).r
    );

    vec4 w = vec4(
        (1.0 - f.x) * (1.0 - f.y),
        f.x * (1.0 - f.y),
        (1.0 - f.x) * f.y,
        f.x * f.y
    );

    const float kMinNdotV = 0.05;
    const float kDepthEdgeToleranceMeters = 0.05;
    float depthSharpness = max(NdotV, kMinNdotV) / kDepthEdgeToleranceMeters; // = 1.0 / (t / max(NdotV, min))

    w *= exp(-abs(d - vec4(refDepth)) * depthSharpness);

    r.w00 = w.x;
    r.w10 = w.y;
    r.w01 = w.z;
    r.w11 = w.w;

    r.invWSum = 1.0 / max(w.x + w.y + w.z + w.w, 1e-5);

    return r;
}

vec4 Upsample(sampler2D source, UpsampleWeights uw)
{
    vec4 c00 = texelFetch(source, uw.p00, 0);
    vec4 c10 = texelFetch(source, uw.p10, 0);
    vec4 c01 = texelFetch(source, uw.p01, 0);
    vec4 c11 = texelFetch(source, uw.p11, 0);

    return (c00 * uw.w00 + c10 * uw.w10 + c01 * uw.w01 + c11 * uw.w11) * uw.invWSum;
}

/* === Main === */

void main()
{
    ivec2 pixCoord = ivec2(gl_FragCoord.xy);

    vec3 albedo = texelFetch(uAlbedoTex, pixCoord, 0).rgb;
    float depth = texelFetch(uDepthTex, pixCoord, 0).r;
    vec4 orm = texelFetch(uOrmTex, pixCoord, 0);

    vec3 P = V_GetWorldPosition(vTexCoord, depth);
    vec3 N = V_GetWorldNormal(uNormalTex, pixCoord);
    vec3 V = normalize(uView.position - P);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = PBR_ComputeF0(orm.z, orm.w, albedo);
    vec3 kD = albedo * (1.0 - orm.z);

    vec4 io = vec4(0.0, 0.0, 0.0, 1.0);
    if (uSsilEnabled || uSsaoEnabled || uSsgiEnabled)
    {
        UpsampleWeights uw = ComputeUpsampleWeights(depth, NdotV);
        if (uSsilEnabled) {
            io = Upsample(uSsilTex, uw);
            io.rgb *= uSsilIntensity;
            io.a = pow(io.a, uSsilAoPower);
        }
        if (uSsaoEnabled) {
            float ao = Upsample(uSsaoTex, uw).r;
            io.a *= pow(ao, uSsaoPower);
        }
        if (uSsgiEnabled) {
            vec3 gi = Upsample(uSsgiTex, uw).rgb;
            io.rgb += gi * uSsgiIntensity;
        }
        orm.x *= io.a;
        io.rgb *= kD;
    }

    vec3 diff = vec3(0.0);
    vec3 spec = vec3(0.0);
    E_ComputeAmbientAndProbes(diff, spec, kD, orm.rgb, F0, P, N, V, NdotV);

    FragDiffuse = vec4(diff + io.rgb, 1.0);
    FragSpecular = vec4(spec, 1.0);
}
