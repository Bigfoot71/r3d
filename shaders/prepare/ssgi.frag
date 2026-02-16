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

// 4x4 interleaving tile (must be power of two)
const uint INTERLEAVE_TILE_LOG2 = 2u;                           // 2 => 4x4
const uint INTERLEAVE_TILE_SIZE = 1u << INTERLEAVE_TILE_LOG2;   // 4
const uint INTERLEAVE_TILE_MASK = INTERLEAVE_TILE_SIZE - 1u;    // 3

// Hemisphere direction set (rings x azimuths)
const uint AZIM_COUNT = 16u;                    // power of two => cheap mod via &
const uint RING_COUNT = 4u;                     // power of two => cheap mod via &
const uint AZIM_MASK  = AZIM_COUNT - 1u;
const uint RING_MASK  = RING_COUNT - 1u;

// Steps must be coprime with counts (to cycle through)
const uint AZIM_STEP = 5u; // coprime with 16
const uint RING_STEP = 3u; // coprime with 4

// Ring elevations (cos(theta)) from tight -> wide
const float RING_COS[RING_COUNT] = float[](
    0.98, 0.88, 0.74, 0.60
);

// Rotation lattice: pick BITS, set ROT_PHASES = 2 * (1<<BITS)
const uint ROT_BITS   = 7u;                         // 6 or 7 are betters
const uint ROT_PHASES = 256u;                       // = 2*(1<<ROT_BITS)
const float ROT_INV   = 1.0 / float(ROT_PHASES);

// Rotation step across samples (odd/coprime with ROT_PHASES if po2)
const uint ROT_K = 73u; // good for 256
const float ROT_STEP = M_TAU * ROT_INV * float(ROT_K);

/* === Output === */

out vec4 FragColor;

/* === Helper Functions === */

uint LatticeBits(uvec2 p)
{
    // rank-1 lattice in uint space
    uint n = p.x * 0x9E3779B9u + p.y * 0xBB67AE85u;
    return n >> (32u - ROT_BITS); // 0..(2^ROT_BITS - 1)
}

vec3 DirFromAzRing(uint az, uint ring)
{
    float phi = (float(az) * (M_TAU / float(AZIM_COUNT)));

    float cosT = RING_COS[ring];
    float sinT = sqrt(max(0.0, 1.0 - cosT * cosT));

    return vec3(
        cos(phi) * sinT,
        sin(phi) * sinT,
        cosT
    );
}

vec3 RotateAroundZ(vec3 v, float a)
{
    float c = cos(a), s = sin(a);
    return vec3(
        c * v.x - s * v.y,
        s * v.x + c * v.y,
        v.z
    );
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
    float depth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    if (depth >= uFadeEnd) {
        FragColor = vec4(0.0);
        return;
    }

    ivec2 pix = ivec2(gl_FragCoord.xy);

    vec3 Nvs = V_GetViewNormal(uNormalTex, pix);
    vec3 Pvs = V_GetViewPosition(vTexCoord, depth);
    mat3 TBN = M_OrthonormalBasis(Nvs);

    // 4x4 interleave index: 0..15, perfectly stable
    uint tileIndex = (uint(pix.x) & INTERLEAVE_TILE_MASK)
                   | ((uint(pix.y) & INTERLEAVE_TILE_MASK) << INTERLEAVE_TILE_LOG2);

    // Tile coordinate (one value per 4x4 block) for the lattice rotation
    uvec2 tileCoord = uvec2(pix) >> INTERLEAVE_TILE_LOG2;

    // Base az/ring: symmetric mapping from tileIndex
    uint baseAz = tileIndex & AZIM_MASK;    // 0..15
    uint baseRing = tileIndex & RING_MASK;  // 0..3

    // Rotation base: (h + 0.5) * 2pi/ROT_PHASES
    uint h = LatticeBits(tileCoord);
    float rotBase = (float(h) + 0.5) * (M_TAU * ROT_INV);

    vec3 gi = vec3(0.0);

    for (int i = 0; i < uSampleCount; i++)
    {
        uint s = uint(i);

        uint az = (baseAz + s * AZIM_STEP) & AZIM_MASK;
        uint ring = (baseRing + s * RING_STEP) & RING_MASK;

        vec3 dirLocal = DirFromAzRing(az, ring);
        dirLocal = RotateAroundZ(dirLocal, rotBase + float(i) * ROT_STEP);

        gi += TraceRay(Pvs, TBN * dirLocal);
    }

    float fade = smoothstep(uFadeEnd, uFadeStart, depth);
    FragColor = vec4(gi * (1.0 / float(uSampleCount)) * fade, 1.0);
}
