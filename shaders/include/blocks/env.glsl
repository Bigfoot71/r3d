/* env.glsl -- Contains everything you need to manage environment
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Includes === */

#include "../math.glsl"

/* === Structures === */

struct EnvAmbient {
    vec4 rotation;
    vec4 color;
    float energy;
    int irradiance;
    int prefilter;
};

struct EnvProbe {
    vec3 position;
    float falloff;
    float range;
    int irradiance;
    int prefilter;
};

/* === Uniform Block === */

layout(std140) uniform EnvBlock {
    EnvProbe uProbes[NUM_PROBES];
    EnvAmbient uAmbient;
    int uNumPrefilterLevels;
    int uNumProbes;
};

/* === IBL Functions === */

float IBL_GetSpecularOcclusion(float NdotV, float ao, float roughness)
{
    // Lagarde method: https://seblagarde.wordpress.com/wp-content/uploads/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
    return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

vec3 IBL_SampleIrradiance(samplerCubeArray irradiance, int index, vec3 N)
{
    return texture(irradiance, vec4(N, float(index))).rgb;
}

vec3 IBL_SampleIrradiance(samplerCubeArray irradiance, int index, vec3 N, vec4 rotation)
{
    return texture(irradiance, vec4(M_Rotate3D(N, rotation), float(index))).rgb;
}

vec3 IBL_SamplePrefilter(samplerCubeArray prefilter, int index, vec3 V, vec3 N, float roughness)
{
    float mipLevel = roughness * float(uNumPrefilterLevels - 1);
    return textureLod(prefilter, vec4(reflect(-V, N), float(index)), mipLevel).rgb;
}

vec3 IBL_SamplePrefilter(samplerCubeArray prefilter, int index, vec3 V, vec3 N, vec4 rotation, float roughness)
{
    float mipLevel = roughness * float(uNumPrefilterLevels - 1);
    return textureLod(prefilter, vec4(M_Rotate3D(reflect(-V, N), rotation), float(index)), mipLevel).rgb;
}

void IBL_MultiScattering(inout vec3 irradiance, inout vec3 radiance, vec3 diffuse, vec3 F0, vec2 brdf, float NdotV, float roughness)
{
    // Adapted from Fdez-Aguera method without the roughness-dependent Fresnel
    // See: https://jcgt.org/published/0008/01/03/paper.pdf

    // Single scattering with standard Fresnel
    vec3 FssEss = F0 * brdf.x + brdf.y;

    // Multiple scattering, from Fdez-Aguera
    float Ess = brdf.x + brdf.y;
    float Ems = 1.0 - Ess;
    vec3 Favg = F0 + (1.0 - F0) / 21.0;
    vec3 Fms = FssEss * Favg / (1.0 - (1.0 - Ess) * Favg);
    vec3 kD = diffuse * (1.0 - FssEss);

    // Compute final irradiance / radiance
    irradiance *= (Fms * Ems + kD);
    radiance *= FssEss;
}

void IBL_SampleProbe(inout vec3 irr, inout vec3 rad, inout float wIrr, inout float wRad, int probeIndex, float roughness, vec3 P, vec3 N, vec3 V)
{
    EnvProbe probe = uProbes[probeIndex];
    float dist = length(P - probe.position);
    float weight = pow(clamp(1.0 - dist / probe.range, 0.0, 1.0), probe.falloff);

    if (weight < 1e-6) return;

    if (probe.irradiance >= 0) {
        vec3 probeIrr = IBL_SampleIrradiance(uIrradianceTex, probe.irradiance, N);
        irr += probeIrr.rgb * weight;
        wIrr += weight;
    }

    if (probe.prefilter >= 0) {
        vec3 probeRad = IBL_SamplePrefilter(uPrefilterTex, probe.prefilter, V, N, roughness);
        rad += probeRad.rgb * weight;
        wRad += weight;
    }
}

/* === Environment Functions === */

void E_ComputeAmbientAndProbes(inout vec3 diffuse, inout vec3 specular, vec3 kD, vec3 orm, vec3 F0, vec3 P, vec3 N, vec3 V, float NdotV)
{
    float occlusion = orm.x;
    float roughness = orm.y;
    float metalness = orm.z;

    vec3 irradiance = vec3(0.0);
    float wIrradiance = 0.0;

    vec3 radiance = vec3(0.0);
    float wRadiance = 0.0;

    for (int i = 0; i < uNumProbes; ++i) {
        IBL_SampleProbe(irradiance, radiance, wIrradiance, wRadiance, i, roughness, P, N, V);
    }

    if (wIrradiance > 1.0) {
        float invTotalWeight = 1.0 / wIrradiance;
        irradiance *= invTotalWeight;
        wIrradiance = 1.0;
    }

    if (wRadiance > 1.0) {
        float invTotalWeight = 1.0 / wRadiance;
        radiance *= invTotalWeight;
        wRadiance = 1.0;
    }

    if (wIrradiance < 1.0) {
        vec3 ambientIrr = vec3(0.0);
        if (uAmbient.irradiance < 0) ambientIrr = uAmbient.color.rgb;
        else ambientIrr = IBL_SampleIrradiance(uIrradianceTex, uAmbient.irradiance, N, uAmbient.rotation);
        irradiance += ambientIrr * (1.0 - wIrradiance);
    }

    if (wRadiance < 1.0 && uAmbient.prefilter >= 0) {
        vec3 ambientRad = IBL_SamplePrefilter(uPrefilterTex, uAmbient.prefilter, V, N, uAmbient.rotation, roughness);
        radiance += ambientRad * (1.0 - wRadiance);
    }

    irradiance *= occlusion * uAmbient.energy;
    radiance *= IBL_GetSpecularOcclusion(NdotV, occlusion, roughness);

    vec2 brdf = texture(uBrdfLutTex, vec2(NdotV, roughness)).xy;
    IBL_MultiScattering(irradiance, radiance, kD, F0, brdf, NdotV, roughness);

    diffuse += irradiance;
    specular += radiance;
}

void E_ComputeAmbientOnly(inout vec3 diffuse, inout vec3 specular, vec3 kD, vec3 orm, vec3 F0, vec3 P, vec3 N, vec3 V, float NdotV)
{
    float occlusion = orm.x;
    float roughness = orm.y;
    float metalness = orm.z;

    vec3 irradiance = (uAmbient.irradiance >= 0)
        ? IBL_SampleIrradiance(uIrradianceTex, uAmbient.irradiance, N, uAmbient.rotation).rgb
        : uAmbient.color.rgb;
    irradiance *= occlusion * uAmbient.energy;

    vec3 radiance = vec3(0.0);
    if (uAmbient.prefilter >= 0) {
        radiance = IBL_SamplePrefilter(uPrefilterTex, uAmbient.prefilter, V, N, uAmbient.rotation, roughness).rgb;
        radiance *= IBL_GetSpecularOcclusion(NdotV, occlusion, roughness);
    }

    vec2 brdf = texture(uBrdfLutTex, vec2(NdotV, roughness)).xy;
    IBL_MultiScattering(irradiance, radiance, kD, F0, brdf, NdotV, roughness);

    diffuse += irradiance;
    specular += radiance;
}

void E_ComputeAmbientColor(inout vec3 diffuse, vec3 kD, float occlusion)
{
    diffuse += kD * uAmbient.color.rgb * uAmbient.energy * occlusion;
}
