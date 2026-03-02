/* fog.glsl -- Contains everything you need for the fog
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Definitions === */

#define FOG_DISABLED 0
#define FOG_LINEAR 1
#define FOG_EXP2 2
#define FOG_EXP 3

/* === Uniform Block === */

struct Fog {
    vec3 color;
    float start;
    float end;
    float density;
    float skyAffect;
    int mode;
};

layout(std140) uniform FogBlock {
    Fog uFog;
};

/* === Functions === */

float FogFactorLinear(float dist, float start, float end)
{
    return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}

float FogFactorExp2(float dist, float density)
{
    const float LOG2 = -1.442695;
    float d = density * dist;
    return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

float FogFactorExp(float dist, float density)
{
    return 1.0 - clamp(exp(-density * dist), 0.0, 1.0);
}

float FogFactor(float dist)
{
    if (uFog.mode == FOG_LINEAR) return FogFactorLinear(dist, uFog.start, uFog.end);
    if (uFog.mode == FOG_EXP2) return FogFactorExp2(dist, uFog.density);
    if (uFog.mode == FOG_EXP) return FogFactorExp(dist, uFog.density);
    return 0.0; // FOG_DISABLED
}

vec4 FogColorAlpha(float dist)
{
    return vec4(uFog.color, FogFactor(dist));
}

vec3 FogColorMix(vec3 color, float dist)
{
    return mix(color, uFog.color, FogFactor(dist));
}

vec4 FogColorMix(vec4 color, float dist)
{
    return vec4(mix(color.rgb, uFog.color, FogFactor(dist)), color.a);
}

vec3 FogSkyMix(vec3 sky)
{
    if (uFog.mode == FOG_DISABLED) return sky;
    return mix(sky, uFog.color, uFog.skyAffect);
}
