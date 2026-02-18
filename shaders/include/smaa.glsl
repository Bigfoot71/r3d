/* smaa.glsl -- Includes SMAA.hlsl with the correct configuration
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

uniform vec4 uMetrics;

#define SMAA_GLSL_3
#define SMAA_RT_METRICS uMetrics
#define SMAA_PRESET_HIGH

#include "../external/SMAA.hlsl"
