/* pbr.glsl -- PBR functions with explicit parameters, no UBO dependency.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

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

vec3 IBL_SamplePrefilter(samplerCubeArray prefilter, int index, vec3 V, vec3 N, float roughness, int numLevels)
{
    float mipLevel = roughness * float(numLevels - 1);
    return textureLod(prefilter, vec4(reflect(-V, N), float(index)), mipLevel).rgb;
}

vec3 IBL_SamplePrefilter(samplerCubeArray prefilter, int index, vec3 V, vec3 N, vec4 rotation, float roughness, int numLevels)
{
    float mipLevel = roughness * float(numLevels - 1);
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
    vec3 FmsD = max(1.0 - (1.0 - Ess) * Favg, vec3(1e-6)); //< avoids division by zero in extreme F0/low roughness cases
    vec3 Fms = FssEss * Favg / FmsD;
    vec3 kD = diffuse * (1.0 - FssEss);

    // Compute final irradiance / radiance
    irradiance *= (Fms * Ems + kD);
    radiance *= FssEss;
}
