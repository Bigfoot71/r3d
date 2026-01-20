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

#include "../include/math.glsl"
#include "../include/blocks/view.glsl"

/* === Varyings === */

smooth in mat4 vMatDecal;
smooth in vec3 vOrientation;
flat   in vec3 vEmission;
smooth in vec4 vColor;

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uEmissionMap;
uniform sampler2D uOrmMap;

uniform sampler2D uDepthTex;
uniform sampler2D uNormTanTex;

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

/* === Main function === */

void main()
{
    /* Transform view-space position to decal local space */
    vec3 positionViewSpace = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));
    vec4 positionObjectSpace = vMatDecal * vec4(positionViewSpace, 1.0);

    /* Discard fragments outside projector bounds */
    if (any(greaterThan(abs(positionObjectSpace.xyz), vec3(0.5)))) discard;

    /* Compute decal UVs */
    vec2 decalTexCoord = uTexCoordOffset + (positionObjectSpace.xz + 0.5) * uTexCoordScale;

    /* Sample albedo and apply alpha cutoff */
    vec4 albedo = vColor * texture(uAlbedoMap, decalTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

    /* Fetch surface normal and tangent */
    ivec2 screenCoord = ivec2(gl_FragCoord.xy);
    vec4 normTanData = texelFetch(uNormTanTex, screenCoord, 0);
    vec3 surfaceNormal = M_DecodeOctahedral(normTanData.rg);

    /* Normal threshold culling */
    float angle = acos(clamp(dot(vOrientation, surfaceNormal), -1.0, 1.0));
    float difference = uNormalThreshold - angle;
    if (difference < 0.0) discard;

    /* Compute fade factor */
    float fadeAlpha = clamp(difference / uFadeWidth, 0.0, 1.0) * albedo.a;

    /* Build TBN matrix */
    vec3 surfaceTangent = M_DecodeOctahedral(normTanData.ba);
    vec3 surfaceBitangent = normalize(cross(surfaceNormal, surfaceTangent));
    surfaceBitangent *= sign(dot(cross(surfaceTangent, surfaceBitangent), surfaceNormal));
    mat3 TBN = mat3(surfaceTangent, surfaceBitangent, surfaceNormal);

    /* Sample all other material textures together */
    vec3 normalSample = texture(uNormalMap, decalTexCoord).rgb * 2.0 - 1.0;
    vec3 emission = texture(uEmissionMap, decalTexCoord).rgb;
    vec3 orm = texture(uOrmMap, decalTexCoord).rgb;

    /* Transform and scale decal normal */
    vec3 N = normalize(TBN * M_NormalScale(normalSample, uNormalScale * fadeAlpha));

    /* Output */
    FragAlbedo   = vec4(albedo.rgb, fadeAlpha * float(uApplyColor));
    FragNormal   = vec4(M_EncodeOctahedral(N), 0.0, 1.0);
    FragEmission = vec4(vEmission * emission, fadeAlpha);
    FragORM      = vec4(orm * vec3(uOcclusion, uRoughness, uMetalness), fadeAlpha);
}
