/* compose.frag -- Deferred pipeline composition fragment shader
 *
 * Composes the final scene by combining the outputs from
 * the deferred rendering pipeline.
 *
 * Copyright (c) 2025 Victor Le Juez
 *
 * This software is distributed under the terms of the accompanying LICENSE file.
 * It is provided "as-is", without any express or implied warranty.
 */

#version 330 core

/* === Includes === */

#include "../include/pbr.glsl"

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uAlbedoTex;
uniform sampler2D uDiffuseTex;
uniform sampler2D uSpecularTex;
uniform sampler2D uOrmTex;
uniform sampler2D uSsrTex;

uniform float uSsrNumLevels;

/* === Fragments === */

layout(location = 0) out vec3 FragColor;

/* === Main function === */

void main()
{
    vec3 albedo = texelFetch(uAlbedoTex, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 diffuse = texelFetch(uDiffuseTex, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 specular = texelFetch(uSpecularTex, ivec2(gl_FragCoord.xy), 0).rgb;

    vec3 orm = texelFetch(uOrmTex, ivec2(gl_FragCoord).xy, 0).rgb;
    vec4 ssr = textureLod(uSsrTex, vTexCoord, orm.y * uSsrNumLevels);

    vec3 F0 = PBR_ComputeF0(orm.z, 0.5, albedo);
    vec3 kS_approx = F0 * (1.0 - orm.y * 0.5);

    FragColor = diffuse + mix(specular, kS_approx * ssr.rgb, ssr.a);
}
