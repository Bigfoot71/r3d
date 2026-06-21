/* ssil.frag -- Screen Space Directional Occlusion (SSDO)
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

// Based on Screen Space Directional Occlusion by Tobias Ritschel et al.
// SEE: https://web.archive.org/web/20140725130649/https://people.mpi-inf.mpg.de/~ritschel/Papers/SSDO.pdf

// And on Scalable Ambient Obscurance method by Morgan McGuire et al.
// SEE: https://research.nvidia.com/publication/2012-06_scalable-ambient-obscurance

#version 330 core

/* === Includes === */

#include <wrap/view.glsl>
#include <lib/math.glsl>
#include <ubo/fx.glsl>

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDiffuseTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

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

out vec4 FragColor;

/* === Helper functions === */

vec2 TapLocation(int i, float numSpiralTurns, float spin, out float rNorm)
{
    float alpha = (float(i) + 0.5) / float(uSsil.sampleCount);
    float angle = alpha * (numSpiralTurns * M_TAU) + spin;

    rNorm = alpha;
    return vec2(cos(angle), sin(angle));
}

/* === Main program === */

void main()
{
    FragColor = vec4(vec3(0.0), 1.0);

    ivec2 pixelCoord = ivec2(gl_FragCoord.xy);
    float depth = texture(uDepthTex, vTexCoord).r;
    if (depth >= uView.far) return;

    vec3 position = V_GetViewPosition(vTexCoord, depth);
    vec3 normal = V_GetViewNormal(uNormalTex, pixelCoord);

    float projScale = abs(uView.proj[1][1]) * textureSize(uDepthTex, 0).y * 0.5;
    float ssRadiusRaw = projScale * uSsil.radius / max(depth, 0.1);
    float ssRadius = min(ssRadiusRaw, uSsil.ssMaxRadius);

    float radiusScale = ssRadius / max(ssRadiusRaw, 1e-4);
    float radiusSq = uSsil.radius * uSsil.radius;

    // Here we use an IGN instead of the hash from the HPG12 AlchemyAO paper.
    // The result is much more pleasing and blurs much better.

    float spin = M_TAU * M_HashR2(gl_FragCoord.xy);
    int numSpiralTurns = ROTATIONS[clamp(uSsil.sampleCount - 1, 0, 97)];

    float aoSum = 0.0;
    vec3 giSum = vec3(0.0);

    for (int i = 0; i < uSsil.sampleCount; ++i)
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
        float w = f * f * f * max((vn - uSsil.bias) / (epsilon + vv), 0.0);

        aoSum += w;
        giSum += texelFetch(uDiffuseTex, pixelOffset, 0).rgb * w;
    }

    float temp = radiusSq * uSsil.radius;
    float normFactor = 1.0 / (temp * temp);

    aoSum *= normFactor;
    giSum *= normFactor;

    // Attenuate intensity proportionally when ssRadius was clamped, preventing over-darkening at close range
    float ao = max(0.0, 1.0 - aoSum * uSsil.aoIntensity * (2.0 / float(uSsil.sampleCount)) * radiusScale);
    vec3 gi = giSum * (4.0 / float(uSsil.sampleCount)) * radiusScale;

    // 1-pixel bilateral filter using derivatives (almost free)
    if (abs(dFdx(depth)) < 0.2) {
        float dx = float(pixelCoord.x & 1) - 0.5;
        ao -= dFdx(ao) * dx;
        gi -= dFdx(gi) * dx;
    }
    if (abs(dFdy(depth)) < 0.2) {
        float dy = float(pixelCoord.y & 1) - 0.5;
        ao -= dFdy(ao) * dy;
        gi -= dFdy(gi) * dy;
    }

    FragColor = vec4(gi, ao);
}
