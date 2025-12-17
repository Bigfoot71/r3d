/* output.frag -- Scene output fragment shader
 *
 * Performs tone mapping, debanding, color adjustments, and
 * converts from linear color space to sRGB.
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Definitions === */

#define TONEMAP_LINEAR 0
#define TONEMAP_REINHARD 1
#define TONEMAP_FILMIC 2
#define TONEMAP_ACES 3
#define TONEMAP_AGX 4

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;        //< Scene color texture
uniform float uTonemapExposure;     //< Tonemap exposure
uniform float uTonemapWhite;        //< Tonemap white point, not used with AGX
uniform float uBrightness;          //< Brightness adjustment
uniform float uContrast;            //< Contrast adjustment
uniform float uSaturation;          //< Saturation adjustment

/* === Fragments === */

out vec4 FragColor;

/* === Tonemap Functions === */

// Based on Reinhard's extended formula, see equation 4 in https://doi.org/cjbgrt
vec3 TonemapReinhard(vec3 color, float pWhite)
{
    float whiteSquared = pWhite * pWhite;
    vec3 whiteSquaredColor = whiteSquared * color;
    // Equivalent to color * (1 + color / whiteSquared) / (1 + color)
    return (whiteSquaredColor + color * color) / (whiteSquaredColor + whiteSquared);
}

vec3 TonemapFilmic(vec3 color, float pWhite)
{
    // exposure bias: input scale (color *= bias, white *= bias) to make the brightness consistent with other tonemappers
    // also useful to scale the input to the range that the tonemapper is designed for (some require very high input values)
    // has no effect on the curve's general shape or visual properties
    const float exposureBias = 2.0;
    const float A = 0.22 * exposureBias * exposureBias; // bias baked into constants for performance
    const float B = 0.30 * exposureBias;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.01;
    const float F = 0.30;

    vec3 colorTonemapped = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float pWhiteTonemapped = ((pWhite * (A * pWhite + C * B) + D * E) / (pWhite * (A * pWhite + B) + D * F)) - E / F;

    return colorTonemapped / pWhiteTonemapped;
}

// Adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// (MIT License).
vec3 TonemapACES(vec3 color, float pWhite)
{
    const float exposureBias = 1.8;
    const float A = 0.0245786;
    const float B = 0.000090537;
    const float C = 0.983729;
    const float D = 0.432951;
    const float E = 0.238081;

    // Exposure bias baked into transform to save shader instructions. Equivalent to `color *= exposureBias`
    const mat3 rgb_to_rrt = mat3(
        vec3(0.59719 * exposureBias, 0.35458 * exposureBias, 0.04823 * exposureBias),
        vec3(0.07600 * exposureBias, 0.90834 * exposureBias, 0.01566 * exposureBias),
        vec3(0.02840 * exposureBias, 0.13383 * exposureBias, 0.83777 * exposureBias)
    );

    const mat3 odt_to_rgb = mat3(
        vec3(1.60475, -0.53108, -0.07367),
        vec3(-0.10208, 1.10813, -0.00605),
        vec3(-0.00327, -0.07276, 1.07602)
    );

    color *= rgb_to_rrt;
    vec3 colorTonemapped = (color * (color + A) - B) / (color * (C * color + D) + E);
    colorTonemapped *= odt_to_rgb;

    pWhite *= exposureBias;
    float pWhiteTonemapped = (pWhite * (pWhite + A) - B) / (pWhite * (C * pWhite + D) + E);

    return colorTonemapped / pWhiteTonemapped;
}

// Polynomial approximation of EaryChow's AgX sigmoid curve.
// x must be within range [0.0, 1.0]
vec3 AgXContrastApprox(vec3 x)
{
    // 6th order polynomial generated from sigmoid curve with 57 sample points
    // Intercept set to 0.0 for performance and correct intersection
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 0.021 * x + 4.0111 * x2 - 25.682 * x2 * x + 70.359 * x4 - 74.778 * x4 * x + 27.069 * x4 * x2;
}

// AgX tonemap implementation based on EaryChow's algorithm used by Blender
// Source: https://github.com/EaryChow/AgX_LUT_Gen/blob/main/AgXBasesRGB.py
vec3 TonemapAgX(vec3 color)
{
    // Combined sRGB to Rec2020 + AgX inset transform
    const mat3 srgbToRec2020AgxInsetMat = mat3(
        0.54490813676363087053, 0.14044005884001287035, 0.088827411851915368603,
        0.37377945959812267119, 0.75410959864013760045, 0.17887712465043811023,
        0.081384976686407536266, 0.10543358536857773485, 0.73224999956948382528
    );

    // Combined inverse AgX outset + Rec2020 to sRGB transform
    const mat3 agxOutsetRec2020ToSrgbMatrix = mat3(
        1.9645509602733325934, -0.29932243390911083839, -0.16436833806080403409,
        -0.85585845117807513559, 1.3264510741502356555, -0.23822464068860595117,
        -0.10886710826831608324, -0.027084020983874825605, 1.402665347143271889
    );

    // EV range constants (LOG2_MIN = -10.0, LOG2_MAX = +6.5, MIDDLE_GRAY = 0.18)
    const float minEV = -12.4739311883324;  // log2(pow(2, LOG2_MIN) * MIDDLE_GRAY)
    const float maxEV = 4.02606881166759;   // log2(pow(2, LOG2_MAX) * MIDDLE_GRAY)

    // Prevent negative values to avoid darker/oversaturated results after matrix transform
    // Small epsilon (2e-10) prevents log2(0.0) while maintaining minimal error
    color = max(color, 2e-10);

    // Transform to Rec2020 and apply inset matrix
    color = srgbToRec2020AgxInsetMat * color;

    // Log2 encoding and normalization to [0,1] range
    color = clamp(log2(color), minEV, maxEV);
    color = (color - minEV) / (maxEV - minEV);

    // Apply sigmoid contrast curve
    color = AgXContrastApprox(color);

    // Convert back to linear (gamma 2.4)
    color = pow(color, vec3(2.4));

    // Apply outset matrix and return to sRGB
    color = agxOutsetRec2020ToSrgbMatrix * color;

    // Return color (may contain negative components useful for further adjustments)
    return color;
}

/* === Main Functions === */ 

vec3 Tonemapping(vec3 color, float exposure, float pWhite) // inputs are LINEAR
{
    // Ensure color values passed to tonemappers are positive.
    // They can be negative in the case of negative lights, which leads to undesired behavior.

    color *= exposure;

#if TONEMAPPER == TONEMAP_REINHARD
    color = TonemapReinhard(max(vec3(0.0), color), pWhite);
#elif TONEMAPPER == TONEMAP_FILMIC
    color = TonemapFilmic(max(vec3(0.0), color), pWhite);
#elif TONEMAPPER == TONEMAP_ACES
    color = TonemapACES(max(vec3(0.0), color), pWhite);
#elif TONEMAPPER == TONEMAP_AGX
    color = TonemapAgX(color);
#endif

    return color;
}

vec3 Adjustments(vec3 color, float brightness, float contrast, float saturation)
{
    color = mix(vec3(0.0), color, brightness);
    color = mix(vec3(0.5), color, contrast);
    color = mix(vec3(dot(vec3(1.0), color) * 0.33333), color, saturation);

    return color;
}

vec3 Debanding(vec3 color)
{
    float ditherStrength = 255.0; // lower is stronger
    return color + vec3((M_HashIGN(gl_FragCoord.xy) - 0.5) / ditherStrength);
}

vec3 LinearToSRGB(vec3 color)
{
	// color = clamp(color, vec3(0.0), vec3(1.0));
	// const vec3 a = vec3(0.055f);
	// return mix((vec3(1.0f) + a) * pow(color.rgb, vec3(1.0f / 2.4f)) - a, 12.92f * color.rgb, lessThan(color.rgb, vec3(0.0031308f)));
	// Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
	return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

/* === Main program === */

void main()
{
    vec3 color = texture(uTexColor, vTexCoord).rgb;

    color = Tonemapping(color, uTonemapExposure, uTonemapWhite);
    color = Adjustments(color, uBrightness, uContrast, uSaturation);
    color = Debanding(color);

    FragColor = vec4(LinearToSRGB(color), 1.0);
}
