/* fog.glsl -- Fog data structures and uniform block.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#define FOG_DISABLED 0
#define FOG_LINEAR 1
#define FOG_EXP2 2
#define FOG_EXP 3

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
