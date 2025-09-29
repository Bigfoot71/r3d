/*
 * Copyright (c) 2025 Le Juez Victor
 *
 * This software is provided "as-is", without any express or implied warranty. In no event
 * will the authors be held liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose, including commercial
 * applications, and to alter it and redistribute it freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must not claim that you
 *   wrote the original software. If you use this software in a product, an acknowledgment
 *   in the product documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must not be misrepresented
 *   as being the original software.
 *
 *   3. This notice may not be removed or altered from any source distribution.
 */

#version 330 core

/* === Includes === */

#include "../include/math.glsl"

/* === Varyings === */

flat in vec3 vEmission;
in vec2 vTexCoord;
in vec3 vColor;
in mat3 vTBN;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmission;
uniform sampler2D uTexORM;

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
    FragAlbedo = vColor * texture(uTexAlbedo, vTexCoord).rgb;
    FragEmission = vEmission * texture(uTexEmission, vTexCoord).rgb;
    FragNormal = M_EncodeOctahedral(normalize(vTBN * M_NormalScale(texture(uTexNormal, vTexCoord).rgb * 2.0 - 1.0, uNormalScale)));

    vec3 orm = texture(uTexORM, vTexCoord).rgb;

    FragORM.r = uOcclusion * orm.x;
    FragORM.g = uRoughness * orm.y;
    FragORM.b = uMetalness * orm.z;
}
