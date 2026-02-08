/* ssao.frag -- Scalable Ambient Occlusion fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Adapted from the method proposed by Morgan McGuire et al. in "Scalable Ambient Obscurance"
// SEE SAO: https://research.nvidia.com/sites/default/files/pubs/2012-06_Scalable-Ambient-Obscurance/McGuire12SAO.pdf

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

uniform int uSampleCount;
uniform float uRadius;
uniform float uBias;
uniform float uIntensity;
uniform float uPower;

/* === Constants === */

// Number of spiral turns for each sample count, ensuring coprime relationship.
// Each entry ROTATIONS[i] is chosen such that GCD(i+1, ROTATIONS[i]) = 1,
// preventing sample alignment artifacts in the spiral pattern.
// Indexed by (sampleCount - 1). Supports 1 to 98 samples.
const int ROTATIONS[98] = int[98](
    1, 1, 2, 3, 2, 5, 2, 3, 2,
    3, 3, 5, 5, 3, 4, 7, 5, 5, 7,
    9, 8, 5, 5, 7, 7, 7, 8, 5, 8,
    11, 12, 7, 10, 13, 8, 11, 8, 7, 14,
    11, 11, 13, 12, 13, 19, 17, 13, 11, 18,
    19, 11, 11, 14, 17, 21, 15, 16, 17, 18,
    13, 17, 11, 17, 19, 18, 25, 18, 19, 19,
    29, 21, 19, 27, 31, 29, 21, 18, 17, 29,
    31, 31, 23, 18, 25, 26, 25, 23, 19, 34,
    19, 27, 21, 25, 39, 29, 17, 21, 27
);

/* === Fragments === */

out float FragOcclusion;

/* === Helper functions === */

vec2 TapLocation(int i, float numSpiralTurns, float spin, out float rNorm)
{
    float alpha = (float(i) + 0.5) / float(uSampleCount);
    float angle = alpha * (numSpiralTurns * M_TAU) + spin;

    rNorm = alpha;
    return vec2(cos(angle), sin(angle));
}

/* === Main program === */

void main()
{
    FragOcclusion = 1.0;

    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    float depth = texture(uDepthTex, vTexCoord).r;
    if (depth >= uView.far) return;

    vec3 position = V_GetViewPosition(vTexCoord, depth);
    vec3 normal = V_GetViewNormal(uNormalTex, pixelCoord);

    float projScale = abs(uView.proj[1][1]) * textureSize(uDepthTex, 0).y * 0.5;
    float ssRadius = projScale * uRadius / max(depth, 0.1);
    float radiusSq = uRadius * uRadius;

    // Here we use an IGN instead of the hash from the HPG12 AlchemyAO paper.
    // The result is much more pleasing and blurs much better.

    float spin = M_TAU * M_HashIGN(gl_FragCoord.xy);
    int numSpiralTurns = ROTATIONS[clamp(uSampleCount - 1, 0, 97)];

    float aoSum = 0.0;
    for (int i = 0; i < uSampleCount; ++i)
    {
        float rNorm;
        vec2 unitDir = TapLocation(i, float(numSpiralTurns), spin, rNorm);
        ivec2 pixelOffset = pixelCoord + ivec2(unitDir * ssRadius * rNorm);

        vec3 samplePos = V_GetViewPosition(uDepthTex, pixelOffset);
        vec3 v = samplePos - position;

        float vv = dot(v, v);
        float vn = dot(v, normal);

        const float epsilon = 0.02;
        float f = max(radiusSq - vv, 0.0);
        aoSum += f * f * f * max((vn - uBias) / (epsilon + vv), 0.0);
    }

    float temp = radiusSq * uRadius;
    aoSum /= (temp * temp);
    
    float A = max(0.0, 1.0 - aoSum * uIntensity * (4.0 / float(uSampleCount)));

    // 1-pixel bilateral filter using derivatives (almost free)
	if (abs(dFdx(depth)) < 0.2) A -= dFdx(A) * (float(pixelCoord.x & 1) - 0.5);
    if (abs(dFdy(depth)) < 0.2) A -= dFdy(A) * (float(pixelCoord.y & 1) - 0.5);

    // SAO tends to over-darken near surfaces; smoothly fade it out
    A = mix(A, 1.0, 1.0 - clamp(0.5 * depth, 0.0, 1.0));

    FragOcclusion = pow(A, uPower);
}
