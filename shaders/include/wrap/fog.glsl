/* fog.glsl -- Fog helpers wrapping UBO and LIB for direct use in shaders.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "../ubo/fx.glsl"
#include "../lib/fog.glsl"

float FogFactor(float dist)
{
    if (uFog.mode == FOG_LINEAR) return FogFactorLinear(dist, uFog.start, uFog.end);
    if (uFog.mode == FOG_EXP2) return FogFactorExp2(dist, uFog.density);
    if (uFog.mode == FOG_EXP) return FogFactorExp(dist, uFog.density);
    return 0.0; // FOG_DISABLED
}

vec4 FogColorAlpha(float depth, float far)
{
    return vec4(uFog.color.rgb, (depth >= far) ? uFog.skyAffect : FogFactor(depth));
}

vec3 FogColorMix(vec3 color, float depth)
{
    return mix(color, uFog.color.rgb, FogFactor(depth));
}

vec4 FogColorMix(vec4 color, float depth)
{
    return vec4(mix(color.rgb, uFog.color.rgb, FogFactor(depth)), color.a);
}
