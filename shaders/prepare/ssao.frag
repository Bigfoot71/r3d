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
uniform float uPower;

/* === Constants === */

const int NOISE_TEXTURE_SIZE = 4;
const int KERNEL_SIZE = 32;

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec3 GetViewPosition(vec2 texCoord)
{
    float depth = texture(uTexDepth, texCoord).r;
    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

vec3 GetViewNormal(vec2 texCoord)
{
    vec2 encoded = texture(uTexNormal, texCoord).rg;
    vec3 normal = M_DecodeOctahedral(encoded);
    return normalize(mat3(uMatView) * normal);
}

vec2 ViewToScreenUV(vec3 viewPos)
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

bool OffScreen(vec2 texCoord)
{
    return any(lessThan(texCoord, vec2(0.0))) ||
           any(greaterThan(texCoord, vec2(1.0)));
}

/* === Main program === */

void main()
{
    vec3 position = GetViewPosition(vTexCoord);
    vec3 normal = GetViewNormal(vTexCoord);

    vec2 noiseCoord = gl_FragCoord.xy / float(NOISE_TEXTURE_SIZE);
    vec3 randomVec = normalize(texture(uTexNoise, noiseCoord).xyz * 2.0 - 1.0);

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    float valid = 0.0;

    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        vec3 offset = SampleKernel(i, KERNEL_SIZE);
        vec3 testPos  = position + (TBN * offset) * uRadius;

        vec2 sampleUV = ViewToScreenUV(testPos);
        if (OffScreen(sampleUV)) continue;

        vec3 samplePos = GetViewPosition(sampleUV);
        float rangeCheck = 1.0 - smoothstep(0.0, uRadius, abs(position.z - samplePos.z));

        occlusion += (samplePos.z >= testPos.z + uBias) ? rangeCheck : 0.0;
        valid += 1.0;
    }

    occlusion = 1.0 - (occlusion / max(valid, 1.0));
    FragOcclusion = pow(occlusion, uPower) * uIntensity;
}
