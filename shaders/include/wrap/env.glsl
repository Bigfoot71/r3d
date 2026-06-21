/* env.glsl -- Env helpers wrapping UBO and LIB for direct use in shaders.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "../ubo/env.glsl"
#include "../lib/ibl.glsl"

void E_SampleProbe(inout vec3 irr, inout vec3 rad, inout float wIrr, inout float wRad, int probeIndex, float roughness, vec3 P, vec3 N, vec3 V)
{
    E_Probe probe = uProbes[probeIndex];
    float dist = length(P - probe.position);
    float weight = pow(clamp(1.0 - dist / probe.range, 0.0, 1.0), probe.falloff);

    if (weight < 1e-6) return;

    if (probe.irradiance >= 0) {
        vec3 probeIrr = IBL_SampleIrradiance(uIrradianceTex, probe.irradiance, N);
        irr += probeIrr.rgb * weight;
        wIrr += weight;
    }

    if (probe.prefilter >= 0) {
        vec3 probeRad = IBL_SamplePrefilter(uPrefilterTex, probe.prefilter, V, N, roughness, uNumPrefilterLevels);
        rad += probeRad.rgb * weight;
        wRad += weight;
    }
}

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
        E_SampleProbe(irradiance, radiance, wIrradiance, wRadiance, i, roughness, P, N, V);
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
        vec3 ambientRad = IBL_SamplePrefilter(uPrefilterTex, uAmbient.prefilter, V, N, uAmbient.rotation, roughness, uNumPrefilterLevels);
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
        radiance = IBL_SamplePrefilter(uPrefilterTex, uAmbient.prefilter, V, N, uAmbient.rotation, roughness, uNumPrefilterLevels).rgb;
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
