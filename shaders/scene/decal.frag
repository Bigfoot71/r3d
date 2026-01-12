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
smooth in vec4 vClipPos;
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

/* === Fragments === */

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec3 FragORM;
layout(location = 3) out vec4 FragNormal;

/* === Main function === */

void main()
{
    /* Normalize screen position to [0, 1] */
    vec2 screenPos = vClipPos.xy / vClipPos.w;
    vec2 fragTexCoord = screenPos * 0.5 + 0.5;

    /* Get surface position in view space */
    vec3 positionViewSpace = V_GetViewPosition(uDepthTex, fragTexCoord);

    /* Get position in projector's local space */
    vec4 positionObjectSpace = vMatDecal * vec4(positionViewSpace, 1.0);

    /* Discard fragments that are outside the bounds of the projector */
    if (abs(positionObjectSpace.x) > 0.5 || 
        abs(positionObjectSpace.y) > 0.5 || 
        abs(positionObjectSpace.z) > 0.5) discard;

	/* Offset coordinates to [0, 1] range for decal texture UV */
    vec2 decalTexCoord = uTexCoordOffset + (positionObjectSpace.xz + 0.5) * uTexCoordScale;

    /* Compute albedo and discard if below alpha cutoff */
    vec4 albedo = vColor * texture(uAlbedoMap, decalTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

    /* Retrieve surface normal */
    vec3 surfaceNormal = M_DecodeOctahedral(texture(uNormTanTex, fragTexCoord).rg);

    /* Compute angular difference between the decal and surface normal */
    float angle = acos(clamp(dot(vOrientation, surfaceNormal), -1.0, 1.0));
    float difference = uNormalThreshold - angle;

    /* Discard if the angle exceeds the threshold */
    if (difference < 0.0) discard;

    /* Fade on edge of threshold */
    // note: could do another (or the main) uAlphaCutoff check here for discard
    float fadeAlpha = clamp(difference / uFadeWidth, 0.0, 1.0);
    albedo.a *= fadeAlpha;

    /* Retrieve surface tangent */
    vec3 surfaceTangent = M_DecodeOctahedral(texture(uNormTanTex, fragTexCoord).ba);

    /* Compute bitangent and correct handedness if necessary */
    vec3 surfaceBitangent = normalize(cross(surfaceNormal, surfaceTangent));

    if (dot(cross(surfaceTangent, surfaceBitangent), normalize(surfaceNormal)) <= 0.0) {
        surfaceBitangent = -surfaceBitangent;
    }

    /* Apply normal */
    mat3 TBN = mat3(surfaceTangent, surfaceBitangent, surfaceNormal);
    vec3 N = normalize(TBN * M_NormalScale(texture(uNormalMap, decalTexCoord).rgb * 2.0 - 1.0, uNormalScale * fadeAlpha));
    FragNormal = vec4(M_EncodeOctahedral(N), 0.0, 1.0);

	/* Apply material */
    FragAlbedo = albedo;
    FragEmission = vec4(vEmission * texture(uEmissionMap, decalTexCoord).rgb, fadeAlpha);

    vec3 orm = texture(uOrmMap, decalTexCoord).rgb;

    FragORM.x = uOcclusion * orm.x;
    FragORM.y = uRoughness * orm.y;
    FragORM.z = uMetalness * orm.z;
}