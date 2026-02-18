#version 330 core

#define SMAA_INCLUDE_VS 1
#define SMAA_INCLUDE_PS 0

#include "../include/smaa.glsl"

const vec2 positions[3] = vec2[]
(
    vec2(-1.0, -1.0),
    vec2( 3.0, -1.0),
    vec2(-1.0,  3.0)
);

noperspective out vec2 vTexCoord;
noperspective out vec2 vPixCoord;
noperspective out vec4 vOffset[3];

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 1.0, 1.0);
    vTexCoord = (gl_Position.xy * 0.5) + 0.5;

    SMAABlendingWeightCalculationVS(vTexCoord, vPixCoord, vOffset);
}
