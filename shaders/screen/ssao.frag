/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
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
uniform mat4 uMatInvView;
uniform mat4 uMatProj;
uniform mat4 uMatView;

uniform vec2 uResolution;
uniform float uNear;
uniform float uFar;

uniform float uRadius;
uniform float uBias;
uniform float uIntensity;

/* === Constants === */

const int NOISE_TEXTURE_SIZE = 4;
const int KERNEL_SIZE = 32;

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec3 GetPositionFromDepth(float depth)
{
    vec4 ndcPos = vec4(vTexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    viewPos /= viewPos.w;
    // Changed to keep position in view space for SSAO
    return viewPos.xyz;
}

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // Back to NDC
    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
}

vec3 SampleKernel(int index, int kernelSize)
{
    float texCoord = (float(index) + 0.5) / float(kernelSize); // Added + 0.5 for better sampling
    return texture(uTexKernel, texCoord).rgb;
}

/* === Main program === */

void main()
{
    // Get current depth and view-space position
    float depth = texture(uTexDepth, vTexCoord).r;
    vec3 position = GetPositionFromDepth(depth);
    
    // Get and decode current normal, then transform to view space
    vec3 normal = M_DecodeOctahedral(texture(uTexNormal, vTexCoord).rg);
    normal = normalize(mat3(uMatView) * normal);
    
    // Calculate screen-space noise scale
    vec2 noiseScale = uResolution / float(NOISE_TEXTURE_SIZE);
    vec3 randomVec = normalize(texture(uTexNoise, vTexCoord * noiseScale).xyz * 2.0 - 1.0);
    
    // Generate tangent space basis
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < KERNEL_SIZE; i++)
    {
        // Get sample position
        vec3 samplePos = TBN * SampleKernel(i, KERNEL_SIZE);
        
        // Scale sample based on distance from center
        float scale = float(i) / float(KERNEL_SIZE);
        scale = mix(0.1, 1.0, scale * scale);
        samplePos = position + samplePos * uRadius * scale;
        
        // Project sample position to screen space
        vec4 offset = uMatProj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        
        // Ensure sample is within screen bounds
        if (offset.x >= 0.0 && offset.x <= 1.0 && 
            offset.y >= 0.0 && offset.y <= 1.0) {
            
            // Get sample depth and position
            float sampleDepth = texture(uTexDepth, offset.xy).r;
            vec3 sampleViewPos = GetPositionFromDepth(sampleDepth);
            
            // Range and depth checks
            float rangeCheck = 1.0 - smoothstep(0.0, uRadius, abs(position.z - sampleViewPos.z));
            occlusion += (sampleViewPos.z >= samplePos.z + uBias) ? rangeCheck : 0.0;
        }
    }
    
    FragOcclusion = 1.0 - ((occlusion / float(KERNEL_SIZE)) * uIntensity);
}
