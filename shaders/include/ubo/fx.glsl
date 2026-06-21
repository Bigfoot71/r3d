/* fx.glsl -- FX data structures and uniform block.
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

/* === Fog Modes === */

#define FOG_DISABLED        0
#define FOG_LINEAR          1
#define FOG_EXP2            2
#define FOG_EXP             3

/* === Bloom Modes === */

#define BLOOM_MIX           1
#define BLOOM_ADDITIVE      2
#define BLOOM_SCREEN        3

/* === Tonemap Modes === */

#define TONEMAP_LINEAR      0
#define TONEMAP_REINHARD    1
#define TONEMAP_FILMIC      2
#define TONEMAP_ACES        3
#define TONEMAP_AGX         4

/* === Structures === */

struct FX_Ssao {
    int sampleCount;
    float intensity;
    float power;
    float ssMaxRadius;
    float radius;
    float bias;
    bool enabled;
};

struct FX_Ssil {
    int sampleCount;
    float giIntensity;
    float aoIntensity;
    float aoPower;
    float ssMaxRadius;
    float radius;
    float bias;
    bool enabled;
};

struct FX_Ssgi {
    int sliceCount;
    float edgeFade;
    float distanceFalloff;
    float normalRejection;
    float intensity;
    bool enabled;
};

struct FX_Ssr {
    int maxRaySteps;
    int binarySteps;
    float stepSize;
    float thickness;
    float maxDistance;
    float edgeFade;
    bool enabled;
};

struct FX_Fog {
    vec4 color;
    float start;
    float end;
    float density;
    float skyAffect;
    int mode;
};

struct FX_VFog {
    vec4 scatteringColor;
    vec4 emissionColor;
    float scatteringDensity;
    float absortionDensity;
    float anisotropy;
    float emissionEnergy;
    float skyAffect;
    float length;
    float stepSize;
    bool enabled;
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

/* === Uniform Block === */

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
