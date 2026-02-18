#version 330 core

#define SMAA_INCLUDE_VS 0
#define SMAA_INCLUDE_PS 1

#include "../include/smaa.glsl"

uniform sampler2D uSceneTex;
uniform sampler2D uBlendTex;

noperspective in vec2 vTexCoord;
noperspective in vec4 vOffset;

out vec4 FragColor;

void main()
{
    FragColor = SMAANeighborhoodBlendingPS(vTexCoord, vOffset, uSceneTex, uBlendTex);
}
