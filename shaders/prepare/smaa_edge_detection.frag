#version 330 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "../include/smaa.glsl"

uniform sampler2D uSceneTex;

noperspective in vec2 vTexCoord;
noperspective in vec4 vOffset[3];

out vec2 FragColor;

void main()
{
    FragColor = SMAALumaEdgeDetectionPS(vTexCoord, vOffset, uSceneTex);
    // or SMAAColorEdgeDetectionPS / SMAADepthEdgeDetectionPS
}
