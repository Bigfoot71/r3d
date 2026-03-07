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

uniform int uSampleCount;
uniform int uMaxRaySteps;
uniform float uStepSize;
uniform float uThickness;
uniform float uMaxDistance;
uniform float uFadeStart;
uniform float uFadeEnd;

/* === Constants === */

const uint TILE_LOG2          = 2u; // 1u
const uint TILE_SIZE          = 1u << TILE_LOG2;
const uint TILE_MASK          = TILE_SIZE - 1u;
const uint PIXELS_PER_TILE    = TILE_SIZE * TILE_SIZE; // 16
const float F_PIXELS_PER_TILE = float(PIXELS_PER_TILE);

/* === Fragments === */

out vec4 FragColor;

/* === Helper Functions === */

vec2 TileCranleyPatterson(uvec2 cell)
{
    uint a = cell.x * 0x9E3779B9u ^ cell.y * 0x85EBCA6Bu;
    uint b = cell.y * 0xC2B2AE35u ^ cell.x * 0xBF58476Du;
    a ^= a >> 16u;
    b ^= b >> 16u;
    return vec2(a, b) * (1.0 / 4294967296.0);
}

vec3 FibonacciHemisphere(float fidx, float invTotal, vec2 cpOffset)
{
    float u = fract((fidx + 0.5) * invTotal + cpOffset.x);
    float phi = fract(fidx * M_PHI_FRAC + cpOffset.y) * M_TAU;

    float cosT = sqrt(1.0 - u);
    float sinT = sqrt(u);

    float cosPhi = cos(phi);
    float sinPhi = sin(phi);

    return vec3(cosPhi * sinT, sinPhi * sinT, cosT);
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

    uint tileIdx = (uint(pix.x) & TILE_MASK) | ((uint(pix.y) & TILE_MASK) << TILE_LOG2);
    float fidx = float(tileIdx);

    uvec2 cell = uvec2(pix) >> TILE_LOG2;
    vec2 cpOffset = TileCranelyPatterson(cell);

    uint S = uint(max(uSampleCount, 1));
    float invTotal = 1.0 / float(PIXELS_PER_TILE * S);
    float invS = 1.0 / float(S);

    vec3 gi = vec3(0.0);

    for (uint s = 0u; s < S; s++) {
        vec3 localDir = FibonacciHemisphere(fidx, invTotal, cpOffset);
        gi += TraceRay(Pvs, TBN * localDir);
        fidx += F_PIXELS_PER_TILE;
    }

    float fade = smoothstep(uFadeEnd, uFadeStart, depth);
    FragColor  = vec4(gi * invS * fade, 1.0);
}
