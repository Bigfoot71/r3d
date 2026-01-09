/* ssil.frag -- Screen space indirect lihgting with visibility mask
 *
 * Copyright (c) 2025 Le Juez Victor
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

uniform sampler2D uLightingTex;
uniform sampler2D uHistoryTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform float uSampleCount;
uniform float uSampleRadius;
uniform float uSliceCount;
uniform float uHitThickness;

uniform float uConvergence;
uniform float uAoPower;
uniform float uBounce;
uniform float uEnergy;

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
uint UpdateSectors(float minHorizon, float maxHorizon, uint outBitfield)
{
    // https://cdrinmatane.github.io/posts/ssaovb-code/
    uint startBit = uint(minHorizon * float(SECTOR_COUNT));
    uint horizonAngle = uint(ceil((maxHorizon - minHorizon) * float(SECTOR_COUNT)));
    uint angleBit = horizonAngle > 0u ? uint(0xFFFFFFFFu >> (SECTOR_COUNT - horizonAngle)) : 0u;
    uint currentBitfield = angleBit << startBit;
    return outBitfield | currentBitfield;
}

vec3 SampleLight(vec2 texCoord)
{
    vec3 indirect = texture(uHistoryTex, texCoord).rgb;
    vec3 direct = texture(uLightingTex, texCoord).rgb;
    return direct + uBounce * indirect;
}

/* === Program === */

void main()
{
    uint indirect = 0u;
    uint occlusion = 0u;

    float visibility = 0.0;
    vec3 lighting = vec3(0.0);

    vec3 position = V_GetViewPosition(uDepthTex, vTexCoord);
    vec3 normal = V_GetViewNormal(uNormalTex, vTexCoord);
    vec3 camera = normalize(-position);

    float sliceRotation = M_TAU / (uSliceCount - 1.0);
    float jitterRotation = M_HashIGN(gl_FragCoord.xy) * M_TAU;

    float sampleScale = (-uSampleRadius * uView.proj[0][0]) / position.z;  // World-space to screen-space conversion
    float sampleOffset = 0.01;

    // Iterate over angular slices around the hemisphere
    for (int slice = 0; slice < uSliceCount; slice++)
    {
        float phi = sliceRotation * float(slice) + jitterRotation;
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
        float n = signN * acos(cosN);

        // Trace radial samples outward along this slice
        for (int currentSample = 0; currentSample < uSampleCount; currentSample++)
        {
            float sampleStep = float(currentSample) / uSampleCount + sampleOffset;
            vec2 sampleUV = vTexCoord - sampleStep * sampleScale * omega * uView.aspect;

            vec3 samplePosition = V_GetViewPosition(uDepthTex, sampleUV);
            vec3 sampleNormal = V_GetViewNormal(uNormalTex, sampleUV);
            vec3 sampleLight = SampleLight(sampleUV);

            vec3 sampleDistance = samplePosition - position;
            float sampleLength = length(sampleDistance);
            vec3 sampleHorizon = sampleDistance / sampleLength;

            // Compute horizon angles: front (actual position) and back (with thickness offset)
            // This prevents thin surfaces from fully occluding behind them
            vec2 frontBackHorizon = vec2(0.0);
            frontBackHorizon.x = dot(sampleHorizon, camera);
            frontBackHorizon.y = dot(normalize(sampleDistance - camera * uHitThickness), camera);

            frontBackHorizon = acos(frontBackHorizon);
            frontBackHorizon = clamp((frontBackHorizon + n + M_HPI) / M_PI, 0.0, 1.0);  // Normalize to [0,1] relative to surface

            // Mark visible sectors for this sample
            indirect = UpdateSectors(frontBackHorizon.x, frontBackHorizon.y, 0u);

            // Weight lighting by newly visible sectors (those not already occluded)
            // and by geometric terms (surface angles)
            lighting += (1.0 - float(BitCount(indirect & ~occlusion)) / float(SECTOR_COUNT)) *
                        sampleLight *
                        clamp(dot(normal, sampleHorizon), 0.0, 1.0) *
                        clamp(dot(sampleNormal, -sampleHorizon), 0.0, 1.0);

            // Accumulate occlusion across all samples
            occlusion |= indirect;
        }

        visibility += 1.0 - float(BitCount(occlusion)) / float(SECTOR_COUNT);
    }

    vec4 history = texture(uHistoryTex, vTexCoord);
    visibility /= uSliceCount;
    lighting /= uSliceCount;

    FragColor = vec4(lighting * uEnergy, pow(visibility, uAoPower));
    FragColor.rgb = mix(FragColor.rgb, history.rgb, uConvergence);
}
