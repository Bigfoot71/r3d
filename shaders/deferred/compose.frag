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

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uDiffuseTex;
uniform sampler2D uSpecularTex;

/* === Fragments === */

layout(location = 0) out vec3 FragColor;

/* === Main function === */

void main()
{
    vec3 diffuse = texelFetch(uDiffuseTex, ivec2(gl_FragCoord.xy), 0).rgb;
    vec3 specular = texelFetch(uSpecularTex, ivec2(gl_FragCoord.xy), 0).rgb;

    FragColor = diffuse + specular;
}
