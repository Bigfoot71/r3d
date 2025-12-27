/* view.glsl -- Contains everything you need to manage transformations
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#include "../math.glsl"

struct View {
    vec3 position;
    mat4 view;
    mat4 invView;
    mat4 proj;
    mat4 invProj;
    mat4 viewProj;
    float aspect;
    float near;
    float far;
};

layout(std140) uniform ViewBlock {
    View uView;
};

vec3 V_GetViewPosition(float depth, vec2 texCoord)
{
    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uView.invProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

vec3 V_GetViewPosition(sampler2D texDepth, vec2 texCoord)
{
    float depth = texture(texDepth, texCoord).r;
    return V_GetViewPosition(depth, texCoord);
}

vec3 V_GetWorldPosition(vec3 viewPosition)
{
    return (uView.invView * vec4(viewPosition, 1.0)).xyz;
}

vec3 V_GetWorldPosition(float depth, vec2 texCoord)
{
    vec3 viewPosition = V_GetViewPosition(depth, texCoord);
    return V_GetWorldPosition(viewPosition);
}

vec3 V_GetWorldPosition(sampler2D texDepth, vec2 texCoord)
{
    float depth = texture(texDepth, texCoord).r;
    return V_GetWorldPosition(depth, texCoord);
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

vec3 V_GetWorldNormal(sampler2D texNormal, vec2 texCoord)
{
    vec2 encWorldNormal = texture(texNormal, texCoord).rg;
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
    float near = uView.near;
    float far = uView.far;

    return (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));
}

float V_LinearizeDepth01(float depth)
{
    float near = uView.near;
    float far = uView.far;

    float z = (2.0 * near * far) / (far + near - (depth * 2.0 - 1.0) * (far - near));
    return (z - near) / (far - near);
}

float V_GetLinearDepth(sampler2D texDepth, vec2 texCoord)
{
    float depth = texture(texDepth, texCoord).r;
    return V_LinearizeDepth(depth);
}

float V_GetLinearDepth01(sampler2D texDepth, vec2 texCoord)
{
    float depth = texture(texDepth, texCoord).r;
    return V_LinearizeDepth01(depth);
}
