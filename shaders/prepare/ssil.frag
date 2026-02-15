/* ssil.frag -- Screen space indirect lihgting with visibility mask
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Original version adapted from "Screen Space Indirect Lighting with Visibility Bitmask" by Olivier Therrien, et al.
// https://cdrinmatane.github.io/posts/cgspotlight-slides/

// This is version is adapted from the Cyberealty's blog post (big thanks for for this discovery!):
// https://cybereality.com/screen-space-indirect-lighting-with-visibility-bitmask-improvement-to-gtao-ssao-real-time-ambient-occlusion-algorithm-glsl-shader-implementation/

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings == */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDiffuseTex;
uniform sampler2D uHistoryTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform float uSampleCount;
uniform float uSampleRadius;
uniform float uSliceCount;
uniform float uThickness;

/* === Fragments === */

layout(location = 0) out vec4 FragColor;

/* === Helper Functions === */

uint BitCount(uint value)
{
    // https://graphics.stanford.edu/%7Eseander/bithacks.html
    value = value - ((value >> 1u) & 0x55555555u);
    value = (value & 0x33333333u) + ((value >> 2u) & 0x33333333u);
    return ((value + (value >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

const uint SECTOR_COUNT = 32u;
uint UpdateSectors(float minHorizon, float maxHorizon)
{
    uint b = uint(ceil((maxHorizon - minHorizon) * float(SECTOR_COUNT))); // Horizon angle
    uint a = uint(minHorizon * float(SECTOR_COUNT)); // Start bit
    uint mask = b > 0u ? ((1u << b) - 1u) : 0u;
    return mask << a;
}

vec3 SampleLight(vec2 texCoord)
{
    vec3 indirect = texture(uHistoryTex, texCoord).rgb;
    vec3 direct = texture(uDiffuseTex, texCoord).rgb;
    float edgeMask = float(!V_OffScreen(texCoord));
    return (direct + indirect) * edgeMask;
}

float FastAcos(float x)
{
    // Lagrange polynomial approximation, maximum error around 0.18 rad
    return (-0.69813170079773212 * x * x - 0.87266462599716477) * x + 1.5707963267948966;
}

vec2 FastAcos(vec2 x)
{
    // Lagrange polynomial approximation, maximum error around 0.18 rad
    return (-0.69813170079773212 * x * x - 0.87266462599716477) * x + 1.5707963267948966;
}

/* === Program === */

void main()
{
    vec3 position = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec3 normal = V_GetViewNormal(uNormalTex, ivec2(gl_FragCoord.xy));
    vec3 camera = normalize(-position);

    vec2 screenSize = vec2(textureSize(uDiffuseTex, 0));
    vec2 aspect = screenSize.yx / screenSize.x;
    float jitter = M_HashIGN(gl_FragCoord.xy);

    float sliceRotation = M_TAU / uSliceCount;
    float jitterRotation = M_TAU * jitter;

    float sampleScale = uSampleRadius * uView.proj[0][0] / -position.z;
    float sampleOffset = 0.01;

    // Indirect + visiblity accumulator
    vec4 indirect = vec4(0.0);

    // Iterate over angular slices around the hemisphere
    for (int slice = 0; slice < uSliceCount; slice++)
    {
        float phi = jitterRotation + sliceRotation * float(slice);
        vec2 omega = vec2(cos(phi), sin(phi));

        vec3 direction = vec3(omega.x, omega.y, 0.0);
        vec3 orthoDirection = direction - dot(direction, camera) * camera;  // Project onto plane perpendicular to view
        vec3 axis = cross(direction, camera);

        // Project surface normal onto the sampling plane
        vec3 projNormal = normal - axis * dot(normal, axis);
        float projLength = length(projNormal);

        // Compute signed angle offset from camera direction
        float signN = sign(dot(orthoDirection, projNormal));
        float cosN = clamp(dot(projNormal, camera) / projLength, 0.0, 1.0);
        float n = signN * FastAcos(cosN);

        // Occlusion accumulator for this slice
        uint occlusion = 0u;

        // Trace radial samples outward along this slice
        for (int currentSample = 0; currentSample < uSampleCount; currentSample++)
        {
            float sampleStep = (float(currentSample) + jitter) / uSampleCount + sampleOffset;
            vec2 sampleUV = vTexCoord - sampleStep * sampleScale * omega * aspect;

            vec3 samplePosition = V_GetViewPosition(uDepthTex, sampleUV);
            vec3 sampleNormal = V_GetViewNormal(uNormalTex, sampleUV);
            vec3 sampleLight = SampleLight(sampleUV);

            vec3 sampleDistance = samplePosition - position;
            vec3 sampleHorizon = sampleDistance / length(sampleDistance);

            // Compute horizon angles: front (actual position) and back (with thickness offset)
            vec3 backSample = normalize(sampleDistance - camera * uThickness);
            vec2 frontBackHorizon = vec2(dot(sampleHorizon, camera), dot(backSample, camera));
            frontBackHorizon = FastAcos(clamp(frontBackHorizon, -1.0, 1.0));
            frontBackHorizon = clamp((frontBackHorizon + n + M_HPI) / M_PI, 0.0, 1.0);  // Normalize to [0,1] relative to surface

            // Mark visible sectors for this sample
            uint sampleBitmask = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y);

            // Weight lighting by newly visible sectors (those not already occluded) and by geometric terms (surface angles)
            float newlyVisible = 1.0 - float(BitCount(sampleBitmask & ~occlusion)) / float(SECTOR_COUNT);
            indirect.rgb += sampleLight * newlyVisible * clamp(dot(normal, sampleHorizon), 0.0, 1.0) * clamp(dot(sampleNormal, -sampleHorizon), 0.0, 1.0);

            // Accumulate occlusion across samples
            occlusion |= sampleBitmask;
        }

        indirect.a += 1.0 - float(BitCount(occlusion)) / float(SECTOR_COUNT);
    }

    FragColor = indirect / uSliceCount;
}
