/* decal.frag -- Fragment shader used for rendering decals into G-buffers
 *
 * Copyright (c) 2025 Michael Blaine
 * This file is derived from the work of Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/frame.glsl"
#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

smooth in vec3 vPosition;           //< For custom shaders
smooth in mat4 vDecalProjection;
smooth in mat3 vDecalAxes;
flat   in vec3 vEmission;
smooth in vec4 vColor;

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uEmissionMap;
uniform sampler2D uOrmMap;

uniform sampler2D uDepthTex;
uniform sampler2D uGeomNormalTex;

uniform float uAlphaCutoff;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

uniform float uNormalThreshold;
uniform float uFadeWidth;
uniform bool uApplyColor;

/* === Fragments === */

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec4 FragORM;
layout(location = 3) out vec4 FragNormal;

/* === Helper functions === */

mat3 BuildDecalTBN(vec3 worldNormal)
{
    vec3 decalX = normalize(vDecalAxes[0]);
    vec3 decalZ = normalize(vDecalAxes[2]);

    vec3 T = normalize(decalX - dot(decalX, worldNormal) * worldNormal);
    vec3 B = normalize(decalZ - dot(decalZ, worldNormal) * worldNormal);

    B *= sign(dot(cross(T, B), worldNormal));

    return mat3(T, B, worldNormal);
}

/* === User override === */

#include "../include/user/scene.frag"

/* === Main function === */

void main()
{
    /* Transform view-space position to decal local space */
    vec3 positionViewSpace = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec4 positionObjectSpace = vDecalProjection * vec4(positionViewSpace, 1.0);

    /* Discard fragments outside projector bounds */
    if (any(greaterThan(abs(positionObjectSpace.xyz), vec3(0.5)))) discard;

    /* Compute decal UVs in [0, 1] range */
    vec2 decalTexCoord = uTexCoordOffset + (positionObjectSpace.xz + 0.5) * uTexCoordScale;

    /* Fetch surface normal and build TBN */
    vec2 encGeomNormal = texelFetch(uGeomNormalTex, ivec2(gl_FragCoord.xy), 0).rg;
    vec3 worldNormal = M_DecodeOctahedral(encGeomNormal);
    mat3 TBN = BuildDecalTBN(worldNormal);

    /* Normal threshold culling */
    float angle = acos(clamp(dot(vDecalAxes[1], worldNormal), -1.0, 1.0));
    float difference = uNormalThreshold - angle;
    if (difference < 0.0) discard;

    /* Sample material maps with alpha cutoff */
    SceneFragment(decalTexCoord, TBN, uAlphaCutoff);

    /* Compute fade factor */
    float fadeAlpha = clamp(difference / uFadeWidth, 0.0, 1.0) * ALPHA;

    /* Transform and scale decal normal */
    TBN = mat3(TANGENT, BITANGENT, NORMAL);
    vec3 N = normalize(TBN * M_NormalScale(NORMAL_MAP * 2.0 - 1.0, uNormalScale * fadeAlpha));

    /* Output */
    FragAlbedo   = vec4(ALBEDO, fadeAlpha * float(uApplyColor));
    FragNormal   = vec4(M_EncodeOctahedral(N), 0.0, 1.0);
    FragEmission = vec4(EMISSION, fadeAlpha);
    FragORM      = vec4(vec3(OCCLUSION, ROUGHNESS, METALNESS), fadeAlpha);
}
