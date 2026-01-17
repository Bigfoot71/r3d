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

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

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
uniform sampler2D uGeomNormal;

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

/* === Helper functions === */

void GetTangent(vec2 decalTexCoord, vec3 viewPosition, vec3 surfaceNormal,
                out vec3 surfaceTangent, out vec3 surfaceBitangent)
{
    vec2 dUVdx = dFdx(decalTexCoord);
    vec2 dUVdy = dFdy(decalTexCoord);
    vec3 dPdx = dFdx(viewPosition);
    vec3 dPdy = dFdy(viewPosition);

    float uvDet = dUVdx.x * dUVdy.y - dUVdx.y * dUVdy.x;
    vec3 T = dPdx * dUVdy.y - dPdy * dUVdx.y;
    vec3 B = dPdy * dUVdx.x - dPdx * dUVdx.y;

    surfaceTangent = normalize(T - surfaceNormal * dot(surfaceNormal, T));
    surfaceBitangent = cross(surfaceNormal, surfaceTangent) * sign(uvDet);
}

/* === Main function === */

void main()
{
    /* Get surface position in view space */
    vec3 positionViewSpace = V_GetViewPosition(uDepthTex, ivec2(gl_FragCoord.xy));

    /* Get position in projector's local space */
    vec4 positionObjectSpace = vMatDecal * vec4(positionViewSpace, 1.0);

    /* Discard fragments that are outside the bounds of the projector */
    if (any(greaterThan(abs(positionObjectSpace.xyz), vec3(0.5)))) discard;

	/* Offset coordinates to [0, 1] range for decal texture UV */
    vec2 decalTexCoord = uTexCoordOffset + (positionObjectSpace.xz + 0.5) * uTexCoordScale;

    /* Compute albedo and discard if below alpha cutoff */
    vec4 albedo = vColor * texture(uAlbedoMap, decalTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

    /* Retrieve surface normal */
    vec3 surfaceNormal = V_GetWorldNormal(uGeomNormal, ivec2(gl_FragCoord.xy));

    /* Compute angular difference between the decal and surface normal */
    float angle = acos(clamp(dot(vOrientation, surfaceNormal), -1.0, 1.0));
    float difference = uNormalThreshold - angle;

    /* Discard if the angle exceeds the threshold */
    if (difference < 0.0) discard;

    /* Fade on edge of threshold */
    // note: could do another (or the main) uAlphaCutoff check here for discard
    float fadeAlpha = clamp(difference / uFadeWidth, 0.0, 1.0);
    albedo.a *= fadeAlpha;

    /* Retrieve surface tangent and bitangent */
    vec3 surfaceTangent, surfaceBitangent;
    GetTangent(decalTexCoord, positionViewSpace, surfaceNormal, surfaceTangent, surfaceBitangent);

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
