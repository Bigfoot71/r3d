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

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexEmission;
uniform sampler2D uTexDiffuse;
uniform sampler2D uTexSpecular;
uniform sampler2D uTexSSAO;

/* === Fragments === */

layout(location = 0) out vec3 FragColor;

/* === Main function === */

void main()
{
    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 emission = texture(uTexEmission, vTexCoord).rgb;

    vec3 diffuse = texture(uTexDiffuse, vTexCoord).rgb;
    vec3 specular = texture(uTexSpecular, vTexCoord).rgb;

    FragColor = (albedo * diffuse) + specular + emission;
}
