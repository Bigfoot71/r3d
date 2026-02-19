/* screen.frag -- Base of custom screen fragment shader
 *
 * Copyright (c) 2025-2026 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/frame.glsl"
#include "../include/blocks/view.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uSceneTex;
uniform sampler2D uNormalTex;
uniform sampler2D uDepthTex;

/* === Fragments === */

out vec4 FragColor;

/* === Built-In Input Variables === */

vec2 TEXCOORD   = vec2(0.0);
ivec2 PIXCOORD  = ivec2(0);

vec2 TEXEL_SIZE = vec2(0.0);
vec2 RESOLUTION = vec2(0.0);
float ASPECT = 0.0;

int FRAME_INDEX = 0;
float TIME = 0.0;

/* === Built-In Output Variables === */

vec3 COLOR = vec3(0.0);

/* === User Callable === */

vec3 FetchColor(ivec2 pixCoord)
{
    return texelFetch(uSceneTex, pixCoord, 0).rgb;
}

vec3 SampleColor(vec2 texCoord)
{
    return texture(uSceneTex, texCoord).rgb;
}

float FetchDepth(ivec2 pixCoord)
{
    return texelFetch(uDepthTex, pixCoord, 0).r;
}

float SampleDepth(vec2 texCoord)
{
    return texture(uDepthTex, texCoord).r;
}

float FetchDepth01(ivec2 pixCoord)
{
    float z = texelFetch(uDepthTex, pixCoord, 0).r;
    return clamp((z - uView.near) / (uView.far - uView.near), 0.0, 1.0);
}

float SampleDepth01(vec2 texCoord)
{
    float z = texture(uDepthTex, texCoord).r;
    return clamp((z - uView.near) / (uView.far - uView.near), 0.0, 1.0);
}

vec3 FetchPosition(ivec2 pixCoord)
{
    return V_GetViewPosition(uDepthTex, pixCoord);
}

vec3 SamplePosition(vec2 texCoord)
{
    return V_GetViewPosition(uDepthTex, texCoord);
}

vec3 FetchNormal(ivec2 pixCoord)
{
    return V_GetViewNormal(uNormalTex, pixCoord);
}

vec3 SampleNormal(vec2 texCoord)
{
    return V_GetViewNormal(uNormalTex, texCoord);
}

/* === Main function === */

#define fragment()

void main()
{
    TEXCOORD = vTexCoord;
    PIXCOORD = ivec2(gl_FragCoord.xy);

    TEXEL_SIZE = uFrame.texelSize;
    RESOLUTION = uFrame.screenSize;
    ASPECT = uView.aspect;

    FRAME_INDEX = uFrame.index;
    TIME = uFrame.time;

    fragment();

    FragColor = vec4(COLOR, 1.0);
}
