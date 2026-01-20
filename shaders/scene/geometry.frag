/* geometry.frag -- Fragment shader used for rendering in G-buffers
 *
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Includes === */

#include "../include/blocks/view.glsl"
#include "../include/math.glsl"

/* === Varyings === */

smooth in vec2 vTexCoord;
flat   in vec3 vEmission;
smooth in vec4 vColor;
smooth in mat3 vTBN;

/* === Uniforms === */

uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uEmissionMap;
uniform sampler2D uOrmMap;

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
layout(location = 4) out vec2 FragGeomNormal;
layout(location = 5) out float FragDepth;

/* === Main function === */

void main()
{
    vec4 albedo = vColor * texture(uAlbedoMap, vTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

    vec3 emission = texture(uEmissionMap, vTexCoord).rgb;
    vec3 normal = texture(uNormalMap, vTexCoord).rgb;
    vec3 orm = texture(uOrmMap, vTexCoord).rgb;

    vec3 N = normalize(vTBN * M_NormalScale(normal * 2.0 - 1.0, uNormalScale));
    vec3 gN = vTBN[2];

    // Flip for back facing triangles with double sided meshes
    if (!gl_FrontFacing) N = -N, gN = -gN;

    FragAlbedo     = albedo.rgb;
    FragEmission   = vEmission * emission;
    FragNormal     = M_EncodeOctahedral(N);
    FragGeomNormal = M_EncodeOctahedral(gN);
    FragORM        = vec3(uOcclusion, uRoughness, uMetalness) * orm;
    FragDepth      = V_LinearizeDepth(gl_FragCoord.z);
}
