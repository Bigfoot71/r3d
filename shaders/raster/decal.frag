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

/* === Varyings === */

in mat4 vFinalMatModel;
flat in vec3 vEmission;
in vec2 vTexCoord;
in vec4 vColor;
in mat3 vTBN;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal;
uniform sampler2D uTexEmission;
uniform sampler2D uTexORM;
uniform sampler2D uTexDepth;

uniform mat4 uMatInvView;
uniform mat4 uMatNormal;
uniform mat4 uMatVP;
uniform mat4 uMatInvProj;
uniform mat4 uMatProj;

uniform vec2 uViewportSize;

uniform float uAlphaCutoff;
uniform float uNormalScale;
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

/* === Fragments === */

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec2 FragNormal;
layout(location = 3) out vec3 FragORM;

/* === Helper Functions === */

vec3 ReconstructViewPosition(vec2 texCoord, float depth)
{
    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

/* === Main function === */

void main()
{
    // Get the screen position normalized to [0, 1]
    vec2 fragTexCoord = gl_FragCoord.xy / uViewportSize;

    // Retrieve the depth from the depth texture
    float textureDepth = texture(uTexDepth, fragTexCoord).r;

    // Calculate original view position from sampled depth
    vec3 scenePosView = ReconstructViewPosition(fragTexCoord, textureDepth);
	
    // Convert from world space to decal projector's model space
    vec4 objectPosition = inverse(vFinalMatModel) * uMatInvView * vec4(scenePosView, 1.0);
	
    // Perform bounds check.
    if (abs(objectPosition.x) > 0.5 || abs(objectPosition.y) > 0.5 || abs(objectPosition.z) > 0.5) {
        discard; // Anything outside of of our decal projector does not intersect.
    }

	// Offset positional coordinates from [-0.5, 0.5] range to [0, 1] range for decal texture UV
    vec2 decalTexCoord = objectPosition.xz + 0.5;
	
	// Apply decal material values
    vec4 albedo = vColor * texture(uTexAlbedo, decalTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

	FragAlbedo = albedo;	
	FragEmission = vec4(vEmission, 1.0) * texture(uTexEmission, decalTexCoord);

    vec3 orm = texture(uTexORM, decalTexCoord).xyz;
    FragORM.x = uOcclusion * orm.x;
    FragORM.y = uRoughness * orm.y;
    FragORM.z = uMetalness * orm.z;

}
