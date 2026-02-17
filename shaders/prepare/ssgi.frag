/* ssgi.frag -- Screen Space Global Illumination fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uHistoryTex;
uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;
uniform sampler1D uLutTex;

uniform int uSampleCount;
uniform int uMaxRaySteps;
uniform float uStepSize;
uniform float uThickness;
uniform float uMaxDistance;
uniform float uFadeStart;
uniform float uFadeEnd;

/* === Constants === */

// These constants are related to the LUT, do not modify them.

const uint TILE_LOG2 = 2u;
const uint TILE_SIZE = 1u << TILE_LOG2;
const uint TILE_MASK = TILE_SIZE - 1u;

const uint AZIM_COUNT = 16u;
const uint RING_COUNT = 4u;
const uint AZIM_STEP = 5u;
const uint RING_STEP = 3u;

const uint ROT_PHASES = 64u;
const uint ROT_BITS = 8u;

const uint AZIM_MASK = AZIM_COUNT - 1u;
const uint RING_MASK = RING_COUNT - 1u;
const uint ROT_MASK  = ROT_PHASES - 1u;

const uint LUT_RING_STRIDE  = AZIM_COUNT;
const uint LUT_PHASE_STRIDE = AZIM_COUNT * RING_COUNT;

/* === Fragments === */

out vec4 FragColor;

/* === Helper Functions === */

uint TileCellHash(uvec2 cell, uint bits)
{
    uint n = cell.x * 0x9E3779B9u + cell.y * 0xBB67AE85u;
    return n >> (32u - bits);
}

vec3 DirFromLUT(uint phase, uint ring, uint az)
{
    uint index = phase * LUT_PHASE_STRIDE + ring * LUT_RING_STRIDE + az;
    return texelFetch(uLutTex, int(index), 0).xyz;
}

vec3 TraceRay(vec3 startViewPos, vec3 dirVS)
{
    vec3 stepVS = dirVS * uStepSize;
    float stepLenSq = dot(stepVS, stepVS);
    float maxLenSq = uMaxDistance * uMaxDistance;

    vec3 posVS = startViewPos + stepVS;
    float distSq = stepLenSq;

    vec2 hitUV = vec2(0.0);
    bool hit = false;

    for (int i = 1; i < uMaxRaySteps; i++)
    {
        if (distSq > maxLenSq) break;

        vec2 uv = V_ViewToScreen(posVS);
        if (V_OffScreen(uv)) break;

        float sceneZ = -textureLod(uDepthTex, uv, 0).r;
        float dz = sceneZ - posVS.z;

        if (dz > 0.0 && dz < uThickness) {
            hitUV = uv;
            hit = true;
            break;
        }

        posVS += stepVS;
        distSq += stepLenSq;
    }

    if (!hit) return vec3(0.0);

    vec3 hist = textureLod(uHistoryTex, hitUV, 0).rgb;
    vec3 diff = textureLod(uDiffuseTex, hitUV, 0).rgb;

    float distFade = 1.0 - smoothstep(0.0, uMaxDistance, sqrt(distSq));
    return (diff + hist) * distFade;
}

/* === Main Program === */

void main()
{
    ivec2 pix = ivec2(gl_FragCoord.xy);
    float depth = texelFetch(uDepthTex, pix, 0).r;
    if (depth >= uFadeEnd) { FragColor = vec4(0.0); return; }

    vec3 Nvs = V_GetViewNormal(uNormalTex, pix);
    vec3 Pvs = V_GetViewPosition(vTexCoord, depth);
    mat3 TBN = M_OrthonormalBasis(Nvs);

    uint idx = (uint(pix.x) & TILE_MASK) | ((uint(pix.y) & TILE_MASK) << TILE_LOG2);

    uint baseAz = idx & AZIM_MASK;
    uint baseRing = idx & RING_MASK;

    uvec2 cell = uvec2(pix) >> TILE_LOG2;
    uint h = TileCellHash(cell, ROT_BITS) & ROT_MASK;

    vec3 gi = vec3(0.0);

    for (int i = 0; i < uSampleCount; i++)
    {
        uint s = uint(i);
        uint az = (baseAz + s * AZIM_STEP) & AZIM_MASK;
        uint ring = (baseRing + s * RING_STEP) & RING_MASK;
        uint phase = (h + s) & ROT_MASK;

        vec3 dirLocal = DirFromLUT(phase, ring, az);
        gi += TraceRay(Pvs, TBN * dirLocal);
    }

    float fade = smoothstep(uFadeEnd, uFadeStart, depth);
    FragColor = vec4(gi * (1.0 / float(uSampleCount)) * fade, 1.0);
}
