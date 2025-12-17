/* ssao.frag -- Fragment shader for calculating ambient occlusion in screen space
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexDepth;
uniform sampler2D uTexNormal;
uniform sampler1D uTexKernel;
uniform sampler2D uTexNoise;

uniform mat4 uMatInvProj;
uniform mat4 uMatProj;
uniform mat4 uMatView;

uniform float uRadius;
uniform float uBias;
uniform float uIntensity;

/* === Constants === */

const int NOISE_TEXTURE_SIZE = 4;
const int KERNEL_SIZE = 32;

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec3 DepthToViewPosition(float depth)
{
    vec4 ndcPos = vec4(vTexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

vec2 ViewPositionToScreenUV(vec3 viewPos)
{
    vec4 clipPos = uMatProj * vec4(viewPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    return ndc.xy * 0.5 + 0.5;
}

vec3 SampleKernel(int index, int kernelSize)
{
    float texCoord = (float(index) + 0.5) / float(kernelSize);
    return texture(uTexKernel, texCoord).rgb;
}

/* === Main program === */

void main()
{
    /* --- Get current depth and view-space position --- */

    float depth = texture(uTexDepth, vTexCoord).r;
    if (depth >= 1.0 - 1e-5) discard;

    vec3 position = DepthToViewPosition(depth);

    /* --- Get and decode current normal, then transform to view space --- */

    vec3 normal = M_DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);
    normal = normalize(mat3(uMatView) * normal);

    /* --- Calculate screen-space noise scale --- */

    vec2 noiseCoord = gl_FragCoord.xy / float(NOISE_TEXTURE_SIZE);
    vec3 randomVec = normalize(texture(uTexNoise, noiseCoord).xyz * 2.0 - 1.0);

    /* --- Generate tangent space basis --- */

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    /* --- Accumulates occlusion by sampling the neighborhood randomly --- */

    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        /* --- Get sample position --- */

        vec3 samplePos = SampleKernel(i, KERNEL_SIZE);
        samplePos = position + (TBN * samplePos) * uRadius;

        /* --- Project sample position to screen space --- */

        vec2 offset = ViewPositionToScreenUV(samplePos);

        /* --- Ensure sample is within screen bounds --- */

        if (all(greaterThanEqual(offset, vec2(0.0))) && all(lessThanEqual(offset, vec2(1.0))))
        {
            // Get sample depth and position
            float sampleDepth = texture(uTexDepth, offset).r;
            vec3 sampleViewPos = DepthToViewPosition(sampleDepth);

            // Range and depth checks
            float rangeCheck = 1.0 - smoothstep(0.0, uRadius, abs(position.z - sampleViewPos.z));
            occlusion += (sampleViewPos.z >= samplePos.z + uBias) ? rangeCheck : 0.0;
        }
    }

    FragOcclusion = 1.0 - (occlusion / float(KERNEL_SIZE)) * uIntensity;
}
