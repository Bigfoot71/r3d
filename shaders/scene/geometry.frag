/* geometry.frag -- Fragment shader used for rendering in G-buffers
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
#include "../include/math.glsl"

/* === Varyings === */

smooth in vec2 vTexCoord;
flat   in vec3 vEmission;
smooth in vec4 vColor;
smooth in mat3 vTBN;

smooth in float vLinearDepth;

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

/* === User override === */

#include "../include/user/scene.frag"

/* === Main function === */

void main()
{
    SceneFragment(vTexCoord, vTBN, uAlphaCutoff);

    mat3 TBN = mat3(TANGENT, BITANGENT, NORMAL);
    vec3 N = normalize(TBN * M_NormalScale(NORMAL_MAP * 2.0 - 1.0, uNormalScale));
    vec3 gN = NORMAL;

    // Flip for back facing triangles with double sided meshes
    if (!gl_FrontFacing) N = -N, gN = -gN;

    FragAlbedo     = ALBEDO;
    FragEmission   = EMISSION;
    FragNormal     = M_EncodeOctahedral(N);
    FragGeomNormal = M_EncodeOctahedral(gN);
    FragORM        = vec3(OCCLUSION, ROUGHNESS, METALNESS);
    FragDepth      = vLinearDepth;
}
