/* ssao.frag -- Fragment shader for calculating ambient occlusion in screen space
 *
 * Copyright (c) 2025 Le Juez Victor
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

uniform sampler2D uTexDepth;
uniform sampler2D uTexNormal;
uniform sampler1D uTexKernel;
uniform sampler2D uTexNoise;

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

vec3 SampleKernel(int index, int kernelSize)
{
    float texCoord = (float(index) + 0.5) / float(kernelSize);
    return texture(uTexKernel, texCoord).rgb;
}

/* === Main program === */

void main()
{
    vec3 position = V_GetViewPosition(uTexDepth, vTexCoord);
    vec3 normal = V_GetViewNormal(uTexNormal, vTexCoord);

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

        vec2 sampleUV = V_ViewToScreen(testPos);
        if (V_OffScreen(sampleUV)) continue;

        vec3 samplePos = V_GetViewPosition(uTexDepth, sampleUV);
        float rangeCheck = 1.0 - smoothstep(0.0, uRadius, abs(position.z - samplePos.z));

        occlusion += (samplePos.z >= testPos.z + uBias) ? rangeCheck : 0.0;
        valid += 1.0;
    }

    occlusion = 1.0 - (occlusion / max(valid, 1.0));
    FragOcclusion = pow(occlusion, uPower) * uIntensity;
}
