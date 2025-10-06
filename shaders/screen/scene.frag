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

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexAlbedo;
uniform sampler2D uTexEmission;

uniform sampler2D uTexDiffuse;
uniform sampler2D uTexSpecular;

/* === Fragments === */

layout(location = 0) out vec3 FragColor;

/* === Main function === */

void main()
{
    /* Sample textures */

    vec3 albedo = texture(uTexAlbedo, vTexCoord).rgb;
    vec3 emission = texture(uTexEmission, vTexCoord).rgb;

    vec3 diffuse = texture(uTexDiffuse, vTexCoord).rgb;
    vec3 specular = texture(uTexSpecular, vTexCoord).rgb;
	
    /* Combine all lighting contributions */

    FragColor = (albedo * diffuse) + specular + emission;
}
