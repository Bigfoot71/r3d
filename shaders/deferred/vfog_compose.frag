/* vfog_compose.frag -- Fragment shader for volumetric fog radiance composition
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uRadianceTex;
uniform sampler2D uDepthTex;

/* === Fragments === */

out vec4 FragRadiance;

/* === Helper Functions === */

vec4 Upsample(sampler2D source, vec2 texCoord, ivec2 size, float refDepth, float depthSharpness)
{
    ivec2 maxCoord = size - ivec2(1);

    vec2 pixLow = texCoord * vec2(size) - 0.5;
    vec2 base = floor(pixLow);
    vec2 f = pixLow - base;

    ivec2 p00 = clamp(ivec2(base),               ivec2(0), maxCoord);
    ivec2 p10 = clamp(ivec2(base) + ivec2(1, 0), ivec2(0), maxCoord);
    ivec2 p01 = clamp(ivec2(base) + ivec2(0, 1), ivec2(0), maxCoord);
    ivec2 p11 = clamp(ivec2(base) + ivec2(1, 1), ivec2(0), maxCoord);

    vec4 w = vec4(
        (1.0 - f.x) * (1.0 - f.y),
        f.x         * (1.0 - f.y),
        (1.0 - f.x) * f.y,
        f.x         * f.y
    );

    vec4 d = vec4(
        texelFetch(uDepthTex, p00, 1).r,
        texelFetch(uDepthTex, p10, 1).r,
        texelFetch(uDepthTex, p01, 1).r,
        texelFetch(uDepthTex, p11, 1).r
    );
    w *= exp(-abs(d - vec4(refDepth)) * depthSharpness);

    float invWSum = 1.0 / max(w.x + w.y + w.z + w.w, 1e-5);

    return (
        texelFetch(source, p00, 0) * w.x +
        texelFetch(source, p10, 0) * w.y +
        texelFetch(source, p01, 0) * w.z +
        texelFetch(source, p11, 0) * w.w
    ) * invWSum;
}

vec4 SampleRotated(sampler2D source, vec2 texCoord)
{
    ivec2 size = textureSize(source, 0);
    vec2 t = 1.0 / vec2(size);

    // Fetch reference depth and sharpness once for all 4 upsample calls
    float refDepth = texelFetch(uDepthTex, ivec2(gl_FragCoord.xy), 0).r;
    float depthSharpness = 1.0 / max(refDepth * 0.1, 0.05);

    // Rotated grid offsets (26.57°) to break up the half-res 2x2 block pattern
    vec4 c0 = Upsample(source, texCoord + vec2( t.x,        t.y * 0.5), size, refDepth, depthSharpness);
    vec4 c1 = Upsample(source, texCoord + vec2(-t.x * 0.5,  t.y      ), size, refDepth, depthSharpness);
    vec4 c2 = Upsample(source, texCoord + vec2(-t.x,       -t.y * 0.5), size, refDepth, depthSharpness);
    vec4 c3 = Upsample(source, texCoord + vec2( t.x * 0.5, -t.y      ), size, refDepth, depthSharpness);

    return (c0 + c1 + c2 + c3) * 0.25;
}

/* === Main Function === */

void main()
{
    FragRadiance = SampleRotated(uRadianceTex, vTexCoord);
}
