/* decal.frag -- Fragment shader used for rendering decals into G-buffers
 *
 * Copyright (c) 2025 Michael Blaine
 * This file is derived from the work of Le Juez Victor
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * For conditions of distribution and use, see accompanying LICENSE file.
 */

#version 330 core

/* === Varyings === */

in mat4 vFinalMatModel;
flat in vec3 vEmission;
in vec4 vColor;
in vec4 vClipPos;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexNormal; // Unused - placeholder for future implementation
uniform sampler2D uTexEmission;
uniform sampler2D uTexORM;
uniform sampler2D uTexDepth;

uniform mat4 uMatInvView;
uniform mat4 uMatNormal; // Unused - placeholder for future implementation
uniform mat4 uMatVP;
uniform mat4 uMatInvProj;
uniform mat4 uMatProj;

uniform float uAlphaCutoff;
uniform float uNormalScale; // Unused - placeholder for future implementation
uniform float uOcclusion;
uniform float uRoughness;
uniform float uMetalness;

uniform vec2 uTexCoordOffset;
uniform vec2 uTexCoordScale;

/* === Fragments === */

layout(location = 0) out vec4 FragAlbedo;
layout(location = 1) out vec4 FragEmission;
layout(location = 2) out vec2 FragNormal; // Unused - normal output placeholder
layout(location = 3) out vec3 FragORM;

/* === Helper Functions === */

vec3 DepthToViewPosition(vec2 texCoord, float depth)
{
    vec4 ndcPos = vec4(texCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = uMatInvProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

/* === Main function === */

void main()
{
    // Normalize screen position to [0, 1]
    vec2 screenPos = vClipPos.xy / vClipPos.w;
    vec2 fragTexCoord = screenPos * 0.5 + 0.5;

    // Retrieve the depth from the depth texture
    float textureDepth = texture(uTexDepth, fragTexCoord).r;

    // Reconstruct position in view space
    vec3 positionViewSpace = DepthToViewPosition(fragTexCoord, textureDepth);

    // Convert from world space to decal projector's model space
    vec4 positionModelSpace = inverse(vFinalMatModel) * uMatInvView * vec4(positionViewSpace, 1.0);

    // Discard fragments that are outside the bounds of the decal projector
    if (abs(positionModelSpace.x) > 0.5 || 
        abs(positionModelSpace.y) > 0.5 || 
        abs(positionModelSpace.z) > 0.5) 
    {
        discard;
    }

	// Offset positional coordinates from [-0.5, 0.5] range to [0, 1] range for decal texture UV
    vec2 decalTexCoord = uTexCoordOffset + (positionModelSpace.xz + 0.5) * uTexCoordScale;

    // Sample albedo texture and discard if below alpha cutoff
    vec4 albedo = vColor * texture(uTexAlbedo, decalTexCoord);
    if (albedo.a < uAlphaCutoff) discard;

	// Apply material values
    FragAlbedo = albedo;	
    FragEmission = vec4(vEmission, 1.0) * texture(uTexEmission, decalTexCoord);

    vec3 orm = texture(uTexORM, decalTexCoord).xyz;
    FragORM.x = uOcclusion * orm.x;
    FragORM.y = uRoughness * orm.y;
    FragORM.z = uMetalness * orm.z;
}