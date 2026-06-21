/* fog.glsl -- Fog functions with explicit parameters, no UBO dependency.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

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
