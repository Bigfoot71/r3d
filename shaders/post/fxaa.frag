/* fxaa.frag -- Fragment shader for applying FXAA to the scene
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === FXAA Setup === */

#define FXAA_PC 1
#define FXAA_GLSL_130 1

#define FXAA_GREEN_AS_LUMA 1

#ifndef FXAA_FAST_PIXEL_OFFSET
#   define FXAA_FAST_PIXEL_OFFSET 0
#endif

#ifndef FXAA_GATHER4_ALPHA
#   define FXAA_GATHER4_ALPHA 0
#endif

#if QUALITY_PRESET == 0
#   define FXAA_QUALITY__PRESET      12
#   define FXAA_EDGE_THRESHOLD       (1.0/4.0)
#   define FXAA_EDGE_THRESHOLD_MIN   (1.0/12.0)
#   define FXAA_SUBPIX               0.50
#elif QUALITY_PRESET == 1
#   define FXAA_QUALITY__PRESET      15
#   define FXAA_EDGE_THRESHOLD       (1.0/8.0)
#   define FXAA_EDGE_THRESHOLD_MIN   (1.0/16.0)
#   define FXAA_SUBPIX               0.75
#elif QUALITY_PRESET == 2
#   define FXAA_QUALITY__PRESET      23
#   define FXAA_EDGE_THRESHOLD       (1.0/8.0)
#   define FXAA_EDGE_THRESHOLD_MIN   (1.0/24.0)
#   define FXAA_SUBPIX               0.90
#elif QUALITY_PRESET == 3
#   define FXAA_QUALITY__PRESET      29
#   define FXAA_EDGE_THRESHOLD       (1.0/12.0)
#   define FXAA_EDGE_THRESHOLD_MIN   (1.0/24.0)
#   define FXAA_SUBPIX               1.00
#endif

#include "../external/Fxaa3_11.h"

/* === Includes === */

#include "../include/blocks/frame.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSceneTex;

/* === Fragments === */

out vec4 FragColor;

/* === Main Function === */

void main()
{
    vec4 fxaaConsolePosPos = FxaaFloat4(
        vTexCoord - (0.5 * uFrame.texelSize),
        vTexCoord + (0.5 * uFrame.texelSize)
    );

    vec4 dummy = vec4(0.0);

    FragColor = FxaaPixelShader(
        vTexCoord,                  // pos
        fxaaConsolePosPos,          // fxaaConsolePosPos
        uSceneTex,                  // tex
        uSceneTex,                  // fxaaConsole360TexExpBiasNegOne
        uSceneTex,                  // fxaaConsole360TexExpBiasNegTwo
        uFrame.texelSize,           // fxaaQualityRcpFrame
        vec4(0.0),                  // fxaaConsoleRcpFrameOpt
        vec4(0.0),                  // fxaaConsoleRcpFrameOpt2
        vec4(0.0),                  // fxaaConsole360RcpFrameOpt2
        FXAA_SUBPIX,                // fxaaQualitySubpix
        FXAA_EDGE_THRESHOLD,        // fxaaQualityEdgeThreshold
        FXAA_EDGE_THRESHOLD_MIN,    // fxaaQualityEdgeThresholdMin
        0.0,                        // fxaaConsoleEdgeSharpness
        0.0,                        // fxaaConsoleEdgeThreshold
        0.0,                        // fxaaConsoleEdgeThresholdMin
        vec4(0.0)                   // fxaaConsole360ConstDir
    );
}
