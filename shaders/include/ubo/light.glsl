/* light.glsl -- Light data structures and uniform block.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#define LIGHT_DIR   0
#define LIGHT_SPOT  1
#define LIGHT_OMNI  2

struct Light {
    mat4 viewProj;
    vec3 color;
    vec3 position;
    vec3 direction;
    float specular;
    float energy;
    float range;
    float near;
    float far;
    float falloff;
    float innerCutOff;
    float outerCutOff;
    float shadowSoftness;
    float shadowOpacity;
    float shadowDepthBias;
    float shadowSlopeBias;
    int shadowLayer;        //< less than zero if no shadows
    int type;
};

#ifdef NUM_FORWARD_LIGHTS
layout(std140) uniform LightArrayBlock {
    Light uLights[NUM_FORWARD_LIGHTS];
    int uNumLights;
};
#else
layout(std140) uniform LightBlock {
    Light uLight;
};
#endif
