/* geometry.frag -- Fragment shader used for rendering in G-buffers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

flat in vec3 vEmission;
in vec2 vTexCoord;
in vec4 vColor;
in mat3 vTBN;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmission;
uniform sampler2D uTexORM;

uniform float uAlphaCutoff;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

/* === Fragments === */

layout(location = 0) out vec3 FragAlbedo;
layout(location = 1) out vec3 FragEmission;
layout(location = 2) out vec2 FragNormal;
layout(location = 3) out vec3 FragORM;

/* === Main function === */

void main()
{
    vec4 albedo = vColor * texture(uTexAlbedo, vTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

    vec3 N = normalize(vTBN * M_NormalScale(texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0, uNormalScale));
    if (!gl_FrontFacing) N = -N; // Flip for back facing triangles with double sided meshes

    FragAlbedo = albedo.rgb;
    FragEmission = vEmission * texture(uTexEmission, vTexCoord).rgb;
    FragNormal = M_EncodeOctahedral(N);

    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    FragORM.r = uOcclusion * orm.x;
    FragORM.g = uRoughness * orm.y;
    FragORM.b = uMetalness * orm.z;
}
