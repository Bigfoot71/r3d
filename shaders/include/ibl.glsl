/* ibl.glsl -- Contains everything you need to manage IBL
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Functions === */

vec3 IBL_FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    // TODO: See approximations, but this version seems to introduce less bias for grazing angles
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float IBL_GetSpecularOcclusion(float NdotV, float ao, float roughness)
{
    // Lagarde method: https://seblagarde.wordpress.com/wp-content/uploads/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
    return clamp(pow(NdotV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao, 0.0, 1.0);
}

vec3 IBL_GetMultiScatterBRDF(sampler2D brdfLut, float NdotV, float roughness, vec3 F0)
{
    // Adapted from: https://blog.selfshadow.com/publications/turquin/ms_comp_final.pdf
    vec2 brdf = texture(brdfLut, vec2(NdotV, roughness)).rg;

    // Single scatter (Karis approximation)
    vec3 Fr = max(vec3(1.0 - roughness), F0) - F0;
    vec3 kS = F0 + Fr * pow(1.0 - NdotV, 5.0);

    // Multiscatter compensation (Fdez-Agüera)
    vec3 FssEss = kS * brdf.x + brdf.y;
    float Ess = brdf.x + brdf.y;
    float Ems = 1.0 - Ess;
    vec3 Favg = F0 + (1.0 - F0) / 21.0;
    vec3 Fms = FssEss * Favg / (1.0 - Favg * Ems);

    return FssEss + Fms * Ems;
}
