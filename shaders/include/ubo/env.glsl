/* env.glsl -- Env data structures and uniform block.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

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

layout(std140) uniform EnvBlock {
    EnvProbe uProbes[NUM_PROBES];
    EnvAmbient uAmbient;
    int uNumPrefilterLevels;
    int uNumProbes;
};
