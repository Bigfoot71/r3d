/* view.glsl -- Contains everything you need to manage transformations
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "../math.glsl"

#define V_PROJ_PERSP 0
#define V_PROJ_ORTHO 1

struct View {
    vec3 position;
    mat4 view;
    mat4 invView;
    mat4 proj;
    mat4 invProj;
    mat4 viewProj;
    int projMode;
    float aspect;
    float near;
    float far;
};

layout(std140) uniform ViewBlock {
    View uView;
};

vec3 V_GetViewPosition(vec2 texCoord, float linearDepth)
{
    vec2 ndc = texCoord * 2.0 - 1.0;

    if (uView.projMode == V_PROJ_ORTHO) {
        vec2 xyScale = vec2(1.0 / uView.proj[0][0], 1.0 / uView.proj[1][1]);
        return vec3(ndc * xyScale, -linearDepth);
    }

    float tanHalfFov = 1.0 / uView.proj[1][1];
    vec3 viewRay = vec3(ndc.x * tanHalfFov * uView.aspect, ndc.y * tanHalfFov, -1.0);

    return viewRay * linearDepth;
}

vec3 V_GetViewPosition(sampler2D texLinearDepth, vec2 texCoord)
{
    float linearDepth = texture(texLinearDepth, texCoord).r;
    return V_GetViewPosition(texCoord, linearDepth);
}

vec3 V_GetViewPosition(sampler2D texLinearDepth, ivec2 pixCoord)
{
    vec2 texCoord = (vec2(pixCoord) + 0.5) / vec2(textureSize(texLinearDepth, 0));
    float linearDepth = texelFetch(texLinearDepth, pixCoord, 0).r;
    return V_GetViewPosition(texCoord, linearDepth);
}

vec3 V_GetWorldPosition(vec3 viewPosition)
{
    return (uView.invView * vec4(viewPosition, 1.0)).xyz;
}

vec3 V_GetWorldPosition(vec2 texCoord, float linearDepth)
{
    vec3 viewPosition = V_GetViewPosition(texCoord, linearDepth);
    return V_GetWorldPosition(viewPosition);
}

vec3 V_GetWorldPosition(sampler2D texLinearDepth, vec2 texCoord)
{
    float linearDepth = texture(texLinearDepth, texCoord).r;
    return V_GetWorldPosition(texCoord, linearDepth);
}

vec3 V_GetWorldPosition(sampler2D texLinearDepth, ivec2 pixCoord)
{
    vec2 texCoord = (vec2(pixCoord) + 0.5) / vec2(textureSize(texLinearDepth, 0));
    float linearDepth = texelFetch(texLinearDepth, pixCoord, 0).r;
    return V_GetWorldPosition(texCoord, linearDepth);
}

vec3 V_GetViewNormal(vec3 worldNormal)
{
    return normalize(mat3(uView.view) * worldNormal);
}

vec3 V_GetViewNormal(vec2 encWorldNormal)
{
    vec3 worldNormal = M_DecodeOctahedral(encWorldNormal);
    return V_GetViewNormal(worldNormal);
}

vec3 V_GetViewNormal(sampler2D texNormal, vec2 texCoord)
{
    vec2 encWorldNormal = texture(texNormal, texCoord).rg;
    return V_GetViewNormal(encWorldNormal);
}

vec3 V_GetViewNormal(sampler2D texNormal, ivec2 pixCoord)
{
    vec2 encWorldNormal = texelFetch(texNormal, pixCoord, 0).rg;
    return V_GetViewNormal(encWorldNormal);
}

vec3 V_GetWorldNormal(sampler2D texNormal, vec2 texCoord)
{
    vec2 encWorldNormal = texture(texNormal, texCoord).rg;
    return M_DecodeOctahedral(encWorldNormal);
}

vec3 V_GetWorldNormal(sampler2D texNormal, ivec2 pixCoord)
{
    vec2 encWorldNormal = texelFetch(texNormal, pixCoord, 0).rg;
    return M_DecodeOctahedral(encWorldNormal);
}

vec2 V_ViewToScreen(vec3 viewPosition)
{
    vec4 clipPos = uView.proj * vec4(viewPosition, 1.0);
    return (clipPos.xy / clipPos.w) * 0.5 + 0.5;
}

vec2 V_WorldToScreen(vec3 worldPosition)
{
    vec4 projPos = uView.viewProj * vec4(worldPosition, 1.0);
    return (projPos.xy / projPos.w) * 0.5 + 0.5;
}

bool V_OffScreen(vec2 texCoord)
{
    return any(lessThan(texCoord, vec2(0.0))) ||
           any(greaterThan(texCoord, vec2(1.0)));
}

float V_LinearizeDepth(float depth)
{
    if (uView.projMode == V_PROJ_ORTHO) {
        return uView.near + depth * (uView.far - uView.near);
    }

    // Perspective
    float near = uView.near, far = uView.far;
    return (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));
}

float V_LinearizeDepth01(float depth)
{
    if (uView.projMode == V_PROJ_ORTHO) {
        return depth;
    }

    // Perspective
    float near = uView.near, far = uView.far;
    float z = (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));

    return (z - near) / (far - near);
}
