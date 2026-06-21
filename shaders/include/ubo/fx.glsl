/* fx.glsl -- FX data structures and uniform block.
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

struct FX_Ssao {
    int sampleCount;
    float intensity;
    float power;
    float maxRadius;
    float radius;
    float bias;
};

struct FX_Ssil {
    int sampleCount;
    float giIntensity;
    float aoIntensity;
    float aoPower;
    float maxRadius;
    float radius;
    float bias;
};

struct FX_Ssgi {
    int sliceCount;
    float edgeFade;
    float distanceFalloff;
    float normalRejection;
    float intensity;
};

struct FX_Ssr {
    int maxRaySteps;
    int binarySteps;
    float stepSize;
    float thickness;
    float maxDistance;
    float edgeFade;
};

struct FX_Fog {
    vec3 color;
    float start;
    float end;
    float density;
    float skyAffect;
    int mode;
};

struct FX_VFog {
    vec3 scatteringColor;
    vec3 emissionColor;
    float scatteringDensity;
    float absortionDensity;
    float anisotropy;
    float emissionEnergy;
    float skyAffect;
    float length;
    float stepSize;
};

struct FX_Dof {
    float focusPoint;
    float focusScale;
    float nearScale;
    float maxBlurSize;
    int mode;
};

struct FX_Bloom {
    vec4 prefilter;
    float intensity;
    float filterRadius;
    int mode;
};

struct FX_Tonemap {
    float exposure;
    float white;
    int mode;
};

struct FX_Bcs {
    float brightness;
    float contrast;
    float saturation;
};

layout(std140) uniform FxBlock {
    FX_Ssao uSsao;
    FX_Ssil uSsil;
    FX_Ssgi uSsgi;
    FX_Ssr uSsr;
    FX_Fog uFog;
    FX_VFog uVFog;
    FX_Dof uDof;
    FX_Bloom uBloom;
    FX_Tonemap uTonemap;
    FX_Bcs uBcs;
};
